#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#define ARDUINOJSON_ENABLE_ARDUINO_PRINT 0
#define ARDUINOJSON_ENABLE_PROGMEM 0
#include "Arduino.h"
#include <ArduinoJson.h>
#include "cam_jpeg_stream.h"
#include "cam_vision_policy.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include "cam_queue_under_test.h"
using PsramString=String;
#include "cam_context_under_test.h"
static String lastError,camVisionDetail="low";
struct Config {bool enableThinking=true,showThinking=true,clockMode=false;size_t llmContextMaxBytes=8192;} cfg;
static size_t omitBefore=0;
static String buildLlmRequestBodyBudget(size_t assistantIndex,bool stream,const String&,size_t,bool){
 JsonDocument doc;doc["model"]="fixture-model";doc["stream"]=stream;
 auto messages=doc["messages"].to<JsonArray>();
 auto system=messages.add<JsonObject>();system["role"]="system";system["content"]="Fixture";
 for(size_t i=omitBefore;i<chatHistory.size();++i){if(i==assistantIndex)continue;auto m=messages.add<JsonObject>();m["role"]=chatHistory[i].role;m["content"]=chatHistory[i].content;m["_cam_history_index"]=i;}
 String body;serializeJson(doc,body);return body;
}
#include "cam_vision_parts_under_test.h"
static uint32_t millis(){static uint32_t now=0;return ++now;}
static String uiText(const char* cn,const char*,const char*){return cn;}
struct SerialMock {template<class... Args>void printf(const char*,Args...){} void println(const char*){}} Serial;
struct EspMock {int getFreeHeap(){return 100000;}} ESP;
static constexpr size_t LLM_FALLBACK_CAPTURE_BYTES=32768,LLM_SSE_LINE_MAX_BYTES=32768,LLM_VISIBLE_REPLY_MAX_BYTES=49152;
static constexpr char RICH_THINKING_TOGGLE=3;
enum class UiPage {Page1};static UiPage uiPage=UiPage::Page1;
static bool screenSleeping=false,chatLinesDirty=true,chatAutoScrollBottom=false;
static std::vector<String> chatLines;static std::vector<uint8_t> chatLineRoles;
static size_t maxTailBytes=0;
static void rebuildChatLines(){chatLines={"user","","assistant",""};chatLineRoles={1,0,2,0};}
static void appendChatMessageLines(const ChatMsg& msg,std::vector<String>& lines,std::vector<uint8_t>& roles){maxTailBytes=max(maxTailBytes,msg.content.length());lines.push_back(msg.content);roles.push_back(2);lines.push_back("");roles.push_back(0);}
static void tickLlmWaitingAnimation(bool){}
static void drawPage1TopLine(){}
static void drawChatWindow(bool){}
static void drawPage1Footer(const String&){}
static void updateChatMessageContent(size_t i,const String& content,bool=true){if(i<chatHistory.size())chatHistory[i].content=content;}
static String truncateUtf8ToBytes(const String& text,size_t count){count=min(count,text.length());while(count<text.length()&&count&&(uint8_t(text[count])&0xc0)==0x80)--count;return text.substring(0,count);}
#include "cam_think_helpers_under_test.h"
#include "cam_sse_under_test.h"
static void feed(LlmSseSink& sink,const char* key,const String& text){JsonDocument d;d["choices"][0]["delta"][key]=text;String line;serializeJson(d,line);line="data: "+line+"\n\n";sink.write((const uint8_t*)line.data(),line.size());}
int main(int argc,char**argv){
 std::ofstream output(argv[1]);
 for(size_t groups:{1,2,3}){
  chatHistory.clear();SD_MMC.files.clear();
  for(size_t g=0;g<groups;++g){ChatMsg user;user.role="user";user.content="Question "+String(g)+" quote: \" 中文 \\";
   for(size_t j=0;j<3;++j){String path="/photo/"+String(g)+"-"+String(j)+".jpg";user.images.push_back({path,3});SD_MMC.files[path]={(uint8_t)g,(uint8_t)j,255};}
   chatHistory.push_back(user);chatHistory.push_back({"assistant","Previous answer",{}});
  }
  chatHistory.push_back({"user","Follow-up, same images?",{}});chatHistory.push_back({"assistant","",{}});
  assert(camHasImageContext());
  // Round-trip the real persistent reference reader.
  JsonDocument saved;auto refs=saved["images"].to<JsonArray>();for(auto& im:chatHistory[0].images){auto r=refs.add<JsonObject>();r["path"]=im.path;r["bytes"]=im.bytes;}
  ChatMsg loaded;loaded.role="user";camLoadImageReferences(loaded,saved["images"].as<JsonArrayConst>());assert(loaded.images.size()==3);assert(loaded.images[0].path==chatHistory[0].images[0].path);
  for(bool stream:{false,true}){
   String parts[7];CamJpegSource sources[6];size_t count=0;
   assert(camBuildVisionParts(chatHistory.size()-1,stream,parts,sources,count));assert(count==min<size_t>(groups*3,6));
   CamMultiJpegRequestStream upload(parts,sources,count,SD_MMC);String body;while(upload.available()>0){int ch=upload.read();assert(ch>=0);body+=char(ch);}assert(!upload.failed());assert(body.size()==upload.size());
   JsonDocument req;assert(!deserializeJson(req,body));assert(req["stream"].as<bool>()==stream);assert(body.find("_cam_history_index")==String::npos);assert(body.find("__CAM_JPEG")==String::npos);
   auto messages=req["messages"].as<JsonArray>();size_t pictures=0;for(JsonObject m:messages)if(m["content"].is<JsonArray>())pictures+=m["content"].as<JsonArray>().size()-1;assert(pictures==count);
   assert(messages[messages.size()-1]["content"]=="Follow-up, same images?");
   assert(SD_MMC.peak<=1);output<<groups<<' '<<stream<<' '<<body<<'\n';
  }
 }
 // Current unsaved photo may stream from RAM; a later follow-up cannot invent
 // its pixels after RAM is released. It remains an explicit unavailable ref.
 {uint8_t jpeg[]={1,2,3};camPhoto.jpeg=jpeg;camPhoto.size=3;camPhoto.confirmed=true;
  chatHistory={{"user","temporary photo",{{"",3}}},{"assistant","",{}}};
  String parts[7];CamJpegSource sources[6];size_t count=0;
  assert(camBuildVisionParts(1,true,parts,sources,count));assert(count==1);assert(sources[0].jpeg==jpeg);
  camPhoto.jpeg=nullptr;camReleasePhoto();chatHistory.push_back({"user","follow-up",{}});chatHistory.push_back({"assistant","",{}});
  CamJpegSource later[6];assert(camBuildVisionParts(3,true,parts,later,count));assert(!later[0].jpeg);assert(later[0].path.empty());}
 assert(!camVisionMayRetryResponse(true,-3,false,false));assert(!camVisionMayRetryResponse(false,-5,false,false));assert(!camVisionMayRetryResponse(false,-3,true,false));assert(!camVisionMayRetryResponse(false,-3,false,true));assert(camVisionMayRetryResponse(false,-3,false,false));
 chatHistory={{"user","long reasoning",{}},{"assistant","",{}}};chatLinesDirty=true;
 {LlmSseSink sink(1);for(int i=0;i<1500;++i){feed(sink,"reasoning_content",String(std::string(1024,'r')));assert(sink.answer().size()<=8192);assert(!sink.failed());}
  feed(sink,"content","最终答案：a∈(-∞,-1]。\n");const char* done="data: {\"choices\":[{\"finish_reason\":\"stop\"}]}\n\n";sink.write((const uint8_t*)done,strlen(done));sink.finish();assert(sink.done());assert(!sink.truncated());assert(!sink.failed());assert(sink.answer().find("最终答案")!=String::npos);assert(maxTailBytes<=771);}
 {cfg.showThinking=false;LlmSseSink sink(1);for(int i=0;i<100;++i)feed(sink,"reasoning_content",String(std::string(1024,'r')));assert(sink.answer().empty());feed(sink,"content","Final");sink.finish();assert(sink.answer()=="Final");}
 {cfg.showThinking=true;LlmSseSink sink(1);for(int i=0;i<800&&!sink.done();++i)feed(sink,"content",String(std::string(128,'x')));assert(sink.done());assert(sink.outputLimited());assert(sink.answer().length()<=49152);}
 {LlmSseSink sink(1);feed(sink,"content","<think>");for(int i=0;i<1000;++i){feed(sink,"content",String(std::string(1024,'t')));assert(sink.answer().length()<=8192);assert(!sink.truncated());}
  feed(sink,"content","</think>Final answer after inline reasoning.");sink.finish();assert(!sink.truncated());assert(sink.answer().find("Final answer after inline reasoning.")!=String::npos);}
 std::cout<<"context_round_trip_and_long_reasoning_parser_passed\n";
}
