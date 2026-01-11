#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdlib>
#include <cstdio>

// 极简 JSON 解析/构造，仅支持对象、数组、数字、布尔和字符串，足够本服务使用
// 生产用途建议改为 nlohmann/json
namespace MiniJson {
	struct Value {
		enum Type { Null, Bool, Number, String, Array, Object } type = Null;
		bool b = false;
		double n = 0.0;
		std::string s;
		std::vector<Value> a;
		std::unordered_map<std::string, Value> o;

		static Value object() { Value v; v.type = Object; return v; }
		static Value array() { Value v; v.type = Array; return v; }
		static Value string(const std::string &x) { Value v; v.type = String; v.s = x; return v; }
		static Value number(double x) { Value v; v.type = Number; v.n = x; return v; }
		static Value boolean(bool x) { Value v; v.type = Bool; v.b = x; return v; }
	};

	struct Parser {
		const char* p; const char* end;
		Parser(const std::string &in) : p(in.c_str()), end(in.c_str()+in.size()) {}
		void ws(){ while(p<end && (*p==' '||*p=='\t'||*p=='\r'||*p=='\n')) ++p; }
		bool match(char c){ ws(); if(p<end && *p==c){ ++p; return true;} return false; }
		bool expect(char c){ ws(); if(p<end && *p==c){ ++p; return true;} return false; }
		std::string parseString(){ std::string out; if(!expect('"')) return out; while(p<end){ char c=*p++; if(c=='"') break; if(c=='\\' && p<end){ char e=*p++; if(e=='"') out.push_back('"'); else if(e=='\\') out.push_back('\\'); else if(e=='n') out.push_back('\n'); else if(e=='t') out.push_back('\t'); else out.push_back(e);} else out.push_back(c);} return out; }
		double parseNumber(){ ws(); char* q=nullptr; double x=strtod(p,&q); p=q; return x; }
		Value parseValue(){ ws(); if(p>=end) return {}; char c=*p; if(c=='{') return parseObject(); if(c=='[') return parseArray(); if(c=='"'){ Value v; v.type=Value::String; v.s=parseString(); return v;} if(c=='t'){ p+=4; return Value::boolean(true);} if(c=='f'){ p+=5; return Value::boolean(false);} if(c=='n'){ p+=4; return {}; } Value v; v.type=Value::Number; v.n=parseNumber(); return v; }
		Value parseObject(){ Value v=Value::object(); expect('{'); ws(); if(match('}')) return v; while(p<end){ std::string k=parseString(); expect(':'); v.o[k]=parseValue(); if(match('}')) break; expect(','); }
			return v; }
		Value parseArray(){ Value v=Value::array(); expect('['); ws(); if(match(']')) return v; while(p<end){ v.a.push_back(parseValue()); if(match(']')) break; expect(','); } return v; }
	};

	inline bool parse(const std::string &in, Value &out){ Parser ps(in); out=ps.parseValue(); return true; }

	inline void dumpString(std::string &out, const std::string &s){ out.push_back('"'); for(char c: s){ if(c=='"'||c=='\\'){ out.push_back('\\'); out.push_back(c);} else if(c=='\n'){ out+="\\n"; } else out.push_back(c);} out.push_back('"'); }
	inline void dumpValue(std::string &out, const Value &v){
		switch(v.type){
			case Value::Null: out+="null"; break;
			case Value::Bool: out+=(v.b?"true":"false"); break;
			case Value::Number: { char buf[64]; snprintf(buf, sizeof(buf), "%g", v.n); out+=buf; } break;
			case Value::String: dumpString(out, v.s); break;
			case Value::Array: { out.push_back('['); for(size_t i=0;i<v.a.size();++i){ if(i) out.push_back(','); dumpValue(out,v.a[i]); } out.push_back(']'); } break;
			case Value::Object: { out.push_back('{'); size_t i=0; for(auto &kv: v.o){ if(i++) out.push_back(','); dumpString(out, kv.first); out.push_back(':'); dumpValue(out, kv.second);} out.push_back('}'); } break;
		}
	}
	inline std::string stringify(const Value &v){ std::string out; dumpValue(out, v); return out; }
}
