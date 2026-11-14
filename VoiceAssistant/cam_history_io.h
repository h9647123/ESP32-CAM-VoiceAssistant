#pragma once
#include <stddef.h>
#include <stdint.h>
// A tail window must start at the START of its overlapping JSONL record.
// Seeking to the next newline discards a last record larger than the window.
template<class Source> static bool camHistoryWindowStart(Source& file,size_t window,size_t& start) {
  start=0;if(file.size()<=window)return file.seek(0);
  size_t end=file.size()-window;uint8_t block[512];
  while(end) {
    const size_t begin=end>sizeof(block)?end-sizeof(block):0,n=end-begin;
    if(!file.seek(begin) || file.read(block,n)!=n)return false;
    for(size_t i=n;i>0;--i)if(block[i-1]=='\n'){start=begin+i;return file.seek(start);}
    end=begin;
  }
  return file.seek(0);
}
// Stream exactly one physical JSONL row into a PSRAM-backed JsonDocument.
// No internal-RAM String proportional to the whole archive record is required.
template<class Source> class CamHistoryLineReader {
  Source& file;size_t count=0,limit;bool ended=false;
 public:
  bool hasData=false,overflow=false;
  explicit CamHistoryLineReader(Source& f,size_t maxBytes):file(f),limit(maxBytes){}
  int read(){if(ended)return -1;int ch=file.read();if(ch<0||ch=='\n'){ended=true;return -1;}
    if(++count>limit){overflow=true;return -1;}if(ch!=' '&&ch!='\t'&&ch!='\r')hasData=true;return ch;}
  size_t readBytes(char* dst,size_t n){size_t i=0;for(;i<n;++i){int ch=read();if(ch<0)break;dst[i]=char(ch);}return i;}
  bool finish(){bool clean=true;while(!ended){int ch=read();if(ch>=0 && ch!=' '&&ch!='\t'&&ch!='\r')clean=false;
    if(overflow){while(file.available() && file.read()!='\n'){}ended=true;return false;}}
    return clean&&!overflow;}
};
static bool camHistoryUtf8Valid(const char* text,size_t length) {
  for(size_t i=0;i<length;){uint8_t c=text[i++];if(c<128)continue;unsigned count;uint32_t value,min;
    if(c>=0xc2&&c<=0xdf){count=1;value=c&31;min=0x80;}
    else if(c>=0xe0&&c<=0xef){count=2;value=c&15;min=0x800;}
    else if(c>=0xf0&&c<=0xf4){count=3;value=c&7;min=0x10000;}else return false;
    if(i+count>length)return false;while(count--){uint8_t x=text[i++];if((x&0xc0)!=0x80)return false;value=(value<<6)|(x&63);}
    if(value<min||value>0x10ffff||(value>=0xd800&&value<=0xdfff))return false;
  }return true;
}
// Fold only the DISPLAY of unusually long thinking. The original message is
// untouched for storage/export; final answer text is never truncated here.
static String camHistoryDisplayText(const char* text,size_t length,bool showThinking,const String& folded) {
  String out;out.reserve(length<12288?length:12288);bool thinking=false,noted=false;size_t kept=0;
  for(size_t at=0;at<length;){const uint8_t ch=text[at];
    if(ch==3){thinking=!thinking;if(showThinking)out+=char(3);++at;continue;}
    size_t bytes=ch<0x80?1:ch<0xe0?2:ch<0xf0?3:4;if(at+bytes>length)bytes=1;
    if(ch==5 && at+1<length)bytes=2;
    if(!thinking || (showThinking && kept+bytes<=8192))out.concat(text+at,bytes);
    else if(showThinking&&!noted){out+='\n';out+=folded;out+='\n';noted=true;}
    if(thinking)kept+=bytes;at+=bytes;
  }return out;
}
