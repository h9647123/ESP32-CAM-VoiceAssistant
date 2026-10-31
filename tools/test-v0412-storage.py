from host_toolchain import host_compiler, arduinojson_include
"""Native transaction failure/recovery and actual model capability header tests."""
from pathlib import Path
import json,os,subprocess
root=Path(__file__).resolve().parents[1]
out=root/'validation/20261031-v0412';out.mkdir(parents=True,exist_ok=True)
source=(root/'VoiceAssistant/VoiceAssistant.ino').read_text(encoding='utf-8')
storage=(root/'VoiceAssistant/cam_chat_storage.h').read_text(encoding='utf-8')
transaction='static bool chatRecoverFile('+storage.split('static bool chatRecoverFile(',1)[1]
native=r'''
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#define ARDUINOJSON_ENABLE_ARDUINO_PRINT 0
#define ARDUINOJSON_ENABLE_PROGMEM 0
#include "Arduino.h"
#include <ArduinoJson.h>
#include <cassert>
#include <map>
#include <vector>
struct Prefs{String saved;String getString(const char*,const char* fallback){return saved.empty()?String(fallback):saved;}void putString(const char*,const String& v){saved=v;}}runtimePrefs;
#include "cam_model_capabilities.h"
namespace fs{struct FS{
 std::map<String,String> files;int calls=0,failAt=0;bool failRemove=false;
 bool exists(const String& p){return files.count(p);}
 bool remove(const String& p){if(failRemove)return false;return files.erase(p);}
 bool rename(const String& a,const String& b){if(++calls==failAt||!exists(a)||exists(b))return false;files[b]=files[a];files.erase(a);return true;}
};}
'''+transaction+r'''
static constexpr uint8_t INTERNAL_SESSION_COUNT=5;
struct Config{uint8_t currentSession=1;}cfg;
'''+storage.split('static bool chatSessionOnTf()',1)[1].split('static String chatTfBoundIdentity;',1)[0].join(['static bool chatSessionOnTf()',''])+r'''
static String chatTfBoundIdentity,inserted;
static bool chatStorageNotice=false;
static bool internalReady=true;
static bool initFileSystem(){return internalReady;}
static String chatReadTfIdentity(){return inserted;}
static bool chatStorageFailure(){return false;}
'''+('static bool chatStorageReady() {'+storage.split('static bool chatStorageReady() {',1)[1].split('// Keep a last-good',1)[0])+r'''
int main(){
 for(int slot=1;slot<=10;++slot){cfg.currentSession=slot;assert(chatSessionOnTf()==(slot>5));}
 cfg.currentSession=6;inserted="card-A";assert(chatStorageReady());assert(chatTfBoundIdentity=="card-A");
 inserted="";assert(!chatStorageReady());inserted="card-B";assert(!chatStorageReady());
 inserted="card-A";assert(chatStorageReady());cfg.currentSession=1;inserted="";assert(chatStorageReady());
 for(int failed=0;failed<=2;++failed){
  fs::FS f;f.files["s"]="OLD";f.files["s.tmp"]="NEW";f.failAt=failed;
  bool committed=chatCommitFile(f,"s");assert(committed==(failed==0));
  f.failAt=0;assert(chatRecoverFile(f,"s"));assert(f.files["s"]==(committed?"NEW":"OLD"));
 }
 fs::FS crash;crash.files["s.bak"]="OLD";crash.files["s.tmp"]="PARTIAL";
 assert(chatRecoverFile(crash,"s"));assert(crash.files["s"]=="OLD");
 fs::FS cleanup;cleanup.files["s"]="OLD";cleanup.files["s.bak"]="OLDER";cleanup.files["s.tmp"]="NEW";cleanup.failRemove=true;
 assert(!chatCommitFile(cleanup,"s"));assert(cleanup.files["s"]=="OLD");
 fs::FS fresh;fresh.files["s.tmp"]="NEW";assert(chatCommitFile(fresh,"s"));assert(fresh.files["s"]=="NEW");
 assert(camModelVision("url-A","same-model")==-1);
 assert(camSetModelVision("url-A","same-model",true));assert(camSetModelVision("url-B","same-model",false));
 assert(camModelVision("url-A","same-model")==1);assert(camModelVision("url-B","same-model")==0);
 assert(camModelVision("url-C","same-model")==-1);
 camSaveModelCapabilities();camModelCapabilities.clear();camLoadModelCapabilities();
 assert(camModelVision("url-A","same-model")==1&&camModelVision("url-B","same-model")==0);
 JsonDocument malformed;deserializeJson(malformed,R"([{"api_url":"bad","model":"x","vision":"true"}])");
 camImportModelCapabilities(malformed.as<JsonArrayConst>());assert(camModelVision("bad","x")==-1);
 bool vision=false;String tag="model-one vision";assert(camParseModelCapability(tag,vision)&&vision&&tag=="model-one");
 tag="model-two text";assert(camParseModelCapability(tag,vision)&&!vision&&tag=="model-two");
 tag="model-three";assert(!camParseModelCapability(tag,vision));
 assert(camSetModelVision("url-A","same-model",false));assert(camModelVision("url-A","same-model")==0);
 for(int i=2;i<64;++i)assert(camSetModelVision("unique",String(i),false));
 assert(!camSetModelVision("overflow","x",true));
}
'''
vision=(root/'VoiceAssistant/cam_vision.h').read_text(encoding='utf-8')
guard='static bool camCheckVisionConfig() {'+vision.split('static bool camCheckVisionConfig() {',1)[1].split('static bool camBuildVisionParts(',1)[0]
fixture='''
static bool camVisionEnabled=true;
static struct {bool confirmed=false;}camPhoto;
static bool camHasImageContext(){return false;}
static String lastError;
static String uiText(const char* cn,const char*,const char*){return cn;}
'''
native=native.replace('struct Config{uint8_t currentSession=1;}cfg;',
    'struct Config{uint8_t currentSession=1;String llmApiKey="fixture",llmApiUrl="https://fixture.test/v1",llmModelId="fixture-model";}cfg;')
native=native.replace('int main(){',fixture+guard+'''int main(){
 assert(!camCheckVisionConfig());assert(lastError.indexOf("未标注")>=0);
 camSetModelVision(cfg.llmApiUrl,cfg.llmModelId,false);
 assert(!camCheckVisionConfig());assert(lastError.indexOf("不支持图像识别")>=0);
 camSetModelVision(cfg.llmApiUrl,cfg.llmModelId,true);assert(camCheckVisionConfig());
 camModelCapabilities.clear();
''')
test=out/'storage-model-test.cpp';test.write_text(native,encoding='utf-8')
host=root/'.build-host';env=os.environ.copy()
for key,folder in [('TEMP','tmp'),('TMP','tmp'),('ZIG_GLOBAL_CACHE_DIR','global-cache'),('ZIG_LOCAL_CACHE_DIR','local-cache')]:(host/folder).mkdir(parents=True,exist_ok=True);env[key]=str(host/folder)
exe=out/'storage-model-test.exe'
subprocess.run([*host_compiler(),'-std=c++17','-I',str(root/'tools/host-tests'),'-I',str(root/'VoiceAssistant'),'-I',str(arduinojson_include()),str(test),'-o',str(exe)],env=env,check=True)
subprocess.run([str(exe)],check=True)
assert 'static constexpr uint8_t SESSION_COUNT = 10;' in source
switch=source.split('static void switchSession(uint8_t session) {',1)[1].split('static bool clearChatHistory()',1)[0]
assert switch.index('!saveChatHistory()')<switch.index('camReleasePhoto()')<switch.index('cfg.currentSession = session')
assert 'if (chatSessionOnTf()) return false;' in source
assert 'uiText("上下文已清空","History cleared","History cleared")' in source
assert 'camImportModelCapabilities(root["model_capabilities"]' in source
report=dict(hardware_tested=False,flashed=False,session_count=10,flash_slots=[1,2,3,4,5],tf_slots=[6,7,8,9,10],
 actual_transaction_failure_recovery_test_passed=True,actual_card_identity_guard_passed=True,
 actual_capability_json_persistence_and_provider_isolation_passed=True,legacy_capability_unknown=True,
 actual_vision_request_guard_for_supported_unsupported_unmarked_passed=True,
 clear_prompt_localized=True)
(out/'storage-model-tests.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report,indent=2))
