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

struct Config{uint8_t searchProvider=1;bool searchFetchEnabled=false;String llmModelId="fixture-model",llmApiUrl="https://llm.example/chat/completions",llmApiKey="fixture-llm";String tavilyApiKey="fixture-tavily",deepseekSearchKey="fixture-deepseek",customSearchKey="fixture-custom",customSearchUrl="https://gateway.example/v1/tools/search",searchApiUrl;}cfg;
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

#include <cstring>
static bool shouldAutoSearchSafe(const String&){return false;}
static String llmTimeContext(){return "Current date: 2026-11-02";}
static String extractLlmContentFromJson(const String& response){JsonDocument d;if(deserializeJson(d,response))return "";return d["choices"][0]["message"]["content"].as<String>();}
struct ButtonEvent{unsigned generation,key,flags;};
static void* buttonEvents=nullptr;
struct Gen{unsigned value=0;unsigned load(){return value;}void operator++(){++value;}}buttonUiGeneration;
static constexpr int pdTRUE=1;
static bool pendingCancel=false;
static int xQueueReceive(void*,ButtonEvent* event,int){if(!pendingCancel)return 0;pendingCancel=false;*event={buttonUiGeneration.load(),2,1};return pdTRUE;}
static constexpr char RICH_THINKING_TOGGLE=3;
enum class AppState{Thinking};enum class UiPage{Page1};
static UiPage uiPage=UiPage::Page1;static bool screenSleeping=false,chatAutoScrollBottom=false;
static String cachedDateText="2026-11-02",cachedTimeText="02:00:00";
static std::vector<String> frames;
static void setState(AppState,const String&){}
static void updateChatMessageContent(size_t,const String& text){frames.push_back(text);}
static void drawChatWindow(bool){}static void drawPage1Footer(const String&){}static void cancelServiceProbe(){}
#include "web_helpers_under_test.h"
#include "cam_web_research.h"
static Reply llm(const String& content){JsonDocument d;d["choices"][0]["message"]["content"]=content;String s;serializeJson(d,s);return {200,s};}
static void resetWeb(){replies.clear();urls.clear();bodies.clear();auth.clear();frames.clear();clockMs=0;cfg.searchProvider=2;cfg.searchFetchEnabled=true;}
int main(){
 {String digest;assert(buildTavilyDigestFromJson(R"({"results":[{"title":"中文\u6570\u5b66","url":"https://a.example/","content":"正文","score":0.9}]})","数学",digest));
  std::vector<CamWebSource> sources;camWebAddDigest(digest,sources);assert(sources.size()==1&&sources[0].url=="https://a.example/");assert(sources[0].title.indexOf("数学")>=0);}
 {resetWeb();String content;replies={{400,R"({"error":{"message":"unsupported thinking"}})"},llm("{\"search\":false,\"queries\":[]}")};
  assert(camWebLlm("plan","math",content,"TEST"));assert(bodies.size()==2);JsonDocument minimal;assert(!deserializeJson(minimal,bodies[1]));assert(minimal["thinking"].isNull()&&minimal["reasoning_effort"].isNull()&&minimal["max_completion_tokens"].isNull());}
 const String json=R"({"title":"a content \" trick","nested":{"content":"wrong"},"content":"中文 \u4e2d\u6587 \ud83d\ude00 \"quote\"\nend"})";
 for(size_t split:{1,2,3,7,37}){CamWebFetchSink sink;for(size_t p=0;p<json.length()&&!sink.ready;p+=split)sink.write((const uint8_t*)json.c_str()+p,min(split,json.length()-p));assert(sink.ready&&!sink.rejected);assert(sink.text=="中文 中文 😀 \"quote\"\nend");}
 {CamWebFetchSink sink;String j="{\"content\":\""+String(std::string(200000,'x'))+"\"}";sink.write((const uint8_t*)j.c_str(),j.length());assert(sink.ready&&sink.truncated&&sink.text.length()==1500);}
 {CamWebFetchSink sink;String j=R"({"results":[{"raw_content":"tavily page"}]})";sink.write((const uint8_t*)j.c_str(),j.length());assert(sink.ready&&sink.text=="tavily page");}
 {CamWebFetchSink sink;String j=R"({"content":"\uZZZZ"})";sink.write((const uint8_t*)j.c_str(),j.length());assert(sink.rejected);}
 const String a=R"({"results":[{"title":"Alpha","url":"https://a.example/news","summary":"Alpha facts","date":"2026-11-02"},{"title":"Beta","url":"https://b.example/news","summary":"Beta facts"}]})";
 const String b=R"({"results":[{"title":"Alpha duplicate","url":"https://a.example/news#section","summary":"same"},{"title":"Gamma","url":"https://c.example/news","summary":"Gamma facts"}]})";
 resetWeb();replies={llm(R"({"search":true,"queries":["AI news Chinese","AI news English"]})"),{200,a},{200,b},llm(R"({"keep":[1,3],"fetch":[1,3]})"),{200,R"({"content":"Alpha original page dated 2026-11-02"})"},{200,R"({"content":"Gamma original page dated 2026-11-02"})"},llm(R"({"items":[{"id":1,"summary":"Alpha verified in excerpt"},{"id":3,"summary":"Gamma verified in excerpt"},{"id":6,"summary":"invented"}]})")};
 String evidence;bool searched=false;assert(camRunWebResearch("today AI news",0,false,evidence,searched));assert(searched&&replies.empty());assert(evidence.indexOf("a.example")>=0&&evidence.indexOf("c.example")>=0&&evidence.indexOf("b.example")<0&&evidence.indexOf("invented")<0);assert(evidence.length()<=2400);assert(bodies.size()==7);assert(!camWebRunning&&camWebTrace.indexOf("unique")>=0);assert(camWebTrace.indexOf("Read 2 pages")>=0);
 resetWeb();replies={llm(R"({"search":false,"queries":[]})")};assert(camRunWebResearch("1+1",0,false,evidence,searched)&&!searched&&evidence.empty()&&bodies.size()==1);
 resetWeb();replies={llm(R"({"search":true,"queries":["q"]})"),{200,a},llm(R"({"keep":[99],"fetch":[]})")};assert(!camRunWebResearch("q",0,false,evidence,searched)&&evidence.empty());
 resetWeb();replies={llm(R"({"search":true,"queries":["q"]})"),{401,"{}"}};assert(!camRunWebResearch("q",0,false,evidence,searched)&&bodies.size()==2);
 resetWeb();replies={llm("not valid JSON")};assert(!camRunWebResearch("write a poem",0,false,evidence,searched)&&bodies.size()==1);
 resetWeb();pendingCancel=true;buttonEvents=(void*)1;assert(!camRunWebResearch("cancel",0,false,evidence,searched)&&camWebWasCancelled&&bodies.empty());buttonEvents=nullptr;
 camWebRunning=true;camWebStarted=0;clockMs=CAM_WEB_BUDGET_MS;assert(camWebCancelled());camWebRunning=false;
 std::cout<<"PASS: UTF-8/escaped Unicode streaming extraction; huge-page bound; Tavily extraction; two-query dedup; multi-page pipeline; source ownership; no-search; invalid IDs; authentication failure\n";
}
