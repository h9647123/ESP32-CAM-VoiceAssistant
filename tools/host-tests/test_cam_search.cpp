#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#define ARDUINOJSON_ENABLE_ARDUINO_PRINT 0
#define ARDUINOJSON_ENABLE_PROGMEM 0
#include "Arduino.h"
#include <ArduinoJson.h>
#include <cassert>
#include <vector>
#include <iostream>
static uint32_t clockMs=0;
static uint32_t millis(){return clockMs;}
static void delay(uint32_t n){clockMs+=n;}
static void yield(){}
#include "cam_search_body.h"
#include "cam_rich_state.h"
#include "cam_photo_order.h"
struct Config{uint8_t searchProvider=1;bool searchFetchEnabled=false;String tavilyApiKey="fixture-tavily",deepseekSearchKey="fixture-deepseek",customSearchKey="fixture-custom",customSearchUrl="https://gateway.example/v1/tools/search",searchApiUrl;}cfg;
static String lastError,camLastSearchSources;
static constexpr size_t LLM_SEARCH_CONTEXT_MAX_BYTES=2400;
static constexpr unsigned TAVILY_HTTP_TIMEOUT_MS=15000,TAVILY_HTTP_ATTEMPTS=2;
static String uiText(const char*,const char* en,const char*){return en;}
static String truncateUtf8ToBytes(const String& x,size_t n){n=min(n,x.length());while(n<x.length()&&n&&(uint8_t(x[n])&0xc0)==0x80)--n;return x.substring(0,n);}
static String compactSearchSnippet(const String& x,size_t n){return truncateUtf8ToBytes(x,n);}
static String applyLanPinToUrl(const String& x){return x;}
static bool connectWifi(){return true;}
struct Log{template<class...T>void printf(const char*,T...){}void println(const String&){} }Serial;
struct WiFiClient{};
struct WiFiClientSecure{void setHandshakeTimeout(unsigned){}};
static bool camConfigureTls(WiFiClientSecure&){return true;}
struct Reply{int status;String body;bool chunked=false,slow=false;};
static std::vector<Reply> replies;
static std::vector<String> urls,bodies,auth;
class HTTPClient{
 Reply reply;
public:
 void setTimeout(unsigned){}void setConnectTimeout(unsigned){}void setUserAgent(const char*){}
 template<class T>bool begin(T&,const String& url){urls.push_back(url);return true;}
 void addHeader(const String& name,const String& value){if(name=="Authorization")auth.push_back(value);}
 int POST(const String& body){bodies.push_back(body);assert(!replies.empty());reply=replies.front();replies.erase(replies.begin());return reply.status;}
 int getSize(){return reply.chunked?-1:int(reply.body.length());}
 int writeToStream(Stream* sink){
  if(reply.slow)clockMs+=16000;
  for(size_t at=0;at<reply.body.length();){size_t n=min<size_t>(37,reply.body.length()-at);if(sink->write((const uint8_t*)reply.body.data()+at,n)!=n)return -1;at+=n;}
  return reply.body.length();
 }
 void end(){}
};
static bool buildTavilyDigestFromJson(const String&,const String&,String&){return false;}
#include "search_under_test.h"
static void reset(std::initializer_list<Reply> cases){replies=cases;urls.clear();bodies.clear();auth.clear();clockMs=0;cfg.searchProvider=1;cfg.searchFetchEnabled=false;lastError="";}
int main(){
 String out;
 const String good=R"({"results":[{"title":"News","url":"https://example.com/a","summary":"Verified summary"},{"title":"Only snippet","url":"https://example.com/b","snippet":"Fallback snippet"}]})";
 {JsonDocument request;camBuildSearchRequest(request,String(std::string(200,'a')),1);assert(request["query"].as<String>().length()==128);assert(request["count"]==3&&!request["need_content"].as<bool>());assert(request["search_depth"].isNull());}
 {JsonDocument request;camBuildSearchRequest(request,"query",0);assert(request["max_results"]==3&&request["search_depth"]=="basic"&&request["count"].isNull());}
 reset({{200,good,true}});assert(performTavilySearch("news",out,"original question"));assert(out.find("Verified summary")!=String::npos&&out.find("https://example.com/a")!=String::npos);assert(auth[0]=="Bearer fixture-deepseek");assert(urls[0]=="https://api.deepseek.com/v1/tools/search");
 {JsonDocument request;assert(!deserializeJson(request,bodies[0]));assert(request["ori_question"]=="original question");}
 reset({{200,good}});cfg.searchProvider=2;assert(performTavilySearch("news",out));assert(urls[0]==cfg.customSearchUrl&&auth[0]=="Bearer fixture-custom");
 for(int status:{401,402,403}){reset({{status,"{}"}});assert(!performTavilySearch("news",out));assert(bodies.size()==1);}
 for(int status:{429,500,504,-1}){reset({{status,"{}"},{status,"{}"}});assert(!performTavilySearch("news",out));assert(bodies.size()==2);}
 for(const char* body:{"{broken",R"({"results":[]})",R"({"results":[{"url":"https://example.com"}]})"}){reset({{200,body},{200,body}});assert(!performTavilySearch("news",out));}
 reset({{200,R"({"results":[{"title":"Missing fields","snippet":"Useful text"}]})"}});assert(performTavilySearch("news",out));
 for(bool chunked:{false,true}){reset({{200,String(std::string(12289,'x')),chunked}});assert(!performTavilySearch("news",out));assert(bodies.size()==1);}
 reset({{200,good,true,true}});assert(!performTavilySearch("news",out));
 reset({{200,good},{200,R"({"content":"Fetched page excerpt"})"}});cfg.searchFetchEnabled=true;assert(performTavilySearch("news",out));assert(bodies.size()==2&&urls[1]=="https://api.deepseek.com/v1/tools/fetch"&&out.find("Fetched page excerpt")!=String::npos);
 reset({{200,good},{403,"{}"}});cfg.searchFetchEnabled=true;assert(performTavilySearch("news",out));assert(out.find("Verified summary")!=String::npos&&bodies.size()==2);
 reset({{200,good},{200,String(std::string(12289,'x')),true}});cfg.searchFetchEnabled=true;assert(performTavilySearch("news",out));assert(out.find("Verified summary")!=String::npos);
 {CamSearchBody sink;const String full(std::string(CAM_SEARCH_BODY_LIMIT,'a'));assert(sink.write((const uint8_t*)full.data(),full.length())==full.length());assert(sink.write(uint8_t('b'))==0&&sink.rejected);}
 {const String text=String(char(3))+"thinking\n\n"+String(char(3))+"answer";assert(camThinkingAt(text.c_str(),4));assert(!camThinkingAt(text.c_str(),text.length()));assert(camThinkingAt("\3\n",2));assert(!camThinkingAt("\5\3",2));}
 assert(camParsePhotoKey("P-V-20261101225001.jpg").epoch>0);
 assert(camParsePhotoKey("V-20261101225001.avi").epoch>0);
 assert(!camParsePhotoKey("P-V-20000101000001.jpg").epoch);
 assert(!camParsePhotoKey("P-V-20261301225001.jpg").epoch);
 assert(camParsePhotoKey("P0000000000000002_T1793400000.jpg").order==2);
 std::cout<<"search_mock_rich_state_and_media_names_passed\n";
}
