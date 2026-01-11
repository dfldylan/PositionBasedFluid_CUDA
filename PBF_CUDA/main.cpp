#include <string>
#include <iostream>
#include <thread>
#include <chrono>

// 服务相关
#include "Service/SocketServer.h"
#include "Service/FluidService.h"

int main(int argc, char *argv[])
{
	// 简单服务模式：TCP 监听，按行分隔 JSON 消息
	FluidService service;
	SocketServer server;
	bool ok = server.start("0.0.0.0", 7777, [&](const std::string &msg){
		return service.handle(msg);
	});
	if(!ok){
		std::cerr << "Failed to start server on port 7777\n";
		return 1;
	}
	std::cout << "Fluid service started at 0.0.0.0:7777\n";
	std::cout << "Protocol: one JSON per line. Field 'type' selects operation.\n";

	// 阻塞主线程直到 Ctrl+C 终止（简单做法：睡眠循环）
	while(true){
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	return 0;
}