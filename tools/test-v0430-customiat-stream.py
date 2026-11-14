from host_toolchain import host_compiler, arduinojson_include
"""Portable tests of actual CAM batch policy and actual IAT receive method.
No serial connection, credentials, microphone or network.
"""
from pathlib import Path
import json, os, subprocess, sys
from source_blocks import get

root=Path(__file__).resolve().parents[1]
out=root/'validation/20261114-v0430/host-stream';out.mkdir(parents=True,exist_ok=True)
ino=(root/'VoiceAssistant/VoiceAssistant.ino').read_text(encoding='utf-8')
proto=(root/'VoiceAssistant/xfyun_iat_v2.h').read_text(encoding='utf-8')
# Keep the production receive/reset methods exactly; platform cryptography and
# wire base64 are irrelevant to snapshot ownership. Test transport below maps
# already-decoded JSON bytes 1:1 through the base64 boundary stub.
head=proto.split('struct XfyunIatV2 {',1)[1].split('  static String hmac256',1)[0]
receiver='  bool receive('+proto.split('  bool receive(',1)[1].split('\n};',1)[0]
(out/'iat_receive_actual.h').write_text('struct XfyunIatV2 {'+head+receiver+'\n};\n',encoding='utf-8')
source=r'''
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#define ARDUINOJSON_ENABLE_ARDUINO_PRINT 0
#define ARDUINOJSON_ENABLE_PROGMEM 0
#include "Arduino.h"
#include "cam_json_config.h"
#include <ArduinoJson.h>
#include "cam_iat_stream.h"
#include "asr_capture_limits.h"
#include <map>
#include <vector>
#include <cassert>
#include <iostream>
#define MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL -42
// Identity codec isolates the actual receive method's JSON/state semantics;
// live WebSocket tests must separately exercise real base64 and socket framing.
static int mbedtls_base64_decode(uint8_t* out,size_t cap,size_t* size,const uint8_t* in,size_t n){
 *size=n;if(!out||cap<n)return MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL;memcpy(out,in,n);return 0;
}
#include "iat_receive_actual.h"
static JsonDocument packet(const char* text,bool final=false){
 JsonDocument words;words["ret"]=0;words["sn"]=0;words["pgs"]="rpl";
 words["rg"][0]=0;words["rg"][1]=0;words["ws"][0]["cw"][0]["w"]=text;
 String serialized;serializeJson(words,serialized);
 JsonDocument p;p["header"]["code"]=0;p["header"]["status"]=final?2:1;
 p["payload"]["result"]["seq"]=0;p["payload"]["result"]["status"]=final?2:1;
 p["payload"]["result"]["text"]=serialized;return p;
}
int main(){
 assert(camAsrLiveBodyLimitMs(true,30,1800)==58200);
 assert(camAsrLiveBodyLimitMs(false,30,1800)==30000);
 assert(camIatPublishedBatch(3199,0,true,false)==0);
 assert(camIatPublishedBatch(3200,0,true,false)==3200);
 assert(camIatPublishedBatch(639,0,false,false)==0);
 assert(camIatPublishedBatch(640,0,false,false)==640);
 assert(camIatPublishedBatch(5,6,true,true)==0); // no unsigned underflow
 // Deterministic variable-sized producer publishes. The consumer never reads
 // beyond the published prefix; a delayed socket cannot alter stored samples.
 for(bool custom:{false,true})for(uint32_t total:{1U,639U,640U,3199U,3200U,123457U,960000U}){
  std::vector<uint32_t> pcm(total);for(uint32_t i=0;i<total;++i)pcm[i]=i^0x55aa;
  uint32_t ready=0,sent=0,verified=0;
  while(ready<total){ready=std::min(total,ready+137U);
   const size_t count=camIatPublishedBatch(ready,sent,custom,false);
   if(count){assert(sent+count<=ready);for(size_t i=0;i<count;++i)assert(pcm[sent+i]==((verified++)^0x55aa));sent+=count;}
  }
  while(const size_t count=camIatPublishedBatch(ready,sent,custom,true)){
   assert(sent+count<=total);for(size_t i=0;i<count;++i)assert(pcm[sent+i]==((verified++)^0x55aa));sent+=count;
  }
  assert(sent==total&&verified==total);
 }
 XfyunIatV2 state;
 for(const char* text:{"今天阳光很好", "今天阳光", "今天", "今天", "今天很好。"}){
  auto p=packet(text);assert(state.receive(p));assert(state.text==text);
  assert(state.segments.size()==1&&!state.finalReceived);
 }
 auto p=packet("今天很好。",true);assert(state.receive(p));assert(state.finalReceived&&state.segments.size()==1);
 state.reset();assert(state.text.empty()&&!state.finalReceived&&state.segments.empty());
 JsonDocument ack;ack["header"]["code"]=0;ack["header"]["status"]=0;
 assert(state.receive(ack)&&state.acknowledged&&!state.finalReceived);
 auto shortText=packet("短句");assert(state.receive(shortText));assert(!state.finalReceived);
 ack["header"]["status"]=2;assert(state.receive(ack)&&state.finalReceived&&state.text=="短句");
 std::cout<<"PASS: immutable published-prefix consumption, official/custom batching, exact tail, 60s includes tail; actual IAT snapshot repeat/shorten/final ACK\n";
}
'''
(out/'test.cpp').write_text(source,encoding='utf-8')
host=root/'.build-host';host.mkdir(exist_ok=True);env=os.environ.copy()
for key,folder in [('TEMP','tmp'),('TMP','tmp'),('ZIG_GLOBAL_CACHE_DIR','global-cache'),('ZIG_LOCAL_CACHE_DIR','local-cache')]:(host/folder).mkdir(parents=True,exist_ok=True);env[key]=str(host/folder)
cmd=[*host_compiler(),'-std=c++17','-I',str(root/'tools/host-tests'),'-I',str(root/'VoiceAssistant'),'-I',str(out),'-I',arduinojson_include(),str(out/'test.cpp'),'-o',str(out/'test.exe')]
subprocess.run(cmd,env=env,check=True)
p=subprocess.run([str(out/'test.exe')],capture_output=True,text=True,check=True)
(out/'tests.log').write_text(p.stdout+p.stderr,encoding='utf-8');print(p.stdout)
# Wiring assertions protect the behavior that a pure batch test cannot cover.
start=get(ino,'startRecording');stop=get(ino,'stopRecordingAndAnswer')
assert 'camRunHttpWithUi([](){return startAsrSession()?1:0;})' in start
assert 'uploadCustomPcm(' not in stop
assert stop.index('camIatPublishedBatch(')<stop.index('finishAsrSession();')
assert stop.index('finishAsrSession();')<stop.index('commitAsrDraftSegment(')
assert 'memory_order_acquire' in ino and 'network_deferred=0 streaming=1' in start
summary=dict(host_passed=True,raw_base64_live_tested=False,hardware_tested=False,
             custom_batch_samples=3200,official_batch_samples=640,
             custom_body_ms=58200,tail_ms=1800,total_limit_ms=60000,
             snapshot_same_sn_replace=True,shorter_corrections=True,
             microphone_thread_changed=False,original_board_changed=False)
(out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
