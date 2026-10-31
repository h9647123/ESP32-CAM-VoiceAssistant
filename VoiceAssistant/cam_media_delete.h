#pragma once
#include "cam_media_delete_state.h"
static CamMediaDeleteState camMediaDelete;

static void camMediaDeleteClose() {
  if (camMediaDelete.active) ++buttonUiGeneration;
  camMediaDelete.clear();
}

static void camMediaDeleteDraw(bool failed = false) {
  if (!camViewReady || !camMediaDelete.active) return;
  camViewSprite.fillScreen(TFT_BLACK);
  camViewSprite.setTextSize(1);
  camViewSprite.setFont(&fonts::efontCN_16);
  camViewSprite.setTextColor(TFT_YELLOW, TFT_BLACK);
  camViewSprite.setCursor(6, 12);
  camViewSprite.print(camMediaDelete.video ? uiText("确认删除视频？", "Delete this video?", "Delete this video?") : uiText("确认删除照片？", "Delete this photo?", "Delete this photo?"));
  camViewSprite.setFont(&fonts::efontCN_12);
  camViewSprite.setTextColor(TFT_WHITE, TFT_BLACK);
  // Fixed-width filename rows fit the 240-pixel display in both languages.
  String name = String(camMediaDelete.path);
  name = name.substring(name.lastIndexOf('/') + 1);
  for (size_t at = 0; at < name.length(); at += 30) {
    camViewSprite.setCursor(6, 55 + (at / 30) * 18);
    camViewSprite.print(name.substring(at, min<size_t>(at + 30, name.length())));
  }
  camViewSprite.setCursor(6, 113);
  camViewSprite.print(uiText("删除后无法恢复", "Deletion cannot be undone", "Deletion cannot be undone"));
  if (failed) {
    camViewSprite.setTextColor(TFT_RED, TFT_BLACK);
    camViewSprite.setCursor(6, 142);
    camViewSprite.print(uiText("删除失败，请检查 TF 卡", "Delete failed; check TF card", "Delete failed; check TF card"));
  }
  camViewSprite.setTextColor(TFT_WHITE, TFT_BLACK);
  camViewSprite.setFont(&fonts::efontCN_16);
  camViewSprite.setCursor(6, 190);
  camViewSprite.print(uiText("A 确认删除", "A: confirm deletion", "A: confirm deletion"));
  camViewSprite.setCursor(6, 216);
  camViewSprite.print(uiText("B / C 取消", "B / C: cancel", "B / C: cancel"));
  camViewSprite.pushSprite(0, 0);
}

static bool camMediaDeleteOpen(bool video, size_t index, const String& path) {
  if (!camMediaDelete.begin(video, index, path.c_str())) return false;
  ++buttonUiGeneration;
  camMediaDeleteDraw();
  return true;
}
