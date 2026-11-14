#pragma once
#include "cam_math_samples.h"
static bool camChatScreenshotReady();
static void camExportChatScreen();
static void camMathScreenshot(unsigned page,unsigned row=0,int language=-1) {
  if(!camChatScreenshotReady() || camPhoto.ready()){Serial.println("[MATH-TEST] idle home without draft photos required");return;}
  const char* sample=camMathSample(page);if(!sample){Serial.println("[MATH-TEST] pages: 1-14; optional scroll row and cn/en");return;}
  const auto savedLanguage=cfg.uiLanguage;if(language>=0)cfg.uiLanguage=language?UiLanguage::English:UiLanguage::Chinese;
  auto saved=std::move(chatHistory);const size_t bytes=chatHistoryBytes;
  const auto pixels=chatScrollPixels,target=chatScrollTargetPixels;const auto scroll=chatScroll;const bool autoScroll=chatAutoScrollBottom;
  chatHistory.clear();ChatMsg message;message.role="assistant";message.content=sample;if(page==8){message.thinkingTimed=message.thinkingComplete=true;message.thinkingMs=12000;}chatHistory.push_back(std::move(message));
  markChatLinesDirty();chatAutoScrollBottom=false;setChatScrollRow(row);
  Serial.printf("[MATH-TEST] page=%u row=%u source=render_buffers no_network=1 no_history_write=1\n",page,row);
  camExportChatScreen();
  cfg.uiLanguage=savedLanguage;
  chatHistory=std::move(saved);chatHistoryBytes=bytes;chatScroll=scroll;chatScrollPixels=pixels;chatScrollTargetPixels=target;chatAutoScrollBottom=autoScroll;
  markChatLinesDirty();drawPage1();
}
static bool camChatScreenshotReady() {
  return uiPage==UiPage::Page1 && appState==AppState::Idle && !recording && !asrSessionActive && !camPhotoUiActive();
}
static void camExportChatScreen() {
  if(!camChatScreenshotReady()){Serial.println("[UI-SCREENSHOT] unavailable; return to idle home");return;}
  noteInteraction();drawPage1();
  if(!headerSpriteReady || !chatSpriteReady || !footerSpriteReady){Serial.println("[UI-SCREENSHOT] render buffers unavailable");return;}
  LGFX_Sprite frame(&tft);frame.setPsram(true);frame.setColorDepth(16);
  if(!frame.createSprite(240,240)){Serial.println("[UI-SCREENSHOT] PSRAM unavailable");return;}
  frame.fillSprite(TFT_BLACK);
  if(headerSpriteReady)headerSprite.pushSprite(&frame,0,0);
  if(camPhoto.ready() && camPhoto.confirmed)camCandidateSprite.pushSprite(&frame,0,42);
  if(chatSpriteReady)chatSprite.pushSprite(&frame,0,CHAT_AREA_TOP);
  if(footerSpriteReady)footerSprite.pushSprite(&frame,0,202);
  if(chatLines.size()>CHAT_VISIBLE_LINES) {
    const int trackY=CHAT_AREA_TOP+12,trackH=CHAT_AREA_HEIGHT-24;
    frame.fillRoundRect(CHAT_SCROLL_TRACK_X,trackY,CHAT_SCROLL_TRACK_W,trackH,3,frame.color565(180,180,180));
    const uint16_t color=frame.color565(150,210,255);
    frame.fillTriangle(CHAT_SCROLL_CENTER_X,CHAT_AREA_TOP+4,CHAT_SCROLL_CENTER_X-CHAT_SCROLL_ARROW_HALF,CHAT_AREA_TOP+10,CHAT_SCROLL_CENTER_X+CHAT_SCROLL_ARROW_HALF,CHAT_AREA_TOP+10,color);
    frame.fillTriangle(CHAT_SCROLL_CENTER_X,CHAT_AREA_BOTTOM-4,CHAT_SCROLL_CENTER_X-CHAT_SCROLL_ARROW_HALF,CHAT_AREA_BOTTOM-10,CHAT_SCROLL_CENTER_X+CHAT_SCROLL_ARROW_HALF,CHAT_AREA_BOTTOM-10,color);
    if(chatScrollbarLastThumbY>=0)frame.fillRoundRect(CHAT_SCROLL_TRACK_X,chatScrollbarLastThumbY,CHAT_SCROLL_TRACK_W,chatScrollbarLastThumbH,3,TFT_WHITE);
  }
  Serial.printf("[UI-SCREENSHOT] source=render_buffers width=240 height=240 chat_top=%d scroll_px=%ld\n",CHAT_AREA_TOP,long(chatScrollPixels));
  camExportSprite(frame);frame.deleteSprite();
}
static void camChatThumbTest(unsigned requested,bool missing=false) {
  if(!camChatScreenshotReady() || camPhoto.ready() || !camSdReady){Serial.println("[CHAT-THUMB-TEST] idle home and TF required");return;}
  requested=constrain(requested,1U,3U);
  ChatMsg user;user.role="user";user.content=uiText("请看看这些图片。","Please look at these photos.","Please look at these photos.");
  LGFX_Sprite probe(&tft);probe.setPsram(true);probe.setColorDepth(16);
  if(!probe.createSprite(60,45)){Serial.println("[CHAT-THUMB-TEST] sample decoder unavailable");return;}
  File dir=SD_MMC.open("/photo");
  unsigned scanned=0;
  for(File f=dir.openNextFile();f && user.images.size()<requested && scanned++<200;f=dir.openNextFile()) {
    const String path=f.path();const size_t bytes=f.size();
    uint16_t w=0,h=0;
    const bool header=!f.isDirectory() && path.endsWith(".jpg") && bytes>4 && bytes<=CAM_JPEG_MAX_BYTES && camChatThumbJpegSize(f,w,h);
    f.close();
    if(header) {
      const float scale=min(60.0f/w,45.0f/h);
      if(probe.drawJpgFile(static_cast<fs::FS&>(SD_MMC),path.c_str(),0,0,60,45,0,0,scale,scale))user.images.push_back({path,bytes});
    }
    yield();
  }
  dir.close();probe.deleteSprite();
  if(user.images.empty()){Serial.println("[CHAT-THUMB-TEST] no saved JPEG available");return;}
  if(missing)user.images.back()={"/photo/__ui_missing_test__.jpg",123};
  // All fixture state lives in RAM. No save, API call or media write occurs.
  auto saved=std::move(chatHistory);const size_t savedBytes=chatHistoryBytes;
  const auto savedPixels=chatScrollPixels,savedTarget=chatScrollTargetPixels;
  const auto savedScroll=chatScroll;const bool savedAuto=chatAutoScrollBottom;
  chatHistory.clear();chatHistory.push_back(std::move(user));
  ChatMsg assistant;assistant.role="assistant";assistant.content=uiText("图片预览正常。","Local thumbnail preview.","Local thumbnail preview.");
  chatHistory.push_back(std::move(assistant));
  markChatLinesDirty();chatAutoScrollBottom=false;setChatScrollRow(0);
  camChatThumbInvalidate();
  Serial.printf("[CHAT-THUMB-TEST] count=%u missing=%d fixture=temporary no_network=1 no_history_write=1\n",unsigned(chatHistory[0].images.size()),missing);
  camExportChatScreen();
  Serial.printf("[CHAT-THUMB-STATS] hits=%lu builds=%lu max_decode_ms=%lu\n",(unsigned long)camChatThumbHits,(unsigned long)camChatThumbBuilds,(unsigned long)camChatThumbMaxMs);
  chatHistory=std::move(saved);chatHistoryBytes=savedBytes;
  chatScroll=savedScroll;chatScrollPixels=savedPixels;chatScrollTargetPixels=savedTarget;chatAutoScrollBottom=savedAuto;
  camChatThumbInvalidate();markChatLinesDirty();drawPage1();
}
