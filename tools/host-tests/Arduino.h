#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
using std::min;
using std::max;
class String:public std::string {
 using const_iterator=std::string::const_iterator;
public:
 String()=default;
 String(const char* s):std::string(s?s:""){}
 String(char ch):std::string(1,ch){}
 String(const std::string& s):std::string(s){}
 String(std::string&& s):std::string(std::move(s)){}
 template<class T,typename std::enable_if<std::is_integral<T>::value&&!std::is_same<T,char>::value,int>::type=0>
 String(T value):std::string(std::to_string(value)){}
 using std::string::operator=;
 String& operator=(const char* s){std::string::operator=(s?s:"");return *this;}
 String substring(size_t begin,size_t end=std::string::npos)const{begin=min(begin,size());end=min(end,size());return substr(begin,end>begin?end-begin:0);}
 bool startsWith(const String& s)const{return compare(0,s.size(),s)==0;}
 bool endsWith(const String& s)const{return size()>=s.size()&&compare(size()-s.size(),s.size(),s)==0;}
 int indexOf(const String& s,size_t at=0)const{auto pos=find(s,at);return pos==npos?-1:int(pos);}
 int indexOf(char ch,size_t at=0)const{auto pos=find(ch,at);return pos==npos?-1:int(pos);}
 int lastIndexOf(const String& s)const{auto pos=rfind(s);return pos==npos?-1:int(pos);}
 int lastIndexOf(char ch)const{auto pos=rfind(ch);return pos==npos?-1:int(pos);}
 void remove(size_t at,size_t n=npos){if(at<size())erase(at,n);}
 void trim(){auto first=find_first_not_of(" \t\r\n");if(first==npos){clear();return;}erase(0,first);erase(find_last_not_of(" \t\r\n")+1);}
 void toLowerCase(){for(char& c:*this)if(c>='A'&&c<='Z')c=char(c+32);}
 bool concat(const char* s){append(s);return true;}
};
class Print {
public:
 virtual ~Print()=default;
 virtual size_t write(uint8_t)=0;
 virtual size_t write(const uint8_t* data,size_t n){for(size_t i=0;i<n;++i)write(data[i]);return n;}
};
class Stream:public Print {
public:
 virtual int read()=0;
 virtual int peek()=0;
 virtual int available()=0;
 virtual size_t readBytes(char* p,size_t n){size_t i=0;for(;i<n;++i){int c=read();if(c<0)break;p[i]=char(c);}return i;}
 virtual size_t readBytes(uint8_t* p,size_t n){return readBytes((char*)p,n);}
 virtual void flush()=0;
};
