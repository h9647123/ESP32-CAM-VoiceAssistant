from host_toolchain import host_compiler, arduinojson_include
"""Exercise the actual chat viewport entry point as streamed rows arrive."""
from pathlib import Path
import os,subprocess
from source_blocks import get
root=Path(__file__).resolve().parents[1]
out=root/'validation/20261107-v0423/bottom-follow';out.mkdir(parents=True,exist_ok=True)
s=(root/'VoiceAssistant/VoiceAssistant.ino').read_text(encoding='utf-8')
code=r"""
#include <cassert>
#include <cstdint>
#include <vector>
#include <cstdio>
static int PAGE1_HEADER_BOTTOM=40,CHAT_AREA_TOP=42,CHAT_VISIBLE_LINES=7,CHAT_LINE_HEIGHT=22;
static int TFT_BLACK=0,camCandidateGeneration=0;
static int32_t chatScrollPixels=0,chatScrollTargetPixels=0,drawn=0;
static size_t chatScroll=0;
static bool chatAutoScrollBottom=true,chatLinesDirty=false,chatStreamRendering=true;
static std::vector<int> chatLines;
struct Photo{bool confirmed=false;bool ready(){return false;}}camPhoto;
struct Sprite{void deleteSprite(){}}camCandidateSprite;
struct Tft{void fillRect(int,int,int,int,int){}}tft;
static void applyChatLayout(){} static void camDrawCandidate(){} static void rebuildChatLines(){}
static void setChatScrollRow(size_t n){chatScroll=n;chatScrollPixels=chatScrollTargetPixels=n*CHAT_LINE_HEIGHT;}
static void syncChatScrollRowsFromPixels(){chatScroll=chatScrollPixels/CHAT_LINE_HEIGHT;}
static void drawChatWindowAtPixel(int32_t p){drawn=p;} static void drawScrollbar(){}
"""+get(s,'drawChatWindow')+r"""
int main(){
 for(unsigned n=0;n<500;++n){chatLines.resize(n);drawChatWindow(false);
  const int expected=(n>7?n-7:0)*22;assert(drawn==expected&&chatScrollPixels==expected&&chatScrollTargetPixels==expected);
  drawChatWindow(false);assert(drawn==expected); // idle updates never leave a pending scroll
 }
 chatAutoScrollBottom=false;chatScrollPixels=33;chatScrollTargetPixels=66;
 chatLines.resize(500);drawChatWindow(false);assert(drawn==33&&chatScrollTargetPixels==66);
 puts("PASS: 500 streamed line counts pin the newest row in the same draw; idle frames stay pinned; manual scroll remains untouched.");
}
"""
(out/'bottom.cpp').write_text(code,encoding='utf-8')
host=root/'.build-host';host.mkdir(exist_ok=True);env=os.environ.copy()
for key,folder in [('TEMP','tmp'),('TMP','tmp'),('ZIG_GLOBAL_CACHE_DIR','global-cache'),('ZIG_LOCAL_CACHE_DIR','local-cache')]:(host/folder).mkdir(parents=True,exist_ok=True);env[key]=str(host/folder)
exe=out/'bottom.exe'
r=subprocess.run([*host_compiler(),'-std=c++17',str(out/'bottom.cpp'),'-o',str(exe)],env=env,capture_output=True,text=True)
if r.returncode:print(r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(exe)],capture_output=True,text=True);(out/'tests.log').write_text(r.stdout+r.stderr,encoding='utf-8');print(r.stdout);raise SystemExit(r.returncode)
