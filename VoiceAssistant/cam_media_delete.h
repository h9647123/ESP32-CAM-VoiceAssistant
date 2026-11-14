#pragma once
#include "cam_media_delete_state.h"
static CamMediaDeleteState camMediaDelete;
static LGFX_Sprite camDeletePreview(&tft);
static bool camDeletePreviewReady=false;

static void camMediaDeleteClose() {
  if (camMediaDelete.active) ++buttonUiGeneration;
  camMediaDelete.clear();
  camDeletePreview.deleteSprite();camDeletePreviewReady=false;
}

static void camMediaDeleteDraw(bool failed = false,bool deleting = false) {
  if(!camViewReady || !camMediaDelete.active)return;
  Serial.printf("[MEDIA-CONFIRM] stage=%u video=%d deleting=%d path=%s\n",camMediaDelete.finalConfirm?2:1,camMediaDelete.video,deleting,camMediaDelete.path);
  // The previous video's date occupies y=23..34. Never restore its header
  // below a shorter confirmation header: that left cyan text fragments.
  camViewSprite.clearClipRect();camViewSprite.fillScreen(TFT_BLACK);
  if(camDeletePreviewReady) {
    camViewSprite.setClipRect(0,40,240,144);
    camDeletePreview.pushSprite(&camViewSprite,0,0);
    camViewSprite.clearClipRect();
  }
  camViewSprite.setTextWrap(false);
  camViewSprite.setTextSize(1);camViewSprite.setFont(&fonts::efontCN_16);
  camViewSprite.setTextColor(failed?TFT_RED:TFT_YELLOW,TFT_BLACK);camViewSprite.setCursor(4,6);
  camViewSprite.print(deleting?uiText("正在删除...","Deleting...","Deleting..."):
    failed?uiText("操作失败，请检查 TF","Operation failed; check TF","Operation failed; check TF"):
    camMediaDelete.finalConfirm?(camMediaDelete.video?uiText("再次确认删除视频？","Delete video permanently?","Delete video permanently?"):uiText("再次确认删除照片？","Delete photo permanently?","Delete photo permanently?")):
    camMediaDelete.video?uiText("确认删除此视频？","Delete this video?","Delete this video?"):uiText("确认删除此照片？","Delete this photo?","Delete this photo?"));
  camViewSprite.setFont(&fonts::efontCN_12);camViewSprite.setTextColor(TFT_WHITE,TFT_BLACK);
  camViewSprite.setCursor(3,187);String name=camMediaDelete.path;name=name.substring(name.lastIndexOf('/')+1);
  camViewSprite.print(name.substring(0,34));
  camViewSprite.setCursor(3,204);
  camViewSprite.print(deleting?uiText("请稍候","Please wait","Please wait"):
      camMediaDelete.finalConfirm?uiText("B确认永久删除","B:delete permanently","B:delete permanently"):
      camMediaDelete.video?uiText("A继续删除 B/C取消","A:next B/C:cancel","A:next B/C:cancel"):
      uiText("A继续删除 B隐藏图片","A:next B:hide photo","A:next B:hide photo"));
  camViewSprite.setCursor(3,222);
  camViewSprite.print(deleting?String():
      camMediaDelete.finalConfirm?uiText("C取消 长A返回","C:cancel Hold A:back","C:cancel Hold A:back"):
      camMediaDelete.video?uiText("删除后无法恢复","Cannot be undone","Cannot be undone"):
      uiText("C取消 隐藏仍保留原图","C:cancel Hide keeps original","C:cancel Hide keeps original"));
  camViewSprite.pushSprite(0,0);if(deleting){delay(1);yield();}
}

static bool camMediaDeleteOpen(bool video, size_t index, const String& path) {
  if (!camMediaDelete.begin(video, index, path.c_str())) return false;
  camDeletePreview.setPsram(true);camDeletePreview.setColorDepth(16);
  camDeletePreviewReady=camDeletePreview.createSprite(240,240)!=nullptr;
  if(camDeletePreviewReady)camViewSprite.pushSprite(&camDeletePreview,0,0);
  ++buttonUiGeneration;
  camMediaDeleteDraw();
  return true;
}
