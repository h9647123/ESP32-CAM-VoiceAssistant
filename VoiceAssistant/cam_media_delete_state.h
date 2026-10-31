#pragma once
#include <stddef.h>
#include <string.h>

// The pending target is copied, never looked up again from the current cursor.
// Filesystem mutation belongs to the caller and requires Confirm explicitly.
struct CamMediaDeleteState {
  enum class Action { Waiting, Cancel, Confirm };
  bool active = false;
  bool video = false;
  size_t index = 0;
  char path[128] = {};

  void clear() { active = false; path[0] = 0; }
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
  Action action(char key, bool click, bool longPress) const {
    if (!active) return Action::Waiting;
    if ((key == 'A' && longPress) || (click && (key == 'B' || key == 'C'))) return Action::Cancel;
    if (key == 'A' && click && !longPress) return Action::Confirm;
    return Action::Waiting;
  }
};
