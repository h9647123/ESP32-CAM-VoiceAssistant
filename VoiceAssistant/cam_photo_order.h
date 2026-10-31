#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// The sequence is the timeline; wall time is only a label. NTP correction or
// an offline boot must not reorder photographs. Legacy files have no sequence.
struct CamPhotoKey {
  uint64_t order = 0;
  int64_t epoch = 0;
};

static bool camValidPhotoTime(int64_t epoch) {
  return epoch >= 1577836800LL && epoch < 4102444800LL;
}

static CamPhotoKey camParsePhotoKey(const char* path, int64_t legacyTime = 0) {
  CamPhotoKey key;
  const char* name = strrchr(path, '/');
  name = name ? name + 1 : path;
  bool format = strlen(name) == 33 && name[0] == 'P' &&
      name[17] == '_' && name[18] == 'T' && !strcmp(name + 29, ".jpg");
  if (format) {
    for (int i = 1; i < 17; ++i) format &= isxdigit(static_cast<unsigned char>(name[i])) != 0;
    for (int i = 19; i < 29; ++i) format &= isdigit(static_cast<unsigned char>(name[i])) != 0;
  }
  if (format) {
    unsigned long long order = 0, epoch = 0;
    if (sscanf(name, "P%16llx_T%10llu.jpg", &order, &epoch) == 2 && order) {
      key.order = order;
      key.epoch = camValidPhotoTime(epoch) ? int64_t(epoch) : 0;
      return key;
    }
  }
  key.epoch = camValidPhotoTime(legacyTime) ? legacyTime : 0;
  return key;
}

static bool camPhotoNewer(const CamPhotoKey& a, const char* aPath,
    const CamPhotoKey& b, const char* bPath) {
  if (bool(a.order) != bool(b.order)) return bool(a.order);
  if (a.order != b.order) return a.order > b.order;
  if (a.epoch != b.epoch) return a.epoch > b.epoch;
  // The old fixed-width boot-id/sequence naming preserves order within a boot.
  // Across boots with no valid file time the exact old chronology is unknown.
  return strcmp(aPath, bPath) > 0;
}
