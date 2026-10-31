from host_toolchain import host_compiler, arduinojson_include
"""Run the actual retry function against SDK-like stop-clears-trust semantics."""
from pathlib import Path
import os,subprocess,json
root=Path(__file__).resolve().parents[1];out=root/'validation/20261031-v0413';out.mkdir(parents=True,exist_ok=True)
source=(root/'VoiceAssistant/iat_dns_fallback.h').read_text(encoding='utf-8')
retry='static bool iatConnectWithDnsFallback('+source.split('static bool iatConnectWithDnsFallback(',1)[1]
prefix=r'''
#include "Arduino.h"
#include <atomic>
#include <cassert>
#include <cstdio>
struct IPAddress{
 IPAddress(){} IPAddress(int,int,int,int){}
 bool fromString(const String& text){return !text.empty()&&text[0]>='0'&&text[0]<='9';}
 String toString()const{return "221.178.62.105";}
};
struct IatEndpoint{String host="asr.h9647123.cn";uint16_t port=16457;bool tls=true;};
static constexpr const char* CAM_TLS_CA_PEM=nullptr;
struct Log{template<class...T>void printf(const char*,T...) {}}Serial;
static int configured=0,queries=0;
static bool resolveOk=true;
class NetworkClient{
public:virtual ~NetworkClient(){};virtual int connect(const char*,uint16_t,uint32_t)=0;virtual void stop()=0;
};
class WiFiClientSecure:public NetworkClient{
public:bool trustAttached=true,normalSuccess=false;int failures=0,retries=0;
 int connect(const char*,uint16_t,uint32_t)override{if(normalSuccess)return 1;stop();return 0;}
 void stop()override{trustAttached=false;}
 int lastError(char* out,size_t size){snprintf(out,size,"fixture failure");return -1;}
 int connect(IPAddress,uint16_t,const char* host,const char*,const char*,const char*){
  ++retries;assert(trustAttached);assert(String(host)=="asr.h9647123.cn");
  if(failures){--failures;stop();return 0;}return 1;
 }
};
static bool camConfigureTls(WiFiClientSecure& client){++configured;client.trustAttached=true;return true;}
static bool iatQueryAlternateDns(const String&,const IPAddress&,IPAddress&,std::atomic<bool>*){++queries;return resolveOk;}
'''
checks=r'''
int main(){
 IatEndpoint endpoint;
 {WiFiClientSecure client;client.normalSuccess=true;
  assert(iatConnectWithDnsFallback(client,client,endpoint,8000,true));assert(queries==0&&configured==0);}
 {WiFiClientSecure client;assert(iatConnectWithDnsFallback(client,client,endpoint,8000,true));assert(client.retries==1&&configured==1&&queries==1);}
 configured=queries=0;
 {WiFiClientSecure client;client.failures=1;assert(iatConnectWithDnsFallback(client,client,endpoint,8000,true));assert(client.retries==2&&configured==2&&queries==2);}
 configured=queries=0;
 {WiFiClientSecure client;assert(!iatConnectWithDnsFallback(client,client,endpoint,8000,false));assert(queries==0&&configured==0);}
 {WiFiClientSecure client;std::atomic<bool> cancel{true};assert(!iatConnectWithDnsFallback(client,client,endpoint,8000,true,&cancel));assert(queries==0);}
 for(const char* host:{"192.168.0.111","gateway.local","gateway.lan","gateway.internal"}){
  WiFiClientSecure client;endpoint.host=host;assert(!iatConnectWithDnsFallback(client,client,endpoint,8000,true));assert(queries==0);}
 endpoint.host="asr.h9647123.cn";endpoint.tls=false;
 {WiFiClientSecure client;assert(!iatConnectWithDnsFallback(client,client,endpoint,8000,true));assert(queries==0);}
}
'''
test=out/'tls-retry-test.cpp';test.write_text(prefix+retry+checks,encoding='utf-8')
host=root/'.build-host';env=os.environ.copy()
for key,folder in [('TEMP','tmp'),('TMP','tmp'),('ZIG_GLOBAL_CACHE_DIR','global-cache'),('ZIG_LOCAL_CACHE_DIR','local-cache')]:(host/folder).mkdir(parents=True,exist_ok=True);env[key]=str(host/folder)
exe=out/'tls-retry-test.exe'
subprocess.run([*host_compiler(),'-std=c++17','-I',str(root/'tools/host-tests'),str(test),'-o',str(exe)],env=env,check=True)
subprocess.run([str(exe)],check=True)
result=dict(actual_retry_function_passed=True,stop_clears_trust_simulated=True,trust_reattached_before_each_retry=True,
 second_retry_after_first_tls_failure_tested=True,normal_success_no_extra_dns=True,plain_local_cancel_and_disabled_guards_passed=True,
 hardware_tested=False,flashed=False)
(out/'tls-retry-tests.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps(result,indent=2))
