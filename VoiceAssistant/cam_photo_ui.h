#pragma once
static uint32_t camFpsWindowAt = 0, camFpsWindowFrames = 0;
static uint32_t camFpsTenths = 0;
// Camera frames are only borrowed during JPEG decoding. The selected photo owns
// its PSRAM copy, so the camera can release DMA memory before voice/TLS starts.
static bool camPhotoUiActive() { return camPhotoStage != CamPhotoStage::Off; }

static void camExportSprite(LGFX_Sprite& sprite) {
  Serial.println("[CAM-VIEW-BEGIN] width=240 height=240 format=RGB888");
  uint8_t row[720], encoded[961];
  for (int y = 0; y < 240; ++y) {
    sprite.readRectRGB(0, y, 240, 1, row);
    size_t length = 0;
    if (mbedtls_base64_encode(encoded, sizeof(encoded), &length, row, sizeof(row))) {
      Serial.println("[CAM-VIEW-ERROR] Encode failed"); return;
    }
    Serial.write(encoded, length); Serial.println(); yield();
  }
  Serial.println("[CAM-VIEW-END]");
}

static void camExportView() {
  if (!camViewReady) { Serial.println("[CAM-VIEW-ERROR] No camera view"); return; }
  camExportSprite(camViewSprite);
}

static void camStopPreviewCamera() {
  if (camCameraReady) { esp_camera_deinit(); camCameraReady = false; }
}

static void camDrawPhotoView(const uint8_t* jpeg, size_t size, int width, int height) {
  if (!camViewReady) return;
  camViewSprite.fillScreen(TFT_BLACK);
  const bool preview = camPhotoStage == CamPhotoStage::Preview;
  const int viewTop = preview ? 0 : 24;
  const int viewHeight = preview ? 226 : 202;
  if (jpeg && width && height) {
    float scale = min(240.0f / width, float(viewHeight) / height);
    if (camPreviewZoom) scale *= 2;
    camViewSprite.setClipRect(0, viewTop, 240, viewHeight);
    camViewSprite.drawJpg(jpeg, size, (240-int(width*scale))/2,
        viewTop+(viewHeight-int(height*scale))/2, 0, 0, 0, 0, scale, scale);
    camViewSprite.clearClipRect();
  }
  if (preview && !camFullPreview) {
    camViewSprite.fillRect(0, 0, 240, 23, TFT_BLACK);
    camViewSprite.setFont(&fonts::efontCN_16);
    camViewSprite.setTextColor(TFT_YELLOW, TFT_BLACK);
    camViewSprite.setCursor(4, 2);
    camViewSprite.print(uiText("缩放预览·保持不动","Scaled - keep steady","Scaled - keep steady"));
  }
  camViewSprite.setFont(&fonts::efontCN_16);
  camViewSprite.setTextColor(TFT_WHITE, TFT_BLACK);
  if (!preview) {
    camViewSprite.setCursor(4, 4);
    camViewSprite.print(uiText("确认照片","Use this photo?","Use this photo?"));
  }
  camViewSprite.setFont(camPhotoOnly ? &fonts::efontCN_12 : &fonts::efontCN_16);
  camViewSprite.setCursor(3, 220);
  camViewSprite.print(preview ? (camPhotoOnly ? uiText("A拍 长B录 长A菜单","A:shot Hold B:rec A:back","A:shot Hold B:rec A:back") : (camPreviewZoom ? uiText("A拍照 B缩小 C退出","A:shot B:- C:exit","A:shot B:- C:exit") : uiText("A拍照 B放大 C退出","A:shot B:+ C:exit","A:shot B:+ C:exit"))) : (camPhotoOnly ? uiText("A保存 B重拍 C取消","A:save B:retake C:back","A:save B:retake C:back") : uiText("A选用 B重拍 C取消","A:use B:retake C:back","A:use B:retake C:back")));
  if (preview) {
    const String fps = camFpsWindowAt ? String(camFpsTenths / 10.0f, 1) + " FPS" : "-- FPS";
    camViewSprite.setCursor(237 - camViewSprite.textWidth(fps), 220);
    camViewSprite.print(fps);
  }
  camViewSprite.pushSprite(0, 0);
}

static void camClosePhotoUi(bool toMenu) {
  if(toMenu) animatePageTransition(false);
  camVideoLeave();
  ++buttonUiGeneration;
  camAlbumClear();
  camStopPreviewCamera();
  camPhotoStage = CamPhotoStage::Off;
  camQuestionCaptureFlow=false;
  camViewSprite.deleteSprite(); camViewReady = false;
  uiPage = toMenu ? UiPage::Page2 : UiPage::Page1;
  applyChatLayout();
  if(toMenu) { menuEditing=false; drawPage2(); }
  else drawPage1();
}

static void camOpenPreview() {
  cancelServiceProbe();
  camVideoNotice="";
  if (!buttonEvents) { setState(AppState::Error, "Button sampler unavailable"); return; }
  if (recording || stoppingRecording || asrSessionActive || asrConnected || composingQuestion) return;
  // Also request a UXGA allocation when the diagnostic native-size mode is on.
  camFrameSize = FRAMESIZE_UXGA;
  camViewSprite.setPsram(true);
  camViewSprite.setColorDepth(16);
  if (!camViewReady) camViewReady = camViewSprite.createSprite(240, 240) != nullptr;
  if (!camViewReady) { setState(AppState::Error, "Preview memory unavailable"); return; }
  // A new photo replaces a failed question. Keeping its retry marker would
  // cause startRecording() to discard the newly confirmed image next time.
  lastError = lastFailedQuestion = "";
  lastFailedUseSearch = false;
  appState = AppState::Idle;
  camPreviewZoom = false;
  ++buttonUiGeneration;
  camPhotoStage = CamPhotoStage::Preview;
  uiPage = UiPage::CameraView;
  camPreviewLastAt = 0;
  camFpsWindowAt = camFpsWindowFrames = camFpsTenths = 0;
  Serial.println("[CAM-UI] preview opened");
  camDrawPhotoView(nullptr, 0, 0, 0);
}

static void camPreviewTick() {
  if(camVideoRecording)return;
  if (uiPage != UiPage::CameraView || camPhotoStage != CamPhotoStage::Preview || millis()-camPreviewLastAt < 180) return;
  camPreviewLastAt = millis();
  String error;
  if (!camInitCamera(error)) {
    lastError = error; camPreviewErrors++;
    camClosePhotoUi(); setState(AppState::Error, error); return;
  }
  sensor_t* sensor = esp_camera_sensor_get();
  const framesize_t previewSize = camFullPreview ? FRAMESIZE_UXGA : FRAMESIZE_VGA;
  if (!sensor || (sensor->status.framesize != previewSize &&
      sensor->set_framesize(sensor, previewSize) != 0)) {
    camClosePhotoUi(); setState(AppState::Error, "Preview resolution failed"); return;
  }
  camera_fb_t* frame = esp_camera_fb_get();
  if (frame) {
    camDrawPhotoView(frame->buf, frame->len, frame->width, frame->height);
    if(camVideoNotice.length()) {
      camViewSprite.fillRect(0,24,240,20,TFT_BLACK);camViewSprite.setFont(&fonts::efontCN_12);
      camViewSprite.setTextColor(TFT_YELLOW,TFT_BLACK);camViewSprite.setCursor(3,26);
      camViewSprite.print(camVideoNotice);camViewSprite.pushSprite(0,0);
    }
    esp_camera_fb_return(frame); camPreviewFrames++;
    const uint32_t now = millis();
    if (!camFpsWindowAt) camFpsWindowAt = now;
    else {
      ++camFpsWindowFrames;
      const uint32_t elapsed = now - camFpsWindowAt;
      if (elapsed >= 1000U) {
        camFpsTenths = (camFpsWindowFrames * 10000U + elapsed / 2) / elapsed;
        camFpsWindowFrames = 0; camFpsWindowAt = now;
      }
    }
  } else { camPreviewErrors++; }
}

static void camDrawCandidate() {
  if (!camPhoto.ready() || !camPhoto.confirmed) return;
  if (camCandidateGeneration != camPhoto.generation) {
    camCandidateSprite.deleteSprite();
    camCandidateSprite.setPsram(true); camCandidateSprite.setColorDepth(16);
    if (!camCandidateSprite.createSprite(240, 50)) return;
    camCandidateSprite.fillScreen(TFT_BLACK);
    if (camPhoto.jpeg) camCandidateSprite.drawJpg(camPhoto.jpeg, camPhoto.size, 2, 2, 60, 45,
        0, 0, 60.0f/camPhoto.width, 45.0f/camPhoto.height);
    camCandidateSprite.setFont(&fonts::efontCN_16);
    camCandidateSprite.setTextColor(TFT_GREEN, TFT_BLACK);
    camCandidateSprite.setCursor(68, 3); camCandidateSprite.print(uiText("已选 ","Selected ","Selected ")+String(camQuestionPhotoCount())+"/"+String(CAM_QUESTION_PHOTO_LIMIT));
    camCandidateSprite.setCursor(68, 24);
    camCandidateSprite.print(camPhoto.imagePath.length() ? uiText("已保存到 TF","Saved on TF","Saved on TF") : uiText("临时照片","Temporary photo","Temporary photo"));
    camCandidateGeneration = camPhoto.generation;
  }
  camCandidateSprite.pushSprite(0, 42);
}

static void camDrawShutterHint() {
  // Retain the last decoded view. No second UXGA decode or black-screen flash.
  camViewSprite.fillRect(0,204,240,36,TFT_BLACK);
  camViewSprite.setFont(&fonts::efontCN_16); camViewSprite.setTextSize(1);
  camViewSprite.setTextColor(TFT_WHITE,TFT_BLACK);
  const String status=uiText("正在拍照...","Taking photo...","Taking photo...");
  camViewSprite.setCursor((240-camViewSprite.textWidth(status))/2,207);
  camViewSprite.print(status);
  if(!camFullPreview) {
    const String hint=uiText("请保持角度不动，正在稳定曝光","Keep steady - exposure settling","Keep steady - exposure settling");
    camViewSprite.setFont(&fonts::efontCN_12);
    camViewSprite.setTextColor(TFT_YELLOW,TFT_BLACK);
    camViewSprite.setCursor((240-camViewSprite.textWidth(hint))/2,228);
    camViewSprite.print(hint);
  }
  camViewSprite.pushSprite(0,0);
}

static bool camPhotoHandleKey(char key, bool click, bool longPress) {
  if (!camPhotoUiActive()) return false;
  if (camPhotoStage == CamPhotoStage::Decision) return camQuestionHandleKey(key,click,longPress);
  if (camPhotoStage == CamPhotoStage::Album) return camAlbumHandleKey(key, click, longPress);
  if (key=='A' && longPress) { if(!camPhoto.confirmed){camPhoto.clear();camRestoreLastPhoto();} menuSelectItem(MenuCamera); camClosePhotoUi(true); return true; }
  if (!click) return true;
  if (key == 'C') {
    if(camQuestionCaptureFlow){camPhoto.clear();camRestoreLastPhoto();camOpenDecision();}
    else {camReleasePhoto();camClosePhotoUi();}return true;
  }
  if (key == 'B') {
    if (camPhotoStage == CamPhotoStage::Review) {
      ++buttonUiGeneration; camPhoto.clear();camQuestionRequiresPhoto=camQuestionCaptureFlow || camEarlierPhotoCount>0;
      camPhotoStage = CamPhotoStage::Preview; camPreviewZoom = false;
      camFpsWindowAt = camFpsWindowFrames = camFpsTenths = 0;
    } else camPreviewZoom = !camPreviewZoom;
    return true;
  }
  if (key != 'A') return true;
  if (camPhotoStage == CamPhotoStage::Preview) {
    camDrawShutterHint();
    camFrameSize = FRAMESIZE_UXGA;
    String error;
    if (!camCapture(error, false)) {
      if(camQuestionCaptureFlow){camRestoreLastPhoto();camOpenDecision(uiErrorText(error));}
      else {camClosePhotoUi();setState(AppState::Error,error);}return true;
    }
    camStopPreviewCamera();
    ++buttonUiGeneration;
    camPhotoStage = CamPhotoStage::Review; camPreviewZoom = false;
    camDrawPhotoView(camPhoto.jpeg, camPhoto.size, camPhoto.width, camPhoto.height);
  } else {
    if (!camPhoto.confirmed) {
      // TF failure still permits a clearly labelled temporary PSRAM candidate.
      if(!camArchiveJpeg(camPhoto.jpeg, camPhoto.size, camPhoto.imagePath, camPhoto.capturedAtEpoch) && camPhotoOnly) {
        camViewSprite.fillRect(0,204,240,36,TFT_BLACK); camViewSprite.setFont(&fonts::efontCN_12);
        camViewSprite.setTextColor(TFT_RED,TFT_BLACK); camViewSprite.setCursor(3,207);
        camViewSprite.print(uiText("保存失败，请检查TF卡后重试","Save failed; check TF and retry","Save failed; check TF and retry")); camViewSprite.pushSprite(0,0); return true;
      }
      if(!camBoundQuestionPhotoRam()) {
        camViewSprite.fillRect(0,204,240,36,TFT_BLACK);camViewSprite.setFont(&fonts::efontCN_12);camViewSprite.setTextColor(TFT_YELLOW,TFT_BLACK);camViewSprite.setCursor(3,208);
        camViewSprite.print(uiText("图片内存不足，请检查TF卡","Photo memory full; check TF","Photo memory full; check TF"));camViewSprite.pushSprite(0,0);return true;
      }
      camPhoto.confirmed = true;
    }
    if(camPhotoOnly) camReleasePhoto();
    if(camQuestionCaptureFlow)camOpenDecision();else camClosePhotoUi();
  }
  return true;
}
