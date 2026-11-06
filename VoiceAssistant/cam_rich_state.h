#pragma once
#include <stddef.h>
// State at a byte boundary, including blank lines and escaped literal markers.
static bool camThinkingAt(const char* text,size_t end,char toggle=3,char escape=5) {
  bool thinking=false;
  for(size_t i=0;i<end;++i) {
    if(text[i]==escape && i+1<end){++i;continue;}
    if(text[i]==toggle)thinking=!thinking;
  }
  return thinking;
}
