#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <functional>
#include <chrono>

#pragma comment(lib, "Ws2_32.lib")

class SocketServer {
public:
	using MessageHandler = std::function<std::string(const std::string&)>; // in -> out

	SocketServer() = default;
	~SocketServer(){ stop(); }

	bool start(const char* host, uint16_t port, MessageHandler handler){
		_handler = std::move(handler);
		WSADATA wsaData;
		int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
		if(iResult != 0) return false;

		addrinfo hints{}; hints.ai_family=AF_INET; hints.ai_socktype=SOCK_STREAM; hints.ai_protocol=IPPROTO_TCP; hints.ai_flags=AI_PASSIVE;
		addrinfo* result=nullptr; char portStr[16]; sprintf_s(portStr, "%hu", port);
		if(getaddrinfo(host, portStr, &hints, &result)!=0){ WSACleanup(); return false; }

		_listener = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if(_listener==INVALID_SOCKET){ freeaddrinfo(result); WSACleanup(); return false; }
		u_long nonblock = 1; ioctlsocket(_listener, FIONBIO, &nonblock);
		if(bind(_listener, result->ai_addr, (int)result->ai_addrlen)==SOCKET_ERROR){ freeaddrinfo(result); closesocket(_listener); WSACleanup(); return false; }
		freeaddrinfo(result);
		if(listen(_listener, SOMAXCONN)==SOCKET_ERROR){ closesocket(_listener); WSACleanup(); return false; }

		_running = true;
		_loop = std::thread([this]{ acceptLoop(); });
		return true;
	}

	void stop(){
		_running = false;
		if(_listener!=INVALID_SOCKET){ closesocket(_listener); _listener=INVALID_SOCKET; }
		if(_loop.joinable()) _loop.join();
		WSACleanup();
	}

private:
	void acceptLoop(){
		while(_running){
			SOCKET client = accept(_listener, nullptr, nullptr);
			if(client==INVALID_SOCKET){ std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
			std::thread(&SocketServer::clientLoop, this, client).detach();
		}
	}
	void clientLoop(SOCKET client){
		char buf[1<<16];
		std::string incoming;
		for(;;){
			int n = recv(client, buf, sizeof(buf), 0);
			if(n<=0) break;
			incoming.append(buf, buf+n);
			// 简单分包：按 \n 作为一条 JSON 消息分隔
			size_t pos;
			while((pos=incoming.find('\n'))!=std::string::npos){
				std::string one = incoming.substr(0,pos);
				incoming.erase(0,pos+1);
				if(_handler){ std::string resp = _handler(one); resp.push_back('\n'); send(client, resp.c_str(), (int)resp.size(), 0);} 
			}
		}
		closesocket(client);
	}

	SOCKET _listener = INVALID_SOCKET;
	std::thread _loop;
	bool _running = false;
	MessageHandler _handler;
};
