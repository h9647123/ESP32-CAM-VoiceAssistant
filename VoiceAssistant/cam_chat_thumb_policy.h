#pragma once
#include <Arduino.h>
static constexpr unsigned CAM_CHAT_THUMB_LIMIT=3;
static constexpr int CAM_CHAT_THUMB_WIDTH=62,CAM_CHAT_THUMB_HEIGHT=47;
static constexpr int CAM_CHAT_THUMB_GAP=5,CAM_CHAT_THUMB_ROW_HEIGHT=66;
static constexpr int CAM_CHAT_THUMB_ROW_WIDTH=196;
static constexpr uint8_t CAM_CHAT_THUMB_ROLE=5;
static constexpr char CAM_CHAT_THUMB_MARKER=6;
static bool camChatThumbLine(const String& line,size_t& message,unsigned& band) {
  if(line.length()<4 || line[0]!=CAM_CHAT_THUMB_MARKER)return false;
  const int colon=line.indexOf(':',1);if(colon<2 || colon!=int(line.length())-2)return false;
  message=0;
  for(int i=1;i<colon;++i){if(line[i]<'0'||line[i]>'9'||message>100000)return false;message=message*10+unsigned(line[i]-'0');}
  if(line[colon+1]<'0'||line[colon+1]>'2')return false;
  band=unsigned(line[colon+1]-'0');return true;
}
// Read JPEG SOF dimensions in a bounded header scan, without loading its body.
template<class Reader>
static bool camChatThumbJpegSize(Reader& file,uint16_t& width,uint16_t& height) {
  if(file.size()<4 || !file.seek(0) || file.read()!=255 || file.read()!=216)return false;
  for(unsigned segment=0;segment<128 && file.position()<65536;++segment) {
    if(file.read()!=255)return false;
    int marker;do{marker=file.read();}while(marker==255 && file.position()<65536);
    if(marker<0 || marker==0xda || marker==0xd9)return false;
    if(marker==1 || (marker>=0xd0&&marker<=0xd8))continue;
    const int a=file.read(),b=file.read();if(a<0||b<0)return false;
    const size_t size=(size_t(a)<<8)|size_t(b);
    if(size<2 || file.position()>file.size() || size-2>file.size()-file.position())return false;
    if(marker==0xc0) {
      if(size<8 || file.read()!=8)return false;
      int h1=file.read(),h2=file.read(),w1=file.read(),w2=file.read();
      if(h1<0||h2<0||w1<0||w2<0)return false;
      height=(uint16_t(h1)<<8)|h2;width=(uint16_t(w1)<<8)|w2;
      return width>0&&height>0&&width<=4096&&height<=4096;
    }
    if(!file.seek(file.position()+size-2))return false;
  }
  return false;
}
