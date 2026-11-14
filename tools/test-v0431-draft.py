from host_toolchain import host_compiler
"""Exercise draft UTF-8 erasing and double-click state from production headers."""
from pathlib import Path
import os,subprocess
from source_blocks import get,endblock
root=Path(__file__).resolve().parents[1];out=root/'validation/20261114-v0431/draft';out.mkdir(parents=True,exist_ok=True)
s=(root/'VoiceAssistant/VoiceAssistant.ino').read_text(encoding='utf-8');fn=get(s,'handleButtonC');i=fn.index('    if(click && appState!=AppState::Thinking');b=fn.index('{',i);block=fn[i:endblock(fn,b)]
code=r'''#include "Arduino.h"
#include <cassert>
#include <iostream>
#include "cam_draft_edit.h"
static CamDraftEraseGesture draftEraseGesture;
static std::vector<String> pendingSegments;static String pendingQuestion,transcript,asrBestTranscript,asrStableTranscript,asrPartialTranscript;
static bool composingQuestion=true,chatAutoScrollBottom=false;enum class AppState{Idle,Thinking,Recognizing};static AppState appState=AppState::Idle;
static uint32_t nowMs=1000;static uint32_t millis(){return nowMs;}
static struct{uint32_t load(){return 1;}}buttonUiGeneration;
static void markChatLinesDirty(){}static void drawPage1(){}static void drawPage1Footer(const String&){}
static String uiText(const char*,const char* e,const char*){return e;}
static void clickC(bool click){
'''+block+r'''
camEraseDraftCharacter(pendingSegments);
}
int main(){
 CamDraftEraseGesture g;assert(!g.click(1000,1,true));assert(g.click(1200,1,true));assert(!g.armed);
 assert(!g.click(2000,1,true));assert(!g.click(2351,1,true));assert(!g.click(2400,2,true));g.reset();assert(!g.click(2500,2,true));
 g.reset();assert(!g.click(100,1,false));assert(!g.click(200,1,false));g.reset();assert(!g.click(0xfffffff0u,1,true));assert(g.click(0x10,1,true));
 pendingSegments={"前一段","中文x"};clickC(true);assert(pendingSegments[1]=="中文");nowMs+=200;clickC(true);assert(pendingSegments.empty()&&pendingQuestion.length()==0&&transcript.length()==0&&!composingQuestion&&chatAutoScrollBottom);
 pendingSegments={"中"};draftEraseGesture.reset();composingQuestion=true;nowMs+=500;clickC(true);assert(pendingSegments.empty());composingQuestion=false;nowMs+=200;clickC(true);assert(!draftEraseGesture.armed);
 puts("PASS actual draft erase: UTF-8, multiple segments, whole-draft clear, last-character double click, timeout, generation, reset, timer wrap");
}
'''
(out/'test.cpp').write_text(code,encoding='utf-8');host=root/'.build-host';host.mkdir(exist_ok=True);env=os.environ.copy()
for k,v in [('TEMP','tmp'),('TMP','tmp'),('ZIG_GLOBAL_CACHE_DIR','global-cache'),('ZIG_LOCAL_CACHE_DIR','local-cache')]:(host/v).mkdir(parents=True,exist_ok=True);env[k]=str(host/v)
subprocess.run([*host_compiler(),'-std=c++17','-I',str(root/'tools/host-tests'),'-I',str(root/'VoiceAssistant'),str(out/'test.cpp'),'-o',str(out/'test.exe')],env=env,check=True)
p=subprocess.run([str(out/'test.exe')],capture_output=True,text=True,check=True);(out/'tests.log').write_text(p.stdout,encoding='utf-8');print(p.stdout)
