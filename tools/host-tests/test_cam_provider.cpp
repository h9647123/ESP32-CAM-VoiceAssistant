#include "Arduino.h"
#include "iat_endpoint.h"
#include <vector>
#include <cassert>
#include <iostream>
#include <cstdarg>
struct ProviderProfile{String name,apiUrl,apiKey;std::vector<String> models;String selectedModel;};
struct Config{String llmProviderName,llmApiUrl,llmApiKey,llmModelId;}cfg;
static std::vector<ProviderProfile> providerProfiles;
static std::vector<String> modelProfiles;
static uint8_t activeProviderProfileIndex=0,providerMenuProfileIndex=0,modelMenuProfileIndex=0;
static constexpr unsigned MAX_MODEL_PROFILES=24,MAX_PROVIDER_PROFILES=8;
static constexpr char RICH_LITERAL_ESCAPE=5,RICH_THINKING_TOGGLE=3,RICH_BOLD_TOGGLE=1,RICH_HEADING_TOGGLE=2,RICH_CODE_TOGGLE=4;
static bool camSerialReplies=true;
static String serialText;
struct Log{
 void println(const String& text){serialText+=text;serialText+='\n';}
 void printf(const char* format,...){char out[256];va_list args;va_start(args,format);vsnprintf(out,sizeof(out),format,args);va_end(args);serialText+=out;}
 void write(const uint8_t* bytes,size_t count){serialText.append((const char*)bytes,count);}
}Serial;
static String uiText(const char*,const char*,const char* en){return en;}
static int camModelVision(const String&,const String& model){return model=="can-see"?1:model=="cannot-see"?0:-1;}
static void yield(){}
static void saveRuntimeConfig(){}
static String chatCompletionsUrlFromBase(const String& value){return value;}
#include "provider_under_test.h"
int main(){
 providerProfiles.push_back({"deepseek","https://api.deepseek.com/chat/completions","fixture-ds",{"deepseek-flash","deepseek-v4-flash"},"deepseek-flash"});
 applyProviderProfile(0);assert(cfg.llmModelId=="deepseek-flash");
 createProvider("custom","https://api.example.com/v1/chat/completions","fixture-custom");
 assert(cfg.llmProviderName=="custom"&&cfg.llmModelId.empty());assert(currentProvider()->models.empty()&&modelProfiles.empty());
 addCurrentProviderModelOnly("custom-model");assert(cfg.llmModelId=="custom-model"&&modelProfiles.size()==1);
 applyProviderProfile(0);assert(cfg.llmModelId=="deepseek-flash");
 createProvider("other","https://other.example.com/v1/chat/completions","fixture-other");assert(cfg.llmModelId.empty()&&modelProfiles.empty());
 createProvider("custom","https://api.example.com/v1/chat/completions","fixture-new");assert(cfg.llmModelId=="custom-model"&&modelProfiles.size()==1);
 createProvider("spoof","https://api.deepseek.com.evil.example/chat/completions","fixture-spoof");assert(cfg.llmModelId.empty()&&modelProfiles.empty());
 serialText="";camPrintAssistantReply(String(char(3))+"private reasoning\n\n"+String(char(3))+"Final answer: 42.");
 assert(serialText.find("Final answer: 42.")!=String::npos&&serialText.find("private reasoning")==String::npos);
 assert(serialText.find("[LLM-REPLY-BEGIN]")!=String::npos&&serialText.find("[LLM-REPLY-END]")!=String::npos);
 camSerialReplies=false;serialText="";camPrintAssistantReply("not printed");assert(serialText.empty());
 createProvider("empty-new","https://new.example/v1/chat/completions","fixture-new");assert(cfg.llmModelId.empty()&&modelProfiles.empty());serialText="";camPrintModelProfiles();assert(serialText.find("unset")!=String::npos);
 addCurrentProviderModelOnly("one");addCurrentProviderModelOnly("two");addCurrentProviderModelOnly("three");assert(cfg.llmModelId=="one");assert(removeModelProfile("one")&&cfg.llmModelId=="two");assert(removeModelProfile("two")&&cfg.llmModelId=="three");assert(!removeModelProfile("three")&&modelProfiles.size()==1);syncLegacyModelProfiles();assert(modelProfiles.size()==1&&modelProfiles[0]=="three");
 modelProfiles={"can-see","cannot-see","unknown"};cfg.llmModelId="can-see";serialText="";camPrintModelProfiles();assert(serialText.find("can-see [vision]")!=String::npos&&serialText.find("cannot-see [text only]")!=String::npos&&serialText.find("unknown [unset; treated as text only]")!=String::npos);
 std::cout<<"provider_isolation_and_serial_reply_passed\n";
}
