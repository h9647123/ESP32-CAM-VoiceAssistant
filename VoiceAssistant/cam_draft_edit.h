#pragma once
#include <vector>
static bool camEraseDraftCharacter(std::vector<String>& segments) {
  while(!segments.empty() && !segments.back().length())segments.pop_back();
  if(segments.empty())return false;
  String& segment=segments.back();size_t at=segment.length()-1;
  while(at && (uint8_t(segment[at])&0xc0)==0x80)--at;
  segment.remove(at);
  if(!segment.length())segments.pop_back();
  return true;
}

// First tap keeps the existing erase action. A second tap clears only voice
// drafts, even if the first tap erased their last character.
struct CamDraftEraseGesture {
  bool armed=false;uint32_t at=0,generation=0;
  void reset(){armed=false;}
  bool click(uint32_t now,uint32_t pageGeneration,bool editing){
    const bool doubled=armed && generation==pageGeneration && uint32_t(now-at)<=350;
    armed=editing && !doubled;at=now;generation=pageGeneration;return doubled;
  }
};
