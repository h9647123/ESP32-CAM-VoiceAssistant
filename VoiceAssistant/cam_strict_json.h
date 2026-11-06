#pragma once
#include <stddef.h>
#include <stdint.h>

// ArduinoJson accepts and emits raw C0 rich-text markers in quoted strings.
// Escape them for portable JSONL without copying a whole history record in RAM.
template<class Destination> class CamStrictJsonWriter {
  Destination& destination;
  uint8_t buffer[256];size_t used=0,bytes=0;bool healthy=true;
  bool flush() {
    if(used && healthy)healthy=destination.write(buffer,used)==used;
    if(healthy)bytes+=used;
    used=0;return healthy;
  }
  bool put(uint8_t c) {
    if(!healthy)return false;
    buffer[used++]=c;return used<sizeof(buffer) || flush();
  }
public:
  explicit CamStrictJsonWriter(Destination& output):destination(output){}
  size_t write(uint8_t c) {
    if(c<0x20) {
      static const char hex[]="0123456789abcdef";
      const uint8_t escaped[]={'\\','u','0','0',uint8_t(hex[c>>4]),uint8_t(hex[c&15])};
      for(uint8_t b:escaped)if(!put(b))return 0;
    }else if(!put(c))return 0;
    return 1;
  }
  size_t write(const uint8_t* data,size_t size) {
    size_t consumed=0;while(consumed<size && write(data[consumed]))++consumed;return consumed;
  }
  bool finish(){return flush();}
  size_t outputBytes()const{return bytes;}
};
