from host_toolchain import host_compiler, arduinojson_include
from pathlib import Path
import os,sys,subprocess
from source_blocks import get
root=Path(__file__).resolve().parents[1];out=Path(os.environ.get('CAM_TEST_OUTPUT',str(root/'validation/20261107-v0427/scroll')));out.mkdir(parents=True,exist_ok=True)
s=(root/'VoiceAssistant/VoiceAssistant.ino').read_text(encoding='utf-8')
code=r'''
#include "Arduino.h"
#include <vector>
#include <cassert>
#include <iostream>
static constexpr uint16_t TFT_BLACK=0,TFT_DARKGREY=4,TFT_LIGHTGREY=7,TFT_RED=9;
namespace fonts {static int efontCN_16;}
struct LGFX_Sprite {
 uint16_t color565(int r,int g,int b){return uint16_t((r>>3)<<11|(g>>2)<<5|(b>>3));}
 bool allocated=false;void setPsram(bool){}void setColorDepth(int){}void* createSprite(int,int){allocated=true;return this;}
 void fillSprite(int){}void setFont(const int*){}void setTextWrap(bool){}void setTextSize(int){}void setTextColor(int,int){}void setCursor(int,int){}void print(const String&){}
 void fillRect(int,int,int,int,int){}void fillRoundRect(int,int,int,int,int,int){}void setClipRect(int,int,int,int){}void clearClipRect(){}
 void pushSprite(int,int){}void pushSprite(LGFX_Sprite*,int,int){}
};
static LGFX_Sprite tft,chatSprite;static bool chatSpriteReady=true,chatStreamRendering=false;
static int CHAT_LINE_HEIGHT=22,CHAT_VISIBLE_LINES=7,CHAT_AREA_TOP=42,CHAT_AREA_BOTTOM=200,CHAT_AREA_HEIGHT=158,CHAT_AREA_WIDTH=222;
static uint32_t chatRenderRevision=1;struct{int uiStyle=1;}cfg;
static std::vector<String> chatLines;static std::vector<uint8_t> chatLineRoles;
static constexpr int CAM_CHAT_THUMB_ROLE=99;
static int32_t maxChatScrollPixels(){return int32_t(chatLines.size()-CHAT_VISIBLE_LINES)*CHAT_LINE_HEIGHT;}
static int32_t clampInt(int32_t n,int32_t lo,int32_t hi){return max(lo,min(n,hi));}
static constexpr uint16_t TFT_CYAN=0x7ff,TFT_YELLOW=0xffe0,TFT_WHITE=0xffff,TFT_ORANGE=0xfd20;
static constexpr int UI_STYLE_COUNT=4;static int constrain(int v,int lo,int hi){return max(lo,min(v,hi));}
struct UiStyle{uint16_t title,time,date,menuSelected,menuText,user,assistant,thinking,heading,emphasis,status,track,thumb;};
THEME_CODE
static String uiText(const char*,const char* en,const char*){return en;}
static bool camChatThumbLine(const String&,size_t&,unsigned&){return false;}static void camChatThumbDrawBand(size_t,unsigned,int){}
static unsigned glyphDraws=0;static uint16_t lastColor=0;
static void drawRichTextLine(LGFX_Sprite*,int,int,const String&,uint16_t color){++glyphDraws;lastColor=color;}
'''+get(s,'drawChatWindowAtPixel')+r'''
int main(){
 for(int theme=1;theme<=4;++theme){cfg.uiStyle=theme;assert(currentStyle().thinking==0xA514);}
 cfg.uiStyle=1;
 for(int i=0;i<100;++i){chatLines.push_back("thinking formula "+String(i));chatLineRoles.push_back(4);}
 drawChatWindowAtPixel(0);const unsigned first=glyphDraws;assert(first==7&&lastColor==0xA514);
 for(int pixel=1;pixel<22;++pixel)drawChatWindowAtPixel(pixel);
 assert(glyphDraws==first+1);drawChatWindowAtPixel(22);assert(glyphDraws==first+1);
 ++chatRenderRevision;drawChatWindowAtPixel(22);assert(glyphDraws==first+8);
 std::cout<<"PASS actual viewport: 21 sub-row scroll frames reuse existing rasters and build only one partially entering row; revision invalidates seven; thinking color preserved\n";
}
'''
code=code.replace('THEME_CODE',get(s,'themedStyle')+'\n'+get(s,'currentStyle'))
(out/'test.cpp').write_text(code,encoding='utf-8');host=root/'.build-host';host.mkdir(exist_ok=True);env=os.environ.copy()
for k,v in [('TEMP','tmp'),('TMP','tmp'),('ZIG_GLOBAL_CACHE_DIR','global-cache'),('ZIG_LOCAL_CACHE_DIR','local-cache')]:(host/v).mkdir(parents=True,exist_ok=True);env[k]=str(host/v)
subprocess.run([*host_compiler(),'-std=c++17','-I',str(root/'tools/host-tests'),str(out/'test.cpp'),'-o',str(out/'test.exe')],env=env,check=True)
p=subprocess.run([str(out/'test.exe')],capture_output=True,text=True);(out/'tests.log').write_text(p.stdout+p.stderr,encoding='utf-8');print(p.stdout+p.stderr);p.check_returncode()
