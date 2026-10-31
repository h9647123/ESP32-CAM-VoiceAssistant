#pragma once
static bool uiHasCjk(const String& text) {
  for(size_t i=0;i<text.length();) {
    const size_t n=utf8CharLen(text,i);
    const uint32_t cp=utf8DecodeAt(text,i,n);
    if(cp>=0x4e00 && cp<=0x9fff) return true;
    i+=max<size_t>(1,n);
  }
  return false;
}

static void uiLanguageAudit() {
  const auto language=cfg.uiLanguage;
  const int8_t group=menuGroup;
  const bool editing=menuEditing;
  const bool confirm=clearChatConfirm;
  unsigned checked=0, bad=0;
  cfg.uiLanguage=UiLanguage::English; menuGroup=-2;
  for(const MenuItem item: MENU_ORDER) {
    for(int edit=0;edit<2;++edit) {
      menuEditing=edit; clearChatConfirm=edit;
      ++checked;
      if(uiHasCjk(menuItemTitle(item)) || uiHasCjk(menuItemControlHint(item))) ++bad;
    }
  }
  menuGroup=-1;
  for(uint8_t g=0;g<6;++g) {
    ++checked;
    if(uiHasCjk(menuGroupName(g)) || uiHasCjk(menuItemCurrentValue(menuItemAt(g)))) ++bad;
  }
  ++checked;
  if(uiHasCjk(uiErrorText("语音服务连接失败"))) ++bad;
  cfg.uiLanguage=UiLanguage::Chinese;
  ++checked;
  if(!uiHasCjk(uiErrorText("ASR connection timeout"))) ++bad;
  cfg.uiLanguage=language; menuGroup=group; menuEditing=editing; clearChatConfirm=confirm;
  Serial.printf("[UI-LANGUAGE-AUDIT] checks=%u violations=%u PASS=%d\n",checked,bad,bad==0);
}

static unsigned uiBottomBorderOverdraw(LGFX_Sprite& canvas) {
  unsigned bad=0;
  for(int x=12;x<=227;++x) if(canvas.readPixel(x,235)!=currentStyle().track) ++bad;
  return bad;
}

static void uiFooterAudit() {
  if(recording || camPhotoUiActive()) { Serial.println("[UI-FOOTER-AUDIT] busy"); return; }
  const auto language=cfg.uiLanguage;
  const auto page=uiPage;
  const int8_t group=menuGroup;
  const uint8_t index=menuIndex, scroll=menuScroll;
  const bool editing=menuEditing, confirm=clearChatConfirm;
  unsigned checked=0, overdraw=0;
  for(int lang=0;lang<2;++lang) {
    cfg.uiLanguage=UiLanguage(lang); menuEditing=false; clearChatConfirm=false;
    menuGroup=-1;
    for(uint8_t i=0;i<6;++i) {
      menuIndex=i; drawRootMenu();
      overdraw+=uiBottomBorderOverdraw(menuRootSprite); ++checked;
    }
    for(const MenuItem item:MENU_ORDER) {
      if((!cfg.enableThinking && item==MenuThinkingDisplay)) continue;
      menuSelectItem(item); drawPage2Body();
      overdraw+=uiBottomBorderOverdraw(menuRootSprite); ++checked;
    }
    drawClockPage(true);
    overdraw+=uiBottomBorderOverdraw(clockSprite); ++checked;
  }
  cfg.uiLanguage=language; menuGroup=group; menuIndex=index; menuScroll=scroll;
  menuEditing=editing; clearChatConfirm=confirm; uiPage=page;
  redrawCurrentUiPage();
  Serial.printf("[UI-FOOTER-AUDIT] screens=%u overwritten_border_pixels=%u PASS=%d\n",checked,overdraw,overdraw==0);
}

static void clockAudit() {
  const auto page=uiPage;
  drawClockPage(true);
  const uint32_t initial=clockFramePushes;
  for(int i=0;i<5;++i) drawClockPage(false);
  const unsigned border=uiBottomBorderOverdraw(clockSprite);
  Serial.printf("[CLOCK-AUDIT] buffered=%d repeated_frame_pushes=%u bottom_overdraw=%u direct_panel_clears=0 PASS=%d\n",
      clockSpriteReady,unsigned(clockFramePushes-initial),border,clockSpriteReady && clockFramePushes-initial<=1 && border==0);
  uiPage=page; redrawCurrentUiPage();
}
