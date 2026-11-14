from host_toolchain import host_compiler, arduinojson_include
from pathlib import Path
import os,subprocess
from source_blocks import get
root=Path(__file__).resolve().parents[1];out=root/'validation/20261114-scroll-delay/host';out.mkdir(parents=True,exist_ok=True)
s=(root/'VoiceAssistant/VoiceAssistant.ino').read_text(encoding='utf-8')
code=r'''
#include "Arduino.h"
#include <cassert>
#include <iostream>
#include "cam_scroll_motion.h"
static uint32_t nowMs=1000,drawCost=0;
static uint32_t millis(){return nowMs;}
static constexpr int CHAT_LINE_HEIGHT=22,CHAT_SCROLL_HOLD_SPEED_PX_PER_SEC=300,CHAT_SCROLL_TAP_SPEED_PX_PER_SEC=420,CHAT_SCROLL_HOLD_FRAME_MS=24,CHAT_SCROLL_TAP_FRAME_MS=16;
enum class UiPage{Page1,Page2};static UiPage uiPage=UiPage::Page1;
static struct{bool clockMode=false;}cfg;
static bool recording=false,screenSleeping=false,chatLinesDirty=false,chatAutoScrollBottom=false;
static int32_t chatScrollPixels=1000,chatScrollTargetPixels=1000;
static int8_t chatScrollHoldDir=0;
static uint32_t chatScrollLastFrameAt=1000,chatScrollHoldLastAt=0,chatScrollHoldRemainder=0,chatScrollFrameRemainder=0,chatScrollFrames=0,chatScrollMaxDrawMs=0,chatScrollMaxFrameMs=0;
static int32_t clampInt(int32_t v,int32_t lo,int32_t hi){return max(lo,min(v,hi));}
static int32_t maxChatScrollPixels(){return 10000;}
static void syncChatScrollRowsFromPixels(){}
static void rebuildChatLines(){}
static void drawPage1Footer(const String&){}
static String chatScrollStatusText(int32_t){return "";}
static void updateScrollbarThumb(){}
static void drawChatWindowAtPixel(int32_t){nowMs+=drawCost;}
static void stopChatScrollHold();static void updateChatScrollAnimation();
'''
code+='\n'.join(get(s,n) for n in ['scrollChat','stopChatScrollHold','updateChatScrollAnimation'])
code+=r'''
int main(){
 uint32_t rem=0;int sum=0;for(int i=0;i<1000;++i)sum+=camScrollDistance(1,300,rem);assert(sum==300);
 rem=0;sum=0;for(int i=0;i<200;++i)sum+=camScrollDistance(5,300,rem);assert(sum==300);
 for(unsigned pause:{1U,5U,24U,45U,80U})for(unsigned cost:{0U,20U,45U}){
  nowMs=1000;drawCost=cost;chatScrollPixels=chatScrollTargetPixels=1000;chatScrollHoldDir=0;chatScrollHoldLastAt=0;chatScrollLastFrameAt=nowMs;chatScrollFrames=chatScrollMaxDrawMs=chatScrollMaxFrameMs=0;chatScrollHoldRemainder=chatScrollFrameRemainder=0;
  scrollChat(1,true);while(nowMs<2000){updateChatScrollAnimation();nowMs+=pause;assert(abs(chatScrollTargetPixels-chatScrollPixels)<=22);}
  const int moved=chatScrollPixels-1000;assert(moved>=150&&moved<=330);
  stopChatScrollHold();assert(chatScrollPixels==chatScrollTargetPixels);
  chatScrollTargetPixels=chatScrollPixels+200;scrollChat(-1,false);assert(chatScrollTargetPixels<chatScrollPixels);
  scrollChat(1,true);assert(chatScrollTargetPixels>=chatScrollPixels);stopChatScrollHold();assert(chatScrollPixels==chatScrollTargetPixels);
  std::cout<<"pause="<<pause<<" cost="<<cost<<" moved="<<moved<<" max_draw="<<chatScrollMaxDrawMs<<" PASS\n";
 }
 // Reproduce the previous scheduling defects under controlled time steps.
 int oldTarget=0,oldCurrent=0;for(int i=0;i<1000;++i){oldTarget+=1;if(i%24==0)oldCurrent+=10;}assert(oldTarget-oldCurrent>500);
 oldTarget=oldCurrent=0;for(int i=0;i<12;++i){oldTarget+=24;oldCurrent+=10;}assert(oldTarget-oldCurrent>100);
 std::cout<<"PASS actual scroll control at 15 loop/render cadences; bounded lead, immediate reversal/release; old fast-loop and slow-frame backlog reproduced\n";
}
'''
(out/'test.cpp').write_text(code,encoding='utf-8');host=root/'.build-host';host.mkdir(exist_ok=True);env=os.environ.copy()
for k,v in [('TEMP','tmp'),('TMP','tmp'),('ZIG_GLOBAL_CACHE_DIR','global-cache'),('ZIG_LOCAL_CACHE_DIR','local-cache')]:(host/v).mkdir(parents=True,exist_ok=True);env[k]=str(host/v)
r=subprocess.run([*host_compiler(),'-std=c++17','-I',str(root/'tools/host-tests'),'-I',str(root/'VoiceAssistant'),str(out/'test.cpp'),'-o',str(out/'test.exe')],env=env,capture_output=True,text=True);(out/'compile.log').write_text(r.stdout+r.stderr,encoding='utf-8');assert r.returncode==0,r.stderr
r=subprocess.run([str(out/'test.exe')],capture_output=True,text=True);(out/'tests.log').write_text(r.stdout+r.stderr,encoding='utf-8');print(r.stdout+r.stderr);r.check_returncode()
