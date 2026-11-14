from host_toolchain import host_compiler, arduinojson_include
"""Actual busy-UI callback test. Host simulation, not microphone acceptance."""
from pathlib import Path
import os,subprocess
from source_blocks import get
root=Path(__file__).resolve().parents[1];out=root/'validation/20261114-v0431/meter';out.mkdir(parents=True,exist_ok=True)
code=r'''#include "Arduino.h"
#include <atomic>
#include <cassert>
#include <iostream>
enum class UiPage {Page1,Page2};static UiPage uiPage=UiPage::Page1;
enum class AppState {Listening,Recognizing,Thinking,Idle};static AppState appState=AppState::Listening;
static bool screenSleeping=false,recording=true,iat=true;
static struct{bool clockMode=false;}cfg;
static struct{std::atomic<unsigned> liveRms{0};}bufferedMic;
static uint32_t nowMs=1000,headers=0,meters=0,nonzero=0,lastRms=0;
static uint32_t millis(){return nowMs;}
static bool usesIatV2(){return iat;}
static void drawPage1TopLine(){++headers;}
static void drawRecordingFooter(unsigned rms){++meters;lastRms=rms;if(rms)++nonzero;}
static String uiText(const char*,const char* en,const char*){return en;}
static void drawPage1Footer(const String&){}
static bool camWebRunning=false,camLlmNetworkWaiting=false;
static uint32_t camBusyHeaderTicks=0,camLlmWaitingSince=0;
static void camWebCancelled(){}static void camPollVisionCancel(){}
'''
for folder,fn in [(root,'camTickBusyUi')]:
 source=(folder/'VoiceAssistant/VoiceAssistant.ino').read_text(encoding='utf-8')
 block=get(source,fn).replace('voice','cam')
 test=code+block+r'''
int main(){
 for(nowMs=1000;nowMs<4000;nowMs+=5){bufferedMic.liveRms.store(nowMs/20);camTickBusyUi();}
 assert(meters==30&&nonzero==30&&headers==3&&lastRms>50);
 auto saved=meters;screenSleeping=true;nowMs+=200;camTickBusyUi();assert(meters==saved);
 screenSleeping=false;uiPage=UiPage::Page2;nowMs+=200;camTickBusyUi();assert(meters==saved);
 uiPage=UiPage::Page1;recording=false;nowMs+=200;camTickBusyUi();assert(meters==saved);
 std::cout<<"PASS actual busy UI: 30 atomic RMS redraws over 3s connection wait, header heartbeat, sleep/menu/recording guards\n";
}
'''
 (out/'test.cpp').write_text(test,encoding='utf-8');host=root/'.build-host';host.mkdir(exist_ok=True);env=os.environ.copy()
 for k,v in [('TEMP','tmp'),('TMP','tmp'),('ZIG_GLOBAL_CACHE_DIR','global-cache'),('ZIG_LOCAL_CACHE_DIR','local-cache')]:(host/v).mkdir(parents=True,exist_ok=True);env[k]=str(host/v)
 subprocess.run([*host_compiler(),'-std=c++17','-I',str(root/'tools/host-tests'),str(out/'test.cpp'),'-o',str(out/'test.exe')],env=env,check=True)
 p=subprocess.run([str(out/'test.exe')],capture_output=True,text=True,check=True);(out/(fn+'.log')).write_text(p.stdout,encoding='utf-8');print(folder.name,p.stdout)
