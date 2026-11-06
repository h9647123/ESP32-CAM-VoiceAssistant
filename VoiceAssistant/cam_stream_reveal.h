#pragma once
#include <stdint.h>
#include <stddef.h>

// The received answer remains the single source of truth. This cursor only
// paces presentation, never network reads or the history/context content.
class CamStreamReveal {
  uint32_t last_=0;bool started_=false;
public:
  size_t next(const char* text,size_t length,size_t from,uint32_t now,bool skip=false,bool ending=false) {
    if(from>=length || skip)return length;
    const uint32_t elapsed=started_?uint32_t(now-last_):18;
    if(started_ && elapsed<18)return from;
    last_=now;started_=true;
    unsigned quota=(elapsed>90?90:elapsed)+17;quota/=18;
    if(!quota)quota=1;
    const size_t backlog=length-from;
    unsigned catchup=backlog>192?unsigned((backlog+95)/96):1;
    if(catchup>32)catchup=32;
    if(ending && catchup<unsigned((backlog+23)/24))catchup=unsigned((backlog+23)/24);
    if(catchup>96)catchup=96;
    if(quota<catchup)quota=catchup;
    size_t at=from;unsigned visible=0;
    while(at<length && visible<quota) {
      const uint8_t lead=uint8_t(text[at]);
      const size_t bytes=lead<0x80?1:lead<0xe0?2:lead<0xf0?3:4;
      if(at+bytes>length)break;
      bool valid=true;for(size_t i=1;i<bytes;++i)if((uint8_t(text[at+i])&0xc0)!=0x80)valid=false;
      if(!valid)break;
      at+=bytes;
      if(lead>=0x20 || lead=='\n')++visible;
    }
    return at;
  }
};
