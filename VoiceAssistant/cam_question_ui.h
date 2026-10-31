#pragma once
static String camDecisionNotice;
static void camDrawDecision() {
  if(!camViewReady)return;
  const size_t count=camQuestionPhotoCount();
  camViewSprite.fillScreen(TFT_BLACK);camViewSprite.setTextSize(1);camViewSprite.setTextWrap(false);
  const uint16_t card=camViewSprite.color565(12,26,40);
  camViewSprite.fillRoundRect(4,3,232,22,5,card);camViewSprite.setFont(&fonts::efontCN_16);
  camViewSprite.setTextColor(TFT_CYAN,card);camViewSprite.setCursor(10,6);camViewSprite.print(uiText("拍照提问","Photo question","Photo question"));
  camViewSprite.setTextColor(TFT_WHITE,TFT_BLACK);camViewSprite.setCursor(8,32);
  camViewSprite.print(uiText("已选图片 ","Selected ","Selected ")+String(count)+"/"+String(CAM_QUESTION_PHOTO_LIMIT));
  camViewSprite.setFont(&fonts::efontCN_12);
  for(size_t i=0;i<CAM_QUESTION_PHOTO_LIMIT;++i) {
    camViewSprite.setCursor(10,57+17*i);camViewSprite.setTextColor(i<count?TFT_GREEN:TFT_DARKGREY,TFT_BLACK);
    if(i<count) {
      const auto& photo=camQuestionPhotoAt(i);
      camViewSprite.print(uiText("图片 ","Photo ","Photo ")+String(i+1)+"  "+String(photo.width)+"x"+String(photo.height)+
          (photo.imagePath.length()?uiText(" 已保存"," saved"," saved"):uiText(" 临时"," temporary"," temporary")));
    } else camViewSprite.print(uiText("空位 ","Empty ","Empty ")+String(i+1));
  }
  camViewSprite.setTextColor(TFT_YELLOW,TFT_BLACK);camViewSprite.setCursor(8,113);
  camViewSprite.print(fitTextMiddle(camDecisionNotice.length()?camDecisionNotice:
      (count==CAM_QUESTION_PHOTO_LIMIT?uiText("已达上限，可选用或仅发图片","Limit reached; use/send photos","Limit reached; use/send photos"):uiText("可继续拍摄下一张","You can add another photo","You can add another photo")),224));
  camViewSprite.fillRoundRect(4,139,232,97,6,card);camViewSprite.drawRoundRect(4,139,232,97,6,currentStyle().track);
  camViewSprite.setFont(&fonts::efontCN_16);
  const String actions[]={
    count<CAM_QUESTION_PHOTO_LIMIT?uiText("A 添加照片","A: add photo","A: add photo"):uiText("A 已达三张上限","A: photo limit reached","A: photo limit reached"),
    count?uiText("B 选用后语音提问","B: photos + voice","B: photos + voice"):uiText("B 请先添加图片","B: add photos first","B: add photos first"),
    uiText("C 不拍照，仅语音","C: voice only","C: voice only"),
    uiText("长B 仅发送图片","Hold B: send photos","Hold B: send photos"),
    uiText("长A 返回菜单","Hold A: menu","Hold A: menu")};
  for(unsigned i=0;i<5;++i){camViewSprite.setTextColor((i==0 && count==CAM_QUESTION_PHOTO_LIMIT)?TFT_DARKGREY:TFT_WHITE,card);camViewSprite.setCursor(10,143+18*i);camViewSprite.print(fitTextMiddle(actions[i],220));}
  camViewSprite.pushSprite(0,0);
}
static void camOpenDecision(const String& notice) {
  if(recording || stoppingRecording || asrSessionActive || asrConnected || appState==AppState::Thinking)return;
  cancelServiceProbe();camStopPreviewCamera();camAlbumClear();
  camQuestionCaptureFlow=false;camQuestionReadyForVoice=false;
  camViewSprite.setPsram(true);camViewSprite.setColorDepth(16);
  if(!camViewReady)camViewReady=camViewSprite.createSprite(240,240)!=nullptr;
  if(!camViewReady){setState(AppState::Error,"Question page memory unavailable");return;}
  camDecisionNotice=notice;lastError=lastFailedQuestion="";lastFailedUseSearch=false;appState=AppState::Idle;
  ++buttonUiGeneration;camPhotoStage=CamPhotoStage::Decision;uiPage=UiPage::CameraView;
  camDrawDecision();
}
static void camSendImagesOnly() {
  if(recording || stoppingRecording || asrSessionActive || appState==AppState::Thinking)return;
  if(!camQuestionPhotoCount()) {
    if(camPhotoOnly)camOpenAlbum();
    else camOpenDecision(uiText("请先添加或选用图片","Add or select photos first","Add or select photos first"));
    return;
  }
  const size_t count=camQuestionPhotoCount();camQuestionReadyForVoice=true;camVoiceOnlyQuestion=false;camQuestionRequiresPhoto=true;
  if(camPhotoUiActive())camClosePhotoUi();else {uiPage=UiPage::Page1;applyChatLayout();drawPage1();}
  String prompt=uiText("请分析这 ","Please analyze these ","Please analyze these ")+String(count)+
      uiText(" 张图片，解答图中的问题；若没有明确问题，请描述主要内容。看不清的文字和细节请明确说明，不要猜测。",
      " image(s). Answer questions shown in them, or describe their main content if no question is shown. Clearly state unreadable text or uncertain details.",
      " image(s). Answer questions shown in them, or describe their main content if no question is shown. Clearly state unreadable text or uncertain details.");
  askAndStoreChat(prompt);
}
static bool camQuestionHandleKey(char key,bool click,bool longPress) {
  if(camPhotoStage!=CamPhotoStage::Decision)return false;
  if(key=='A' && longPress) {menuSelectItem(MenuQuestionMode);camClosePhotoUi(true);return true;}
  if(key=='B' && longPress) {camSendImagesOnly();return true;}
  if(!click)return true;
  if(key=='C') {
    camReleasePhoto();camVoiceOnlyQuestion=true;camQuestionReadyForVoice=true;
    camClosePhotoUi();drawPage1Footer(uiText("本次仅语音 长A录音","Voice only: hold A to speak","Voice only: hold A to speak"));return true;
  }
  if(key=='B') {
    if(!camQuestionPhotoCount()){camDecisionNotice=uiText("请先添加图片，或按C仅语音","Add photos, or C for voice only","Add photos, or C for voice only");camDrawDecision();return true;}
    camRestoreLastPhoto();camQuestionReadyForVoice=true;camVoiceOnlyQuestion=false;
    camClosePhotoUi();drawPage1Footer(uiText("已选用图片 长A录音","Photos ready: hold A to speak","Photos ready: hold A to speak"));return true;
  }
  if(key=='A') {
    if(!camStashCurrentPhoto()){camDecisionNotice=uiText("最多三张，请选用或直接发图","Maximum 3; use or send photos","Maximum 3; use or send photos");camDrawDecision();return true;}
    camQuestionCaptureFlow=true;camOpenPreview();
    if(camPhotoStage!=CamPhotoStage::Preview){camRestoreLastPhoto();camOpenDecision(uiText("无法进入相机","Camera unavailable","Camera unavailable"));}
  }
  return true;
}
