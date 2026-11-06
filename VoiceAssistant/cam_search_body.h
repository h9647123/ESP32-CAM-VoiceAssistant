#pragma once
#include <Arduino.h>
static constexpr size_t CAM_SEARCH_BODY_LIMIT=12288;
static constexpr uint32_t CAM_SEARCH_BODY_TIMEOUT_MS=15000;
// HTTPClient decodes chunked transfer into this sink. A short write aborts
// oversized/slow responses, rather than allocating an unbounded getString().
class CamSearchBody:public Stream {
 public:
  String text;
  bool rejected=false;
  bool (*cancelled)()=nullptr;
  uint32_t budgetMs=CAM_SEARCH_BODY_TIMEOUT_MS;
  uint32_t started=millis();
  size_t write(uint8_t byte)override{return write(&byte,1);}
  size_t write(const uint8_t* bytes,size_t count)override{
    if(rejected || (cancelled && cancelled()) || millis()-started>budgetMs || count>CAM_SEARCH_BODY_LIMIT-text.length()){
      rejected=true;return 0;
    }
    if(!text.concat((const char*)bytes,count)){rejected=true;return 0;}
    yield();return count;
  }
  int available()override{return 0;}
  int read()override{return -1;}
  int peek()override{return -1;}
  void flush()override{}
};
