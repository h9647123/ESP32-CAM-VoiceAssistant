#include "Arduino.h"
#include "cam_chat_thumb_policy.h"
#include "cam_rich_state.h"
#include <vector>
#include <cassert>
#include <iostream>
struct Reader{std::vector<uint8_t> data;size_t at=0;size_t size(){return data.size();}size_t position(){return at;}bool seek(size_t n){at=n;return n<=size();}int read(){return at<size()?data[at++]:-1;}};
struct ChatMsg{String role,content;std::vector<int> images;uint32_t thinkingMs=0;bool thinkingTimed=false,thinkingComplete=false,thinkingActive=false;};
static std::vector<ChatMsg> chatHistory;
struct{bool showThinking=true;}cfg;
static constexpr int CHAT_TEXT_MAX_WIDTH=208;
static constexpr char RICH_THINKING_TOGGLE=3,RICH_LITERAL_ESCAPE=5;
static String uiText(const char* a,const char*,const char*){return a;}
static String stripDisplayMarkers(const String& s){return s;}
static String normalizeMarkdownBody(const String& s){return s;}
static String richPlainText(const String& s){return s;}
static bool lineHasThinkingVisibleText(const String&){return false;}
static void wrapIntoLines(const String& s,int,std::vector<String>& lines){lines.push_back(s);}
#include "thumb_lines_under_test.h"
int main(){
 assert(CAM_CHAT_THUMB_ROW_WIDTH==3*CAM_CHAT_THUMB_WIDTH+2*CAM_CHAT_THUMB_GAP);
 assert(CAM_CHAT_THUMB_ROW_WIDTH<=208&&CAM_CHAT_THUMB_ROW_HEIGHT==66);
 for(int count=0;count<=4;++count){chatHistory={{"user","question",std::vector<int>(count,1)}};std::vector<String> lines;std::vector<uint8_t> roles;appendChatMessageLines(chatHistory[0],lines,roles);assert(lines.size()==size_t(count?5:2)&&lines.size()==roles.size());
  if(count)for(unsigned b=0;b<3;++b){size_t message=999;unsigned band=999;assert(roles[1+b]==5&&camChatThumbLine(lines[1+b],message,band)&&message==0&&band==b);}
  assert(roles.back()==0);
 }
 {ChatMsg timed;timed.role="assistant";timed.content="正文";timed.thinkingTimed=timed.thinkingComplete=true;timed.thinkingMs=12000;
  std::vector<String> lines;std::vector<uint8_t> roles;appendChatMessageLines(timed,lines,roles);
  assert(lines.size()==3&&roles[0]==4&&roles[1]==2&&lines[0].indexOf("12s")>=0&&lines[1]=="正文");}
 size_t message;unsigned band;assert(!camChatThumbLine("plain",message,band));assert(!camChatThumbLine(String(char(6))+"-1:0",message,band));assert(!camChatThumbLine(String(char(6))+"1:3",message,band));
 Reader jpeg{{255,216,255,224,0,4,1,2,255,192,0,11,8,4,176,6,64,1,1,17,0,255,217}};
 uint16_t w=0,h=0;assert(camChatThumbJpegSize(jpeg,w,h)&&w==1600&&h==1200);
 auto bad=jpeg;bad.data[12]=16;assert(!camChatThumbJpegSize(bad,w,h));
 bad=jpeg;bad.data[10]=255;assert(!camChatThumbJpegSize(bad,w,h));
 bad=jpeg;bad.data[9]=194;assert(!camChatThumbJpegSize(bad,w,h));
 bad=jpeg;bad.data.resize(12);assert(!camChatThumbJpegSize(bad,w,h));
 std::cout<<"PASS: actual user-message thumbnail bands, row/role alignment, three-card geometry, marker validation, bounded JPEG header checks\n";
}
