from host_toolchain import host_compiler, arduinojson_include
"""Exercise actual normalization, wrapping and draw code with host font metrics.

This checks geometry/state, not real-panel font rasterization or readability.
"""
from pathlib import Path
import os,subprocess,json,re
from source_blocks import get
root=Path(__file__).resolve().parents[1];out=Path(os.environ.get('CAM_TEST_OUTPUT',str(root/'validation/20261107-v0420/render')));out.mkdir(parents=True,exist_ok=True)
source=(root/'VoiceAssistant/VoiceAssistant.ino').read_text(encoding='utf-8')
names=['utf8CharLen','utf8DecodeAt','richDelimiterEscaped','isRichEscapableMarkdownChar','findRichLinkLabelEnd','findRichLinkSpan','normalizeRichText','asciiDigit','asciiAlpha','asciiAlnum','unicodeSmallDigitToAscii','isMathScriptContext','isCompactMathScript','mathScriptSpan','richFractionSpan','richPlainText','richTextWidth','drawTextPiece','drawRichTextLine','wrapIntoLines','findRichBoldClose','markdownStablePrefixLength','camStreamSafePrefix']
blocks={n:get(source,n) for n in names}
prefix=r'''
#include "Arduino.h"
#include <cassert>
#include <vector>
#include <iostream>
#include <cmath>
#define PROGMEM
#include "latex_display.h"
#include "markdown_table.h"
#include "cam_math_samples.h"
#include "cam_rich_styles.h"
static constexpr char RICH_BOLD_TOGGLE=1,RICH_HEADING_TOGGLE=2,RICH_THINKING_TOGGLE=3,RICH_CODE_TOGGLE=4,RICH_LITERAL_ESCAPE=5;
static constexpr uint16_t TFT_BLACK=0;
namespace fonts{struct Font{int height;};static Font efontCN_10{10},efontCN_12{12},efontCN_16{16};}
struct Mark{int x,y,w,h;uint16_t color;};
namespace lgfx{
class LGFXBase{
 public:std::vector<Mark> marks;float scale=1;int x=0,y=0;uint16_t color=1;const fonts::Font* font=&fonts::efontCN_16;
 int32_t cx=0,cy=0,cw=4096,ch=4096;
 void getClipRect(int32_t* a,int32_t* b,int32_t* c,int32_t* d){*a=cx;*b=cy;*c=cw;*d=ch;}
 void setClipRect(int32_t a,int32_t b,int32_t c,int32_t d){cx=a;cy=b;cw=c;ch=d;}
 void mark(int a,int b,int w,int h,uint16_t c){int r=std::min(a+w,int(cx+cw)),bot=std::min(b+h,int(cy+ch));a=std::max(a,int(cx));b=std::max(b,int(cy));if(r>a&&bot>b)marks.push_back({a,b,r-a,bot-b,c});}
 void setTextSize(float v){scale=v;}void setFont(const fonts::Font* f){font=f;}
 int textWidth(const String& s,const fonts::Font* f=nullptr){int width=0;f=f?f:font;
  for(size_t i=0;i<s.length();){uint8_t c=s[i];unsigned n=c<128?1:(c<224?2:(c<240?3:4));width+=c<128?f->height/2:f->height;i+=n;}return int(width*scale);}
 void setTextColor(uint16_t c,uint16_t=0){color=c;}void setCursor(int a,int b){x=a;y=b;}
 void print(const String& s){const int w=textWidth(s);if(w)mark(x,y,w,int(font->height*scale),color);x+=w;}
 void drawPixel(int a,int b,uint16_t c){mark(a,b,1,1,c);}
 void fillRect(int a,int b,int w,int h,uint16_t c){mark(a,b,w,h,c);}
 void drawFastHLine(int a,int b,int w,uint16_t c){mark(a,b,w,1,c);}
};}
class LGFX_Sprite:public lgfx::LGFXBase{};static lgfx::LGFXBase tft;
struct UiStyle{uint16_t thinking=0x7777,heading=0x1111,emphasis=0x2222;};static UiStyle currentStyle(){return {};}
static String makeDisplaySafe(const String& s){return s;}
static bool mathScriptSpan(const String&,size_t,String&,size_t&);
#include "science_glyphs.h"
'''
ordered=['utf8CharLen','utf8DecodeAt']
code=prefix+'\n'.join(blocks[n] for n in ordered)+'\n#include "science_renderer.h"\n'
code+='\n'.join(blocks[n] for n in names if n not in ordered)
aliases=re.findall(r'\{"([A-Za-z]+)","([^"]*)"\}',(root/'VoiceAssistant/latex_display.h').read_text(encoding='utf-8'))
checks='\n'.join(' {std::string out;size_t at=0;assert(latexDisplayToken('+json.dumps('\\'+name,ensure_ascii=False)+', '+str(len(name)+1)+',at,out,0));assert(out=='+json.dumps(value,ensure_ascii=False)+');}' for name,value in aliases)
glyphs=sorted(set(''.join(v for _,v in aliases)+'ℝℕℤℚℂ'))
glyph_checks=''.join('assert(scienceGlyph('+str(ord(c))+'));' for c in glyphs if ord(c)>127)
code+='static void testAllAliases(){'+checks+glyph_checks+'}\n'
code+=r'''
int main(){
 testAllAliases();
 assert(normalizeRichText("````markdown\n# Heading\n*italic*\n````").indexOf(char(16))>=0);
 assert(normalizeRichText(R"(\sqrt3)").indexOf("\\sqrt")<0);
 assert(normalizeRichText(R"(30^\circ 60^{\circ})").indexOf('^')<0);
 {String t=normalizeRichText("# One\n## Two\n*italic* ~~gone~~\n> quote\n> > nested");assert(t.indexOf(char(16))>=0&&t.indexOf(char(17))>=0&&t.indexOf(char(18))>=0&&t.indexOf(char(19))>=0);}

 unsigned lines=0;
 for(unsigned page=1;page<=14;++page){String normalized=normalizeRichText(camMathSample(page));std::vector<String> rows;wrapIntoLines(normalized,208,rows);
  for(const auto& row:rows){if(richTextWidth(row)>208){std::cerr<<"overwide page="<<page<<" width="<<richTextWidth(row)<<" row="<<row.c_str()<<"\n";return 1;}
   LGFX_Sprite sprite;drawRichTextLine(&sprite,0,0,row,0x5555);
   for(const auto& mark:sprite.marks){if(mark.y<0||mark.y+mark.h>22||mark.x<0||mark.x+mark.w>210){std::cerr<<"bleed page="<<page<<" x="<<mark.x<<" y="<<mark.y<<" w="<<mark.w<<" h="<<mark.h<<" row="<<row.c_str()<<"\n";return 1;}}
   ++lines;
  }
 }
 for(const String input:{String(R"(\frac ab)"),String(R"(\frac12)"),String(R"(\frac{x+1}2)"),String(R"(\frac\alpha\beta)"),String(R"(\dfrac a{b+c})")}) {
   String value=normalizeRichText(input);assert(value.indexOf("\\frac")<0&&value.indexOf("\\dfrac")<0&&value.indexOf(char(14))>=0);
 }
 assert(camStreamSafePrefix("\\frac a",7,false)==0);
 assert(camStreamSafePrefix("\\frac12",3,false)==7);
 String chemistry=normalizeRichText(R"(\ce{2H2 + O2 -> 2H2O})");assert(chemistry.indexOf("H_{2}")>=0&&chemistry.indexOf("→")>=0);
 String fraction=normalizeRichText(R"(\frac{x+1}{x-1})");String top,bottom;size_t end=0;assert(richFractionSpan(fraction,0,top,bottom,end)&&top=="x+1"&&bottom=="x-1");
 assert(normalizeRichText(R"(\boxed{a\in(-\infty,-1]})").indexOf("\\boxed")<0);
 assert(normalizeRichText(R"(\frac{x}{)").indexOf("\\frac")>=0); // streaming incomplete group is retained
 String scripts;size_t next=0;assert(mathScriptSpan("SO_4^{2-}",2,scripts,next)&&scripts=="4");assert(!mathScriptSpan("GPIO_2",4,scripts,next));

 for(const String text:{String("2OH^-"),String("2H_{2}"),String("2NH_{4}^{+}")}) {
   const int marker=text.indexOf('_')>=0?text.indexOf('_'):text.indexOf('^');
   assert(mathScriptSpan(text,marker,scripts,next));
 }
 assert(mathScriptSpan("Fe^3+",2,scripts,next)&&scripts=="3+"&&next==5);
 assert(mathScriptSpan("2OH^-",3,scripts,next)&&scripts=="-"&&next==5);
 assert(normalizeRichText(R"(\boxed{{a}\in(-\infty,-1]})")==String("a∈(-∞,-1]"));
 assert(normalizeRichText(R"(\(\{x\in\mathbb{R}\}\))")==String("{x∈ℝ}"));
 assert(normalizeRichText(R"(${x}+{y}$)")==String("x+y"));
 assert(normalizeRichText(R"(\ce{2OH^- + 2H^+ -> 2H2O})").indexOf("\\")<0);
 assert(normalizeRichText(R"(\leftrightarrow\rightleftharpoons)")==String("↔⇌"));
 assert(normalizeRichText("`x^{2}`").indexOf("{2}")>=0); // code keeps literal syntax
 assert(normalizeRichText(R"(\frac{x^2+1}{2})").indexOf(char(14))>=0); // expanded script-aware fraction
 {std::vector<String> rows;wrapIntoLines(normalizeRichText("```text\n\\frac{f(x_1)-f(x_2)}{x_1-x_2}\n=1-x_1-x_2\n```"),208,rows);
  assert(rows.size()==4);for(int b=0;b<3;++b){assert(rows[b][0]==char(20)&&rows[b][1]=='0'+b);assert(rows[b].indexOf("f(x_1)-f(x_2)")>=0&&rows[b].indexOf("x_1-x_2")>=0);}assert(rows[3]=="=1-x_1-x_2");}
 {std::vector<String> rows;wrapIntoLines(normalizeRichText("```text\n[f(x₁)-f(x₂)] / (x₁-x₂)\n=1-x₁-x₂\n```"),208,rows);assert(rows.size()==4&&rows[0][0]==char(20)&&rows[2][1]=='2');}
 {std::vector<String> rows;wrapIntoLines(normalizeRichText(R"(\frac{x+1}{x-1}=\frac32)"),208,rows);assert(rows.size()==2);assert(rows[0].substring(2)==rows[1].substring(2));assert(rows[0].indexOf('=')>=0);for(const auto& row:rows){LGFX_Sprite s;drawRichTextLine(&s,0,0,row,0x5555);assert(!s.marks.empty());for(auto m:s.marks)assert(m.y>=0&&m.y+m.h<=22);assert(s.cy==0&&s.ch==4096);}}
 assert(camDisplayPlainFraction("a+b/c")=="a+b/c");assert(camDisplayPlainFraction("(a)/(b)+c")=="(a)/(b)+c");
 {const auto code=normalizeRichText("```cpp\nGPIO_2 = x^2;\n```");assert(code.indexOf("cpp")<0&&code.indexOf("GPIO_2 = x^2;")>=0&&code.indexOf(char(4))>=0);}
 {std::vector<String> rows;wrapIntoLines(normalizeRichText("[f(x_1)-f(x_2)] / (x_1-x_2)"),155,rows);for(const auto& row:rows)assert(row!="2)"&&row!=")");}
 assert(camStreamSafePrefix(R"(开始\frac{x}{2}结束)",8,false)==String("开始\\frac{x}{2}").length());
 assert(camStreamSafePrefix(R"(\frac{x}{)",5,false)==0);
 auto table=camMarkdownTables("| A | B |\n|---|---|\n|甲|乙|\n");assert(table.find("A: 甲")!=table.npos&&table.find("B: 乙")!=table.npos);
 assert(camMarkdownTables("```\n| A | B |\n|---|---|\n```\n").find("|---|---|")!=std::string::npos);
 {String thought=String(char(3))+"# **灰色标题**\n\n换行思考\n"+String(char(3))+"蓝色正文";std::vector<String> rows;wrapIntoLines(normalizeRichText(thought),208,rows);
  LGFX_Sprite sprite;drawRichTextLine(&sprite,0,0,rows[0],0x5555);assert(!sprite.marks.empty());for(auto m:sprite.marks)assert(m.color==0x7777);
  assert(rows[1].length()&&rows[1][0]==char(3));}
 std::cout<<"PASS: "<<lines<<" actual normalized/wrapped/drawn math/chemistry/Markdown rows; all command aliases and symbol bitmaps; 22px bounds; heading width; thinking priority; fractions; paired scripts; table code fences\n";
}
'''
(out/'render.cpp').write_text(code,encoding='utf-8')
host=root/'.build-host';host.mkdir(exist_ok=True);env=os.environ.copy()
for key,folder in [('TEMP','tmp'),('TMP','tmp'),('ZIG_GLOBAL_CACHE_DIR','global-cache'),('ZIG_LOCAL_CACHE_DIR','local-cache')]:(host/folder).mkdir(parents=True,exist_ok=True);env[key]=str(host/folder)
exe=out/'render-tests.exe'
args=[*host_compiler(),'-std=c++17','-I',str(root/'tools/host-tests'),'-I',str(root/'VoiceAssistant'),str(out/'render.cpp'),'-o',str(exe)]
r=subprocess.run(args,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/'compile.log').write_text(r.stdout+r.stderr,encoding='utf-8')
if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(exe)],env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/'tests.log').write_text(r.stdout+r.stderr,encoding='utf-8');print(r.stdout+r.stderr);raise SystemExit(r.returncode)
