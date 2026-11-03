#pragma once
#include "internal/log.h"
#include <sstream>
namespace tefkernel::buttons {
inline void format_log(std::ostringstream& out,const char* fmt){out<<fmt;}
template<class T,class... Rest>void format_log(std::ostringstream& out,const char* fmt,const T& value,const Rest&... rest){
 const char* p=fmt;while(*p && !(p[0]=='{'&&p[1]=='}'))++p;
 out.write(fmt,p-fmt);if(!*p)return;out<<value;format_log(out,p+2,rest...);
}
template<class... Args>std::string log_message(const char* fmt,const Args&... args){std::ostringstream out;format_log(out,fmt,args...);return out.str();}
}
#define LOG_INFO(...) TEKLOG_INFO("%s",tefkernel::buttons::log_message(__VA_ARGS__).c_str())
#define LOG_WARN(...) TEKLOG_WARN("%s",tefkernel::buttons::log_message(__VA_ARGS__).c_str())
#define LOG_ERROR(...) TEKLOG_ERROR("%s",tefkernel::buttons::log_message(__VA_ARGS__).c_str())
