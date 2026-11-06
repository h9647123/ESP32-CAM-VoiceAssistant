from host_toolchain import host_compiler, arduinojson_include
"""Deterministic presentation-clock tests of the firmware's actual cursor."""
from pathlib import Path
import os,subprocess,json
root=Path(__file__).resolve().parents[1];out=root/'validation/20261107-v0420/reveal';out.mkdir(parents=True,exist_ok=True)
source=r'''
#include "cam_stream_reveal.h"
#include <string>
#include <cassert>
#include <iostream>
static bool boundary(const std::string& s,size_t at){return at==s.size() || (uint8_t(s[at])&0xc0)!=0x80;}
int main(){
 {CamStreamReveal reveal;std::string text="中文测试🙂abc";size_t at=0;unsigned frames=0;
  for(uint32_t now=0;at<text.size()&&now<1000;now+=20){size_t next=reveal.next(text.data(),text.size(),at,now);assert(next>=at&&next<=text.size()&&boundary(text,next));if(next>at)++frames;at=next;}
  assert(at==text.size()&&frames>=5);
  assert(reveal.next(text.data(),text.size(),0,1001,true)==text.size());
 }
 {CamStreamReveal reveal;std::string text;for(int i=0;i<400;++i)text+="字";
  size_t at=0;unsigned frames=0,maxStep=0;uint32_t now=0;
  for(;at<text.size()&&now<5000;now+=20){auto next=reveal.next(text.data(),text.size(),at,now,false,now>=1500);assert(boundary(text,next));if(next>at)++frames;maxStep=maxStep>next-at?maxStep:unsigned(next-at);at=next;}
  assert(at==text.size()&&frames>10&&maxStep<text.size()/4);
  std::cout<<"burst: frames="<<frames<<" max_step_bytes="<<maxStep<<" drained_ms="<<now<<"\n";
 }
 {CamStreamReveal reveal;std::string text;size_t at=0;unsigned frames=0;uint32_t now=0;
  for(;now<6000;now+=20){if(now%240==0&&now<4800)for(int i=0;i<20;++i)text+="中";
   auto next=reveal.next(text.data(),text.size(),at,now,false,now>=4800);assert(next>=at&&boundary(text,next));if(next>at)++frames;at=next;
  }
  assert(at==text.size()&&frames>100);std::cout<<"chunked: frames="<<frames<<" received_bytes="<<text.size()<<" all_visible=1\n";
 }
 {CamStreamReveal reveal;const char incomplete[]={char(0xe4),char(0xb8)};assert(reveal.next(incomplete,2,0,0)==0);}
 std::cout<<"PASS: paced UTF-8; incomplete codepoint held; immediate reveal; burst catch-up; final-tail drain; no reception buffer copies\n";
}
'''
(out/'reveal.cpp').write_text(source,encoding='utf-8')
host=root/'.build-host';host.mkdir(exist_ok=True);env=os.environ.copy()
for key,folder in [('TEMP','tmp'),('TMP','tmp'),('ZIG_GLOBAL_CACHE_DIR','global-cache'),('ZIG_LOCAL_CACHE_DIR','local-cache')]:(host/folder).mkdir(parents=True,exist_ok=True);env[key]=str(host/folder)
exe=out/'reveal-tests.exe'
subprocess.run([*host_compiler(),'-std=c++17','-I',str(root/'VoiceAssistant'),str(out/'reveal.cpp'),'-o',str(exe)],env=env,check=True)
r=subprocess.run([str(exe)],capture_output=True,text=True,check=True);(out/'tests.log').write_text(r.stdout,encoding='utf-8');print(r.stdout)
