#pragma once
#include <algorithm>

struct CamAlbumItem { String path; CamPhotoKey key; };
static std::vector<CamAlbumItem> camAlbumItems;
static size_t camAlbumIndex = 0;
static uint8_t* camAlbumJpeg = nullptr;
static size_t camAlbumBytes = 0;
static uint16_t camAlbumWidth = 0, camAlbumHeight = 0;
static String camAlbumError;
static bool camAlbumClickPending=false;
static uint32_t camAlbumClickAt=0;
static String camAlbumDeleteTarget;

static void camAlbumClear() {
  camMediaDeleteClose();
  camAlbumClickPending=false; camAlbumDeleteTarget="";
  if (camAlbumJpeg) heap_caps_free(camAlbumJpeg);
  camAlbumJpeg = nullptr; camAlbumBytes = 0;
  camAlbumWidth = camAlbumHeight = 0;
  std::vector<CamAlbumItem>().swap(camAlbumItems);
  camAlbumIndex = 0;
}

// Read dimensions without allocating a second, full-resolution RGB image.
static bool camAlbumJpegDimensions(const uint8_t* p, size_t n, uint16_t& w, uint16_t& h) {
  if (n < 4 || p[0] != 255 || p[1] != 216 || p[n-2] != 255 || p[n-1] != 217) return false;
  size_t at = 2;
  while (at + 3 < n) {
    if (p[at++] != 255) return false;
    while (at < n && p[at] == 255) ++at;
    if (at >= n) return false;
    const uint8_t marker = p[at++];
    if (marker == 0xda || marker == 0xd9) return false;
    if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd8)) continue;
    if (at + 2 > n) return false;
    const size_t len = (size_t(p[at]) << 8) | p[at+1];
    if (len < 2 || len > n - at) return false;
    // OV2640 emits baseline JPEG. Reject unsupported encodings explicitly.
    if (marker == 0xc0) {
      if (len < 8 || p[at+2] != 8) return false;
      h = (uint16_t(p[at+3]) << 8) | p[at+4];
      w = (uint16_t(p[at+5]) << 8) | p[at+6];
      return w && h && w <= 4096 && h <= 4096;
    }
    at += len;
  }
  return false;
}

static String camAlbumTimeLabel(const CamPhotoKey& key) {
  if (key.epoch) {
    time_t epoch = key.epoch;
    struct tm local;
    localtime_r(&epoch, &local);
    char label[40];
    strftime(label, sizeof(label), "%Y-%m-%d %H:%M:%S", &local);
    return (key.order ? String("") : uiText("旧图 ","Old ","Old ")) + label;
  }
  if (!key.order) return uiText("旧图·时间未知","Old photo - time unknown","Old photo - time unknown");
  char label[48];
  snprintf(label, sizeof(label), cfg.uiLanguage==UiLanguage::Chinese ? "无时钟·序号 %llu" : "No clock - sequence %llu", (unsigned long long)key.order);
  return label;
}

static void camAlbumDraw() {
  if (!camViewReady) return;
  camViewSprite.fillScreen(TFT_BLACK);
  camViewSprite.setFont(&fonts::efontCN_16);
  camViewSprite.setTextColor(TFT_WHITE, TFT_BLACK);
  camViewSprite.setCursor(4, 2);
  camViewSprite.print(uiText("相册 ","Album ","Album ") + String(camAlbumItems.empty() ? 0 : camAlbumIndex+1) + "/" + String(camAlbumItems.size()));
  camViewSprite.setCursor(4, 20);
  camViewSprite.setFont(&fonts::efontCN_12);
  if (!camAlbumItems.empty()) camViewSprite.print(camAlbumTimeLabel(camAlbumItems[camAlbumIndex].key));
  if (camAlbumJpeg) {
    const float scale = min(240.0f / camAlbumWidth, 192.0f / camAlbumHeight);
    camViewSprite.setClipRect(0, 32, 240, 192);
    camViewSprite.drawJpg(camAlbumJpeg, camAlbumBytes, (240-int(camAlbumWidth*scale))/2,
        32+(192-int(camAlbumHeight*scale))/2, 0, 0, 0, 0, scale, scale);
    camViewSprite.clearClipRect();
  } else {
    camViewSprite.setCursor(4, 104);
    camViewSprite.setFont(&fonts::efontCN_12);
    camViewSprite.print(camAlbumError);
  }
  camViewSprite.setFont(&fonts::efontCN_12);
  camViewSprite.setCursor(3, 223);
  camViewSprite.print(uiText("A选 B旧 C新 双C删 长A退","A:use B:old C:new 2C:del Hold A:back","A:use B:old C:new 2C:del Hold A:back"));
  camViewSprite.pushSprite(0, 0);
}

static void camAlbumLoad() {
  if (camAlbumJpeg) heap_caps_free(camAlbumJpeg);
  camAlbumJpeg = nullptr; camAlbumBytes = 0;
  camAlbumError = camSdReady ? uiText("暂无照片","No photos","No photos") : uiText("未连接 TF 卡","TF card unavailable","TF card unavailable");
  if (camAlbumItems.empty()) { camAlbumDraw(); return; }
  File f = SD_MMC.open(camAlbumItems[camAlbumIndex].path, FILE_READ);
  const size_t n = f ? f.size() : 0;
  camAlbumError = uiText("照片读取失败，可继续翻页","Cannot read photo - skip it","Cannot read photo - skip it");
  if (n >= 4 && n <= CAM_JPEG_MAX_BYTES)
    camAlbumJpeg = static_cast<uint8_t*>(heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  bool ok = camAlbumJpeg != nullptr;
  for (size_t at = 0; ok && at < n;) {
    const size_t chunk = min<size_t>(4096, n-at);
    ok = f.read(camAlbumJpeg+at, chunk) == chunk;
    at += chunk; yield();
  }
  f.close();
  ok = ok && camAlbumJpegDimensions(camAlbumJpeg, n, camAlbumWidth, camAlbumHeight);
  if (ok) camAlbumBytes = n;
  else { if (camAlbumJpeg) heap_caps_free(camAlbumJpeg); camAlbumJpeg = nullptr; }
  camAlbumDraw();
}

static void camAlbumStatus() {
  Serial.printf("[ALBUM] active=%d count=%u index=%u loaded=%u width=%u height=%u\n",
      camPhotoStage == CamPhotoStage::Album, unsigned(camAlbumItems.size()),
      unsigned(camAlbumIndex+1), unsigned(camAlbumBytes), camAlbumWidth, camAlbumHeight);
  for (size_t i = 0; i < camAlbumItems.size(); ++i) {
    const auto& item = camAlbumItems[i];
    Serial.printf("[ALBUM-ITEM] index=%u order=%llu epoch=%lld path=%s\n",
        unsigned(i+1), (unsigned long long)item.key.order,
        (long long)item.key.epoch, item.path.c_str());
  }
}

static void camPhotoOrderAudit() {
  const auto offline = camParsePhotoKey("P0000000000000002_T0000000000.jpg");
  const auto online = camParsePhotoKey("P0000000000000001_T1793400000.jpg");
  const auto corrected = camParsePhotoKey("P0000000000000003_T1700000000.jpg");
  const auto legacy = camParsePhotoKey("deadbeef-00000012.jpg");
  const auto bad = camParsePhotoKey("P000000000000000Z_T1793400000.jpg");
  const bool checks[] = {
    offline.order == 2 && !offline.epoch,
    online.order == 1 && online.epoch == 1793400000,
    camPhotoNewer(offline, "", online, ""),
    camPhotoNewer(corrected, "", online, ""),
    !legacy.order && !legacy.epoch,
    !bad.order && !bad.epoch,
    camPhotoNewer(offline, "", legacy, ""),
    !camValidPhotoTime(0) && !camValidPhotoTime(4102444800LL)
  };
  unsigned passed = 0;
  for (bool ok : checks) passed += ok;
  Serial.printf("[PHOTO-ORDER-AUDIT] checks=%u pass=%u synthetic_metadata_only=1\n",
      unsigned(sizeof(checks)/sizeof(checks[0])), passed);
}

static void camOpenAlbum() {
  if (recording || stoppingRecording || asrSessionActive || asrConnected || appState == AppState::Thinking) return;
  menuSelectItem(MenuAlbum);
  camStopPreviewCamera(); camAlbumClear();
  camViewSprite.setPsram(true); camViewSprite.setColorDepth(16);
  if (!camViewReady) camViewReady = camViewSprite.createSprite(240, 240) != nullptr;
  if (!camViewReady) { setState(AppState::Error, "Album memory unavailable"); return; }
  if (camSdReady) {
    File dir = SD_MMC.open("/photo");
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
      const String path = f.path();
      if (!f.isDirectory() && path.endsWith(".jpg")) {
        CamAlbumItem item{path, camParsePhotoKey(path.c_str(), f.getLastWrite())};
        auto pos = std::lower_bound(camAlbumItems.begin(), camAlbumItems.end(), item,
            [](const CamAlbumItem& a, const CamAlbumItem& b) {
              return camPhotoNewer(a.key, a.path.c_str(), b.key, b.path.c_str());
            });
        camAlbumItems.insert(pos, item);
        if (camAlbumItems.size() > CAM_PHOTO_ARCHIVE_LIMIT) camAlbumItems.pop_back();
      }
      f.close(); yield();
    }
    dir.close();
  }
  ++buttonUiGeneration;
  camPhotoStage = CamPhotoStage::Album; uiPage = UiPage::CameraView;
  camAlbumLoad();
  Serial.printf("[ALBUM] opened count=%u newest_first=1\n", unsigned(camAlbumItems.size()));
}

static bool camAlbumHandleKey(char key, bool click, bool longPress) {
  if (camMediaDelete.active) {
    const auto action = camMediaDelete.action(key, click, longPress);
    if (action == CamMediaDeleteState::Action::Cancel) {
      camMediaDeleteClose(); camAlbumDraw();
    } else if (action == CamMediaDeleteState::Action::Confirm) {
      const bool targetValid = camAlbumIndex < camAlbumItems.size() &&
          camMediaDelete.matches(false, camAlbumIndex, camAlbumItems[camAlbumIndex].path.c_str());
      if (!targetValid) { camMediaDeleteClose(); camAlbumDraw(); return true; }
      if (!camSdReady || !SD_MMC.remove(camMediaDelete.path)) { camMediaDeleteDraw(true); return true; }
      for (size_t i = camQuestionPhotoCount(); i > 0; --i)
        if (camQuestionPhotoAt(i-1).imagePath == camMediaDelete.path) camRemoveQuestionPhoto(i-1);
      applyChatLayout();
      Serial.println(String("[ALBUM] deleted ") + camMediaDelete.path);
      camAlbumItems.erase(camAlbumItems.begin() + camAlbumIndex);
      if (camAlbumIndex >= camAlbumItems.size()) camAlbumIndex = camAlbumItems.empty() ? 0 : camAlbumItems.size()-1;
      camMediaDeleteClose(); camAlbumLoad();
    }
    return true;
  }
  if (key == 'A' && longPress) {
    camClosePhotoUi(true);
    return true;
  }
  if (!click) return true;
  if(key=='C' && !camAlbumItems.empty()) {
    if(camAlbumClickPending && millis()-camAlbumClickAt<=350 && camAlbumItems[camAlbumIndex].path==camAlbumDeleteTarget) {
      camAlbumClickPending=false;
      camMediaDeleteOpen(false, camAlbumIndex, camAlbumDeleteTarget);
    } else { camAlbumClickPending=true; camAlbumClickAt=millis(); camAlbumDeleteTarget=camAlbumItems[camAlbumIndex].path; }
    return true;
  }
  camAlbumClickPending=false;
  if (key == 'B' || key == 'C') {
    if (camAlbumItems.empty()) return true;
    size_t next = camAlbumIndex;
    if (key == 'B' && next+1 < camAlbumItems.size()) ++next;
    if (key == 'C' && next) --next;
    if (next != camAlbumIndex) { camAlbumIndex = next; camAlbumLoad(); }
    return true;
  }
  if (key != 'A' || !camAlbumJpeg || !camAlbumBytes) return true;
  if(camPhotoPathPinned(camAlbumItems[camAlbumIndex].path)) {
    camViewSprite.setFont(&fonts::efontCN_12);camViewSprite.fillRect(0,198,240,22,TFT_BLACK);camViewSprite.setTextColor(TFT_YELLOW,TFT_BLACK);camViewSprite.setCursor(3,201);camViewSprite.print(uiText("这张图片已选用","This photo is already selected","This photo is already selected"));camViewSprite.pushSprite(0,0);return true;
  }
  if(!camStashCurrentPhoto()) {
    camViewSprite.setFont(&fonts::efontCN_12);camViewSprite.fillRect(0,198,240,22,TFT_BLACK);camViewSprite.setTextColor(TFT_YELLOW,TFT_BLACK);camViewSprite.setCursor(3,201);camViewSprite.print(uiText("最多可选三张图片","Maximum 3 selected photos","Maximum 3 selected photos"));camViewSprite.pushSprite(0,0);return true;
  }
  camPhoto.jpeg = camAlbumJpeg; camAlbumJpeg = nullptr;
  camPhoto.size = camAlbumBytes;
  camPhoto.width = camAlbumWidth; camPhoto.height = camAlbumHeight;
  camPhoto.imagePath = camAlbumItems[camAlbumIndex].path;
  camPhoto.capturedAtEpoch = camAlbumItems[camAlbumIndex].key.epoch;
  camPhoto.confirmed = true; camPhoto.generation = ++camPhotoGeneration;
  if(!camBoundQuestionPhotoRam()) {camPhoto.clear();camRestoreLastPhoto();camAlbumLoad();return true;}
  camQuestionRequiresPhoto = true;
  camVoiceOnlyQuestion=false;camQuestionReadyForVoice=true;
  // An album selection applies to this question; it never changes the user's
  // default voice/photo mode. Short C can therefore return to voice naturally.
  lastError = lastFailedQuestion = ""; lastFailedUseSearch = false;
  // Selection preserves any voice draft and never sends a network request.
  if (!composingQuestion) appState = AppState::Idle;
  Serial.printf("[ALBUM] selected path=%s bytes=%u\n", camPhoto.imagePath.c_str(), unsigned(camPhoto.size));
  camClosePhotoUi();
  return true;
}

static void camAlbumTick() {
  if(!camMediaDelete.active && camPhotoStage==CamPhotoStage::Album && camAlbumClickPending && millis()-camAlbumClickAt>350) {
    camAlbumClickPending=false;
    if(camAlbumIndex) { --camAlbumIndex; camAlbumLoad(); }
  }
}
