#pragma once
#include <Arduino.h>

// Decode only the first page-content string. Never buffer a complete webpage.
// HTTPClient removes chunk framing before writing here. A short write closes
// the response once a UTF-8-safe excerpt is available, including huge pages.
class CamWebFetchSink : public Stream {
 public:
  String text;
  bool ready=false, rejected=false, truncated=false;
  bool (*cancelled)()=nullptr;
  uint32_t started=millis(), timeoutMs=12000;
  static constexpr size_t LIMIT=1500, WIRE_LIMIT=131072;
  size_t write(uint8_t c) override { return write(&c,1); }
  size_t write(const uint8_t* data,size_t count) override {
    if(ready || rejected)return 0;
    for(size_t i=0;i<count;++i) {
      if(++wire_>WIRE_LIMIT || millis()-started>timeoutMs || (cancelled && cancelled())) {rejected=true;return i;}
      consume(data[i]);
      if(ready || rejected)return i+1;
    }
    yield();return count;
  }
  int available() override{return 0;}
  int read() override{return -1;}
  int peek() override{return -1;}
  void flush() override{}
 private:
  size_t wire_=0;
  int depth_=0;
  bool string_=false,key_=false,capture_=false,escape_=false;
  bool target_=false;
  char previous_=0;
  String keyText_;
  unsigned digits_=0;
  uint32_t unicode_=0,high_=0;
  void finish(bool cut) {
    truncated=cut;
    size_t n=min(text.length(),LIMIT);
    while(n<text.length() && n && (uint8_t(text[n])&0xc0)==0x80)--n;
    text.remove(n);ready=true;
  }
  void append(char c) {
    if(ready || rejected)return;
    if(capture_) {
      if(!c){rejected=true;return;}
      text+=c;
      if(text.length()>=LIMIT+4)finish(true);
    } else if(key_ && keyText_.length()<32)keyText_+=c;
  }
  void point(uint32_t cp) {
    if(cp>=0xd800 && cp<=0xdbff){if(high_)rejected=true;high_=cp;return;}
    if(cp>=0xdc00 && cp<=0xdfff){if(!high_){rejected=true;return;}cp=0x10000+((high_-0xd800)<<10)+(cp-0xdc00);high_=0;}
    else if(high_){rejected=true;return;}
    if(cp<0x80)append(char(cp));
    else if(cp<0x800){append(char(0xc0|(cp>>6)));append(char(0x80|(cp&63)));}
    else if(cp<0x10000){append(char(0xe0|(cp>>12)));append(char(0x80|((cp>>6)&63)));append(char(0x80|(cp&63)));}
    else {append(char(0xf0|(cp>>18)));append(char(0x80|((cp>>12)&63)));append(char(0x80|((cp>>6)&63)));append(char(0x80|(cp&63)));}
  }
  void consume(uint8_t c) {
    if(string_) {
      if(digits_) {
        int x=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
        if(x<0){rejected=true;return;}
        unicode_=(unicode_<<4)|unsigned(x);
        if(!--digits_)point(unicode_);
        return;
      }
      if(escape_) {
        escape_=false;
        if(c=='u'){digits_=4;unicode_=0;return;}
        if(c=='n')append('\n');else if(c=='r')append('\r');else if(c=='t')append('\t');
        else if(c=='b')append('\b');else if(c=='f')append('\f');
        else if(c=='"'||c=='/'||c=='\\')append(char(c));else rejected=true;
        return;
      }
      if(c=='\\'){escape_=true;return;}
      if(c=='"') {
        if(high_){rejected=true;return;}
        string_=false;
        if(capture_){finish(false);return;}
        if(key_)target_=(keyText_=="content" && depth_==1)||keyText_=="raw_content";
        previous_='"';return;
      }
      if(c<32){rejected=true;return;}
      append(char(c));return;
    }
    if(c==' '||c=='\r'||c=='\n'||c=='\t')return;
    if(c=='"') {
      string_=true;key_=previous_=='{'||previous_==',';
      capture_=previous_==':' && target_;
      keyText_="";escape_=false;return;
    }
    if(c=='{'||c=='[')++depth_;
    else if(c=='}'||c==']'){if(--depth_<0)rejected=true;}
    if(c!=':')target_=false;
    previous_=char(c);
  }
};
