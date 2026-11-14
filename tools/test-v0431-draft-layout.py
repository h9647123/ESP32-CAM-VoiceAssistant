from host_toolchain import host_compiler
"""Compare cached production layout against full-rebuild reference after mutations."""
from pathlib import Path
import os,subprocess
from source_blocks import get
root=Path(__file__).resolve().parents[1];out=root/'validation/20261114-v0431/draft-layout';out.mkdir(parents=True,exist_ok=True)
s=(root/'VoiceAssistant/VoiceAssistant.ino').read_text(encoding='utf-8');fn=get(s,'rebuildChatLines');start=fn.index('  // Only the live draft');end=fn.index('  if (transientSearchResult.length())',start)
reference=fn[:start]+'''  chatRoundStarts.clear();chatLines.clear();chatLineRoles.clear();
  for(const auto& msg:chatHistory){if(msg.role=="user")chatRoundStarts.push_back(chatLines.size());appendChatMessageLines(msg,chatLines,chatLineRoles);}
'''+fn[end:];reference=reference.replace('rebuildChatLines()','rebuildBaseline()')
code=r'''#include "Arduino.h"
#include <cassert>
#include <iostream>
struct Image{String path;size_t bytes=1;};
struct ChatMsg{String role,content;uint32_t thinkingMs=0;bool thinkingTimed=false,thinkingComplete=false,thinkingActive=false;std::vector<Image> images;};
static std::vector<ChatMsg> chatHistory;static std::vector<String> chatLines;static std::vector<uint8_t> chatLineRoles;static std::vector<size_t> chatRoundStarts;
static struct {int uiLanguage=0;bool showThinking=true;}cfg;
static unsigned CHAT_TEXT_MAX_WIDTH=208,calls=0;
static String transientSearchResult,transcript,pendingQuestion,lastError;
static bool composingQuestion=true,recording=false,asrSessionActive=false;
enum class AppState{Idle,Error};static AppState appState=AppState::Idle;
static String uiText(const char*,const char* en,const char*){return en;}
static String uiErrorText(const String& s){return s;}static String normalizeMarkdownBody(const String& s){return s;}
static void wrapIntoLines(const String& s,unsigned,std::vector<String>& out){out.push_back(s);}
static void appendChatMessageLines(const ChatMsg& m,std::vector<String>& lines,std::vector<uint8_t>& roles){++calls;lines.push_back(m.role+":"+m.content);roles.push_back(1);lines.push_back("");roles.push_back(0);}
'''+fn+'\n'+reference+r'''
static void compare(){rebuildChatLines();auto rows=chatLines;auto roles=chatLineRoles;auto rounds=chatRoundStarts;rebuildBaseline();assert(rows==chatLines&&roles==chatLineRoles&&rounds==chatRoundStarts);}
int main(){
 for(int i=0;i<100;++i){ChatMsg m;m.role=i%2?"assistant":"user";m.content="long formula "+String(i);chatHistory.push_back(m);}
 transcript="first";rebuildChatLines();calls=0;
 for(unsigned i=0;i<100;++i){transcript="live word "+String(i);rebuildChatLines();}assert(calls==0);
 compare();chatHistory[1].content="same size replacement";compare();auto count=calls;chatHistory[1].content="same size replacemenX";rebuildChatLines();assert(calls==count+100);compare();
 cfg.showThinking=false;compare();cfg.uiLanguage=1;compare();CHAT_TEXT_MAX_WIDTH=180;compare();chatHistory[0].thinkingMs=7000;compare();chatHistory[0].thinkingActive=true;compare();chatHistory[0].images.push_back({"P-first.jpg",10});compare();chatHistory[0].images[0].path="P-other.jpg";compare();
 for(unsigned i=0;i<100;++i){transcript=i%3?"word":"";pendingQuestion=i%2?"pending":"";transientSearchResult=i%5?"":"sources";lastError=i%7?"":"error";appState=i%7?AppState::Idle:AppState::Error;compare();}
 chatHistory.clear();compare();transcript="";pendingQuestion="";transientSearchResult="";lastError="";compare();
 puts("PASS actual cached layout equals full rebuild across 100 overlay mutations; unchanged 100-message history parsed zero times across 100 interim updates; same-length edits/language/thinking/width/photos invalidate");
}
'''
(out/'test.cpp').write_text(code,encoding='utf-8');host=root/'.build-host';host.mkdir(exist_ok=True);env=os.environ.copy()
for k,v in [('TEMP','tmp'),('TMP','tmp'),('ZIG_GLOBAL_CACHE_DIR','global-cache'),('ZIG_LOCAL_CACHE_DIR','local-cache')]:(host/v).mkdir(parents=True,exist_ok=True);env[k]=str(host/v)
subprocess.run([*host_compiler(),'-std=c++17','-I',str(root/'tools/host-tests'),str(out/'test.cpp'),'-o',str(out/'test.exe')],env=env,check=True)
p=subprocess.run([str(out/'test.exe')],capture_output=True,text=True,check=True);(out/'tests.log').write_text(p.stdout,encoding='utf-8');print(p.stdout)
