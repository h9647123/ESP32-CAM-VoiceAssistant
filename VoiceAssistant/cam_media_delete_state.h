#pragma once
#include <stddef.h>
#include <string.h>

// The pending target is copied, never looked up again from the current cursor.
// Filesystem mutation belongs to the caller and requires Confirm explicitly.
struct CamMediaDeleteState {
  enum class Action { Waiting, Cancel, Advance, Hide, Confirm };
  bool active = false;
  bool video = false;
  bool finalConfirm = false;
  size_t index = 0;
  char path[128] = {};

  void clear() { active = false; finalConfirm = false; path[0] = 0; }
  bool begin(bool isVideo, size_t selected, const char* target) {
    clear();
    if (!target) return false;
    const size_t n = strlen(target);
    const char* prefix = isVideo ? "/video/" : "/photo/";
    const char* extension = isVideo ? ".avi" : ".jpg";
    if (n >= sizeof(path) || n <= strlen(prefix) + 4 ||
        strncmp(target, prefix, strlen(prefix)) ||
        strcmp(target + n - 4, extension) || strstr(target, "..") ||
        strchr(target + strlen(prefix), '/') || strchr(target, '\\')) return false;
    memcpy(path, target, n + 1);
    video = isVideo; index = selected; active = true;
    return true;
  }
  bool matches(bool isVideo, size_t selected, const char* target) const {
    return active && video == isVideo && index == selected && target && !strcmp(path, target);
  }
  Action action(char key, bool click, bool longPress) {
    if (!active) return Action::Waiting;
    if(key=='A' && longPress)return Action::Cancel;
    if(!click || longPress)return Action::Waiting;
    if(key=='C')return Action::Cancel;
    if(!finalConfirm) {
      if(key=='B')return video?Action::Cancel:Action::Hide;
      if(key=='A'){finalConfirm=true;return Action::Advance;}
    }else if(key=='B')return Action::Confirm;
    return Action::Waiting;
  }
};
