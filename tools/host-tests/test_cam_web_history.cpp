#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#define ARDUINOJSON_ENABLE_ARDUINO_PRINT 0
#define ARDUINOJSON_ENABLE_PROGMEM 0
#include "Arduino.h"
#include "cam_json_config.h"
#include <ArduinoJson.h>
#include "cam_strict_json.h"
#include "cam_history_io.h"
#include "cam_llm_policy.h"
#include <cassert>
#include <map>
#include <memory>
#include <iostream>
using PsramString=String;
struct CamChatImage{String path;size_t bytes;};
struct ChatMsg{String role;PsramString content;std::vector<CamChatImage> images;PsramString webEvidence,webTrace;size_t diskEnd=0;uint32_t thinkingMs=0;bool thinkingTimed=false,thinkingComplete=false,thinkingActive=false;};
static std::vector<ChatMsg> chatHistory;
static size_t chatHistoryBytes=0,chatHistoryPrefixBytes=0;
static bool chatHistoryReadIncomplete=false;
static bool camValidationMode=false,chatStorageNotice=false,chatHistoryUnsaved=false;
struct HistoryJsonAllocator:ArduinoJson::Allocator {size_t peak=0;
 void* allocate(size_t n)override{peak=max(peak,n);return malloc(n);}void deallocate(void*p)override{free(p);}void* reallocate(void*p,size_t n)override{peak=max(peak,n);return realloc(p,n);}
}camJsonAllocator;
static constexpr size_t MAX_HISTORY_BYTES=150000,MAX_HISTORY_LOAD_BYTES=32768,MIN_INTERNAL_FREE_AFTER_LOAD=98304;
static constexpr int MALLOC_CAP_INTERNAL=1;
static size_t availableHeap=80000;
static size_t heap_caps_get_free_size(int){return availableHeap;}
static void yield(){}
static String camLastSearchSources;
static std::map<String,std::shared_ptr<String>> files;
class File {
 std::shared_ptr<String> data_;size_t at_=0;
public:
 File(){}explicit File(std::shared_ptr<String> p):data_(p){}
 explicit operator bool()const{return bool(data_);}
 size_t size()const{return data_?data_->length():0;}
 size_t position()const{return at_;}
 bool available()const{return data_&&at_<data_->length();}
 bool seek(size_t at){at_=at;return at<=size();}
 int read(){return available()?uint8_t((*data_)[at_++]):-1;}
 size_t read(uint8_t* p,size_t n){size_t i=0;for(;i<n&&available();++i)p[i]=read();return i;}
 String readStringUntil(char c){String text;while(available()){char x=read();if(x==c)break;text+=x;}return text;}
 size_t write(const uint8_t* p,size_t n){if(!data_)return 0;data_->concat((const char*)p,n);return n;}
 size_t write(uint8_t c){return write(&c,1);}
 void flush(){}void close(){data_.reset();}
};
static const char* FILE_READ="r";static const char* FILE_WRITE="w";
struct Store {
 bool exists(const String& p){return files.count(p);}
 bool remove(const String& p){return files.erase(p);}
 File open(const String& p,const char* mode){if(mode==FILE_WRITE)files[p]=std::make_shared<String>();return files.count(p)?File(files[p]):File();}
}store;
static Store& chatStore(){return store;}
static bool chatStorageReady(){return true;}
static bool chatSessionOnTf(){return false;}
static bool loadLegacyNvsChatHistory(){return false;}
static bool chatStorageFailure(){return false;}
static bool chatClearMarkerExists(){return false;}
static void clearLegacyNvsChatHistory(){}
static void clearChatRollingSummary(){}
static void buildChatRollingSummary(){}
static String chatHistoryPath(){return "/chat/session1.jsonl";}
static bool chatRecoverFile(Store&,const String&){return true;}
static bool chatCommitFile(Store&,const String& p){files[p]=files[p+".tmp"];files.erase(p+".tmp");return true;}
static void markChatLinesDirty(){}
static void chatHistoryLoadProgress(size_t,size_t){}
static String sanitizeUtf8(const String& s,bool* changed=nullptr){if(changed)*changed=false;return s;}
static String truncateUtf8ToBytes(const String& s,size_t n){return s.substring(0,min(s.length(),n));}
static void camLoadImageReferences(ChatMsg&,JsonArrayConst){}
static String chatRollingWebEvidence,chatRollingSummary;
static size_t chatRollingSummaryMsgs=0;
static constexpr size_t LLM_SEARCH_CONTEXT_MAX_BYTES=2400;
static bool llmTestIsolation=false;
static String llmTimeContext(){return "Current date: 2026-11-02";}
struct{int currentSession=1;String llmModelId="fixture",llmApiUrl="https://llm.example/chat/completions",llmSystemPrompt;uint8_t deepseekThinkingLevel=2;bool enableThinking=false;}cfg;
struct Log {template<class...T>void printf(const char*,T...){}void println(const String&){} }Serial;
static bool saveChatHistory();
#include "web_history_under_test.h"
int main(){
 {
  String controls;for(unsigned i=1;i<32;++i)controls+=char(i);controls+="中文";
  JsonDocument original;original["content"]=controls;original["literal"]="\\u0003";
  String encoded;File output(std::make_shared<String>());
  auto bytes=std::make_shared<String>();File sink(bytes);CamStrictJsonWriter<File> writer(sink);
  assert(serializeJson(original,writer)==measureJson(original)&&writer.finish());
  assert(writer.outputBytes()==bytes->length());
  for(size_t i=0;i<bytes->length();++i)assert(uint8_t((*bytes)[i])>=32);
  JsonDocument decoded;assert(!deserializeJson(decoded,*bytes));
  assert(decoded["content"].as<String>()==controls&&decoded["literal"].as<String>()=="\\u0003");
  std::cout<<"PASS: strict JSON escaping round-trips all C0 markers and preserves literal escapes\n";
 }
 String original;
 for(int i=0;i<40;++i){JsonDocument d;d["role"]=i%2?"assistant":"user";d["content"]="message-"+String(i);if(i==38){d["web_evidence"]="Retrieved at: 2026-11-02\nhttps://source.example/\nuseful fact";d["web_trace"]="Read 2 pages";}String row;serializeJson(d,row);original+=row+"\n";}
 files[chatHistoryPath()]=std::make_shared<String>(original);
 loadChatHistory();assert(chatHistory.size()==8&&chatHistory[0].content=="message-32"&&chatHistory[7].content=="message-39");
 assert(chatHistory[6].webEvidence.indexOf("source.example")>=0);
 assert(saveChatHistory());assert(*files[chatHistoryPath()]==original);
 addChatMessage("user","follow-up",false);addChatMessage("assistant","answer",false);assert(saveChatHistory());
 availableHeap=200000;loadChatHistory();assert(chatHistory.size()==42);assert(chatHistory[38].webEvidence.indexOf("source.example")>=0);assert(chatHistory[40].content=="follow-up");
 availableHeap=80000;loadChatHistory();assert(chatHistory.size()==8&&chatHistory[6].content=="follow-up");assert(saveChatHistory());
 availableHeap=200000;loadChatHistory();assert(chatHistory.size()==42&&chatHistory[38].webTrace=="Read 2 pages");
 assert(camWebMessageContext(chatHistory[38],true).indexOf("prior_web_research_untrusted")>=0);
 assert(camWebMessageContext(chatHistory[38],false).indexOf("source.example")<0);
 String request=buildLlmRequestBodyBudget(41,true,"",8192,false);
 JsonDocument req;assert(!deserializeJson(req,request));
 bool found=false,question=false;
 for(JsonObjectConst m:req["messages"].as<JsonArrayConst>()) {
   String content=m["content"].as<String>();
   if(content.indexOf("source.example")>=0)found=true;
   if(content=="follow-up")question=true;
   assert(content.indexOf("Read 2 pages")<0);
 }
 if(!found||!question)std::cerr<<"found="<<found<<" question="<<question<<" request="<<request.c_str()<<"\n";
 assert(found&&question);
 request=buildLlmRequestBodyBudget(41,true,"Search failed. raw_results=6 page_excerpts_read=2; no accepted evidence.",8192,false);
 assert(request.indexOf("raw_results=6")>=0&&request.indexOf("page_excerpts_read=2")>=0);
 chatHistory.back().thinkingTimed=true;chatHistory.back().thinkingComplete=true;chatHistory.back().thinkingMs=4321;
 assert(saveChatHistory());loadChatHistory();
 assert(chatHistory.back().thinkingTimed&&chatHistory.back().thinkingComplete&&chatHistory.back().thinkingMs==4321);
 assert(buildLlmRequestBodyBudget(41,true,"",8192,false).indexOf("thinking_ms")<0);
 // Cross the RAM eviction threshold repeatedly, then reload the whole archive.
 files.clear();chatHistory.clear();chatHistoryBytes=chatHistoryPrefixBytes=0;
 String payload;for(int i=0;i<1800;++i)payload+='x';
 for(int i=0;i<190;++i){addChatMessage(i%2?"assistant":"user",String(i)+":"+payload,true);}
 String archived=*files[chatHistoryPath()];size_t rows=0;
 for(size_t i=0;i<archived.length();++i)rows+=archived[i]=='\n';
 assert(rows==190);assert(archived.indexOf("0:")>=0&&archived.indexOf("189:")>=0);
 loadChatHistory();assert(saveChatHistory());assert(*files[chatHistoryPath()]==archived);
 // A full long thought plus final body is one JSONL record, not a 32KB window.
 files.clear();chatHistory.clear();chatHistoryBytes=chatHistoryPrefixBytes=0;
 String thought(char(3));thought+=String(std::string(70000,'r'));thought+=char(3);thought+="FINAL-LONG-THOUGHT-END";
 addChatMessage("user","long thought question",true);addChatMessage("assistant",thought,true);
 const String longArchive=*files[chatHistoryPath()];
 loadChatHistory();assert(!chatHistoryReadIncomplete&&!chatHistory.empty());assert(chatHistory.back().content==thought);
 assert(saveChatHistory());assert(*files[chatHistoryPath()]==longArchive);
 String display=camHistoryDisplayText(thought.c_str(),thought.length(),true,"[FOLDED]");
 assert(display.length()<8500&&display.indexOf("[FOLDED]")>=0&&display.indexOf("FINAL-LONG-THOUGHT-END")>=0);
 assert(camHistoryDisplayText(thought.c_str(),thought.length(),false,"")=="FINAL-LONG-THOUGHT-END");
 // Rendering/folding cannot mutate either RAM or the persisted original.
 assert(chatHistory.back().content==thought&&*files[chatHistoryPath()]==longArchive);
 assert(camWebMessageContext(chatHistory.back(),false)=="FINAL-LONG-THOUGHT-END");
 std::cout<<"PASS: single 70KB reasoning record loads across 32KB window and saves byte-identically; display fold preserves final text and archive\n";
 // A malformed row is evidence, not permission to silently drop it on save.
 *files[chatHistoryPath()]+="{broken}\n";
 String damaged=*files[chatHistoryPath()];loadChatHistory();assert(chatHistoryReadIncomplete);
 assert(!saveChatHistory());assert(*files[chatHistoryPath()]==damaged);
 std::cout<<"PASS: actual history load/save preserves all 42 records under low RAM; newest tail; research metadata round-trip; prompt inclusion/exclusion\n";
 std::cout<<"PASS: 190 large records survive repeated RAM eviction/reload/save; corrupt archive is read-only\n";
}
