#pragma once
#include <stdint.h>
// Keep AUTO's stored ID 2. Older ON=1 and AGENT=3 migrate to AUTO.
enum class SearchMode:uint8_t {Off=0,Auto=2};
static SearchMode camNormalizeSearchMode(int stored) {
  return stored>=1 && stored<=3?SearchMode::Auto:SearchMode::Off;
}
static bool camSearchChatEnabled(SearchMode mode,bool isolated) {
  return mode==SearchMode::Auto && !isolated;
}
