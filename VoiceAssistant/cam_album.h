#pragma once
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

// Sidecar markers leave the original JPEG and its ordering metadata intact.
// The panel has no unhide entry; serial and deleting .hidden on a PC restore it.
static bool camPhotoHidden(const String& path,bool& ioOk) {
  struct stat info={};const String marker=String("/sdcard")+path+".hidden";
  errno=0;if(stat(marker.c_str(),&info)==0){ioOk=S_ISREG(info.st_mode);return ioOk;}
  ioOk=errno==ENOENT;return false;
}
static bool camSetPhotoHidden(const String& path,bool hidden) {
  CamMediaDeleteState target;if(!target.begin(false,0,path.c_str()) || !camSdReady || !SD_MMC.exists(path))return false;
  const String marker=path+".hidden";
  bool ok=false;
  if(hidden) {
    if(SD_MMC.exists(marker))ok=true;
    else {
      String temp;do{temp=marker+"-"+String(esp_random(),HEX)+".tmp";}while(SD_MMC.exists(temp));
      File file=SD_MMC.open(temp,FILE_WRITE);ok=file && file.write((const uint8_t*)"CAMHIDE1",8)==8;
      if(file){file.flush();file.close();}
      if(ok)ok=SD_MMC.rename(temp,marker);
      if(!ok)SD_MMC.remove(temp); // Only this operation's newly created temporary marker.
    }
  }else ok=!SD_MMC.exists(marker) || SD_MMC.remove(marker);
  if(ok)camMediaChanged(path);return ok;
}
struct CamAlbumItem { String path; CamPhotoKey key; };
// Close directory handles before opening metadata. FAT's small open-file limit
// used to turn nested directory/file/metadata opens into a silent partial list.
static bool camScanMediaNames(const char* folder,const char* extension,std::vector<String>& names) {
  names.clear();const String native=String("/sdcard")+folder;
  DIR* dir=opendir(native.c_str());if(!dir)return errno==ENOENT;
  int error=0;
  for(;;){errno=0;auto* entry=readdir(dir);if(!entry){error=errno;break;}
    const String name=entry->d_name;
    if(name.endsWith(extension) && name.indexOf('/')<0 && name.indexOf("..")<0)names.push_back(String(folder)+"/"+name);
    yield();
  }
  closedir(dir);std::sort(names.begin(),names.end());return error==0;
}
// A complete index survives closing the view. A mount or media mutation
// invalidates it; failed refreshes never replace the last complete index.
struct CamMediaIndex {
  std::vector<String> names;std::vector<CamAlbumItem> items;
  size_t next=0,totalFiles=0,hiddenCount=0;uint64_t maxOrder=0;uint32_t revision=0,started=0;bool building=false,valid=false;
};
static CamMediaIndex camPhotoIndexCache,camVideoIndexCache;
static uint32_t camMediaCacheRevision(const CamMediaIndex& cache){return &cache==&camPhotoIndexCache?camPhotoIndexRevision:camVideoIndexRevision;}
static CamMediaIndex& camMediaIndex(const char* folder){return !strcmp(folder,"/photo")?camPhotoIndexCache:camVideoIndexCache;}
static bool camMediaIndexBegin(const char* folder,const char* extension,CamMediaIndex& cache) {
  std::vector<String> names,verify;
  if(!camScanMediaNames(folder,extension,names) || !camScanMediaNames(folder,extension,verify) || names!=verify)return false;
  cache.names.swap(names);cache.items.clear();cache.next=0;cache.totalFiles=cache.names.size();cache.hiddenCount=0;cache.maxOrder=0;cache.revision=camMediaCacheRevision(cache);
  cache.items.reserve(cache.names.size());cache.started=millis();cache.building=true;cache.valid=false;return true;
}
static bool camMediaIndexStep(CamMediaIndex& cache) {
  if(!camSdReady || cache.revision!=camMediaCacheRevision(cache)){cache.building=false;return false;}
  if(cache.next<cache.names.size()) {
    const String& path=cache.names[cache.next];struct stat info={};const String native=String("/sdcard")+path;
    if(stat(native.c_str(),&info)!=0){cache.building=false;Serial.printf("[MEDIA-INDEX] stat_failed errno=%d\n",errno);return false;}
    if(S_ISREG(info.st_mode)) {
      bool ioOk=true;const bool hidden=(&cache==&camPhotoIndexCache) && camPhotoHidden(path,ioOk);
      if(!ioOk){cache.building=false;return false;}
      const auto key=camReadMediaKey(path,info.st_mtime);cache.maxOrder=max(cache.maxOrder,key.order);
      if(hidden)++cache.hiddenCount;else cache.items.push_back({path,key});
    }
    ++cache.next;yield();return true;
  }
  std::sort(cache.items.begin(),cache.items.end(),[](const CamAlbumItem&a,const CamAlbumItem&b){return camPhotoNewer(a.key,a.path.c_str(),b.key,b.path.c_str());});
  if(cache.items.size()>CAM_PHOTO_ARCHIVE_LIMIT)cache.items.resize(CAM_PHOTO_ARCHIVE_LIMIT);
  alignas(4) uint8_t sector[512];if(!SD_MMC.readRAW(sector,0)){cache.building=false;return false;}
  cache.valid=true;cache.building=false;std::vector<String>().swap(cache.names);return true;
}
static bool camScanMedia(const char* folder,const char* extension,std::vector<CamAlbumItem>& result) {
  const uint32_t started=millis();auto& cache=camMediaIndex(folder);
  const bool hit=cache.valid && cache.revision==camMediaCacheRevision(cache);
  if(!hit) {
    if(!cache.building || cache.revision!=camMediaCacheRevision(cache)) {
      bool begun=false;for(unsigned attempt=0;attempt<2 && !begun;++attempt){begun=camMediaIndexBegin(folder,extension,cache);if(!begun)delay(30);}
      if(!begun)return false;
    }
    while(cache.building)if(!camMediaIndexStep(cache))return false;
  }
  alignas(4) uint8_t sector[512];if(!camSdReady || !SD_MMC.readRAW(sector,0))return false;
  result=cache.items;
  Serial.printf("[MEDIA-SCAN] kind=%s files=%u shown=%u stable=1 cache_hit=%d elapsed_ms=%lu\n",folder,unsigned(cache.totalFiles),unsigned(result.size()),hit,(unsigned long)(millis()-started));return true;
}
static std::vector<CamAlbumItem> camAlbumItems;
static size_t camAlbumIndex = 0;
static uint8_t* camAlbumJpeg = nullptr;
static size_t camAlbumBytes = 0;
static uint16_t camAlbumWidth = 0, camAlbumHeight = 0;
static LGFX_Sprite camAlbumCover(&tft);
static bool camAlbumCoverReady=false;
static String camAlbumCoverPath;
static uint32_t camAlbumCoverRevision=0;
static String camAlbumError;
static bool camAlbumScanFailed=false;
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

static bool camAlbumCoverMatches(const String& path) {
  return camAlbumCoverReady && camAlbumCoverRevision==camPhotoIndexRevision && camAlbumCoverPath==path;
}
static bool camAlbumMakeCover(const String& path,const uint8_t* jpeg,size_t bytes,uint16_t width,uint16_t height) {
  camAlbumCover.setPsram(true);camAlbumCover.setColorDepth(16);
  if(!camAlbumCoverReady)camAlbumCoverReady=camAlbumCover.createSprite(240,192)!=nullptr;
  if(!camAlbumCoverReady)return false;
  camAlbumCoverPath="";camAlbumCover.fillScreen(TFT_BLACK);
  const float scale=min(240.0f/width,192.0f/height);
  if(!camAlbumCover.drawJpg(jpeg,bytes,(240-int(width*scale))/2,(192-int(height*scale))/2,0,0,0,0,scale,scale))return false;
  camAlbumCoverPath=path;camAlbumCoverRevision=camPhotoIndexRevision;return true;
}
static void camAlbumCoverWarmTick() {
  static uint32_t retryAt=0;
  if(uiPage!=UiPage::Page1 || millis()-lastInteractionAt<2000 || (retryAt && millis()-retryAt<10000))return;
  if(!camPhotoIndexCache.valid || camPhotoIndexCache.revision!=camPhotoIndexRevision || camPhotoIndexCache.items.empty())return;
  const auto& path=camPhotoIndexCache.items.front().path;if(camAlbumCoverMatches(path))return;
  retryAt=millis();const uint32_t began=millis();File file=SD_MMC.open(path,FILE_READ);const size_t bytes=file?file.size():0;
  if(bytes<4 || bytes>CAM_JPEG_MAX_BYTES)return;
  auto* jpeg=(uint8_t*)heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!jpeg)return;
  bool ok=file.read(jpeg,bytes)==bytes;file.close();uint16_t w=0,h=0;
  ok=ok && camAlbumJpegDimensions(jpeg,bytes,w,h) && camAlbumMakeCover(path,jpeg,bytes,w,h);heap_caps_free(jpeg);
  Serial.printf("[ALBUM-COVER] warm=%d bytes=%u elapsed_ms=%lu\n",ok,unsigned(bytes),(unsigned long)(millis()-began));
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
  if(camAlbumScanFailed)camViewSprite.print(uiText("扫描未完成，B/C重试","Scan incomplete; B/C retry","Scan incomplete; B/C retry"));
  else if (!camAlbumItems.empty()) camViewSprite.print(camAlbumTimeLabel(camAlbumItems[camAlbumIndex].key));
  if (camAlbumJpeg) {
    const float scale = min(240.0f / camAlbumWidth, 192.0f / camAlbumHeight);
    const auto& path=camAlbumItems[camAlbumIndex].path;
    bool cached=camAlbumCoverMatches(path);
    if(!cached && camAlbumIndex==0)cached=camAlbumMakeCover(path,camAlbumJpeg,camAlbumBytes,camAlbumWidth,camAlbumHeight);
    if(cached)camAlbumCover.pushSprite(&camViewSprite,0,32);
    else {
      camViewSprite.setClipRect(0,32,240,192);
      camViewSprite.drawJpg(camAlbumJpeg,camAlbumBytes,(240-int(camAlbumWidth*scale))/2,
          32+(192-int(camAlbumHeight*scale))/2,0,0,0,0,scale,scale);
      camViewSprite.clearClipRect();
    }
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
  const uint32_t began=millis();
  if (camAlbumJpeg) heap_caps_free(camAlbumJpeg);
  camAlbumJpeg = nullptr; camAlbumBytes = 0;
  camAlbumError = camSdReady ? (camAlbumScanFailed?uiText("相册扫描失败，请重试","Album scan failed; retry","Album scan failed; retry"):uiText("暂无照片","No photos","No photos")) : uiText("未连接 TF 卡","TF card unavailable","TF card unavailable");
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
  const uint32_t readDone=millis();camAlbumDraw();
  Serial.printf("[ALBUM-LOAD] bytes=%u read_ms=%lu draw_ms=%lu total_ms=%lu ok=%d\n",unsigned(camAlbumBytes),(unsigned long)(readDone-began),(unsigned long)(millis()-readDone),(unsigned long)(millis()-began),ok);
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
  const uint32_t openedAt=millis();
  menuSelectItem(MenuAlbum);
  if(!camAcquireModelTransport())return;
  camStopPreviewCamera();auto previous=std::move(camAlbumItems);camAlbumClear();
  camAlbumScanFailed=false;
  camViewSprite.setPsram(true); camViewSprite.setColorDepth(16);
  if (!camViewReady) camViewReady = camViewSprite.createSprite(240, 240) != nullptr;
  if (!camViewReady) { setState(AppState::Error, "Album memory unavailable"); return; }
  if (camSdReady) {
    if(!camScanMedia("/photo",".jpg",camAlbumItems)) {
      camAlbumScanFailed=true;
      camAlbumItems=std::move(previous);
      Serial.println("[ALBUM] incomplete scan; retaining previous index; retry after card check");
    }
  }
  ++buttonUiGeneration;
  camPhotoStage = CamPhotoStage::Album; uiPage = UiPage::CameraView;
  camAlbumLoad();
  Serial.printf("[ALBUM] opened count=%u newest_first=1 total_ms=%lu\n", unsigned(camAlbumItems.size()),(unsigned long)(millis()-openedAt));
}

static bool camAlbumHandleKey(char key, bool click, bool longPress) {
  if(camAlbumScanFailed && click && (key=='B' || key=='C')){camOpenAlbum();return true;}
  if (camMediaDelete.active) {
    const auto action = camMediaDelete.action(key, click, longPress);
    if (action == CamMediaDeleteState::Action::Cancel) {
      camMediaDeleteClose(); camAlbumDraw();
    } else if(action==CamMediaDeleteState::Action::Advance) {
      ++buttonUiGeneration;camMediaDeleteDraw();
    } else if(action==CamMediaDeleteState::Action::Hide) {
      const bool valid=camAlbumIndex<camAlbumItems.size() && camMediaDelete.matches(false,camAlbumIndex,camAlbumItems[camAlbumIndex].path.c_str());
      if(!valid){camMediaDeleteClose();camAlbumDraw();return true;}
      if(!camSetPhotoHidden(camMediaDelete.path,true)){camMediaDeleteDraw(true);return true;}
      Serial.println(String("[ALBUM] hidden original_retained=1 path=")+camMediaDelete.path);
      camAlbumItems.erase(camAlbumItems.begin()+camAlbumIndex);
      if(camAlbumIndex>=camAlbumItems.size())camAlbumIndex=camAlbumItems.empty()?0:camAlbumItems.size()-1;
      camMediaDeleteClose();camAlbumLoad();
    } else if (action == CamMediaDeleteState::Action::Confirm) {
      const bool targetValid = camAlbumIndex < camAlbumItems.size() &&
          camMediaDelete.matches(false, camAlbumIndex, camAlbumItems[camAlbumIndex].path.c_str());
      if (!targetValid) { camMediaDeleteClose(); camAlbumDraw(); return true; }
      camMediaDeleteDraw(false,true);
      if (!camSdReady || !SD_MMC.remove(camMediaDelete.path)) { camMediaDeleteDraw(true); return true; }
      SD_MMC.remove(String(camMediaDelete.path)+".meta");
      camMediaChanged(camMediaDelete.path);camChatThumbInvalidate();
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
    if (key == 'B') next=(next+1)%camAlbumItems.size();
    if (key == 'C') next=(next+camAlbumItems.size()-1)%camAlbumItems.size();
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
    if(!camAlbumItems.empty()) { camAlbumIndex=(camAlbumIndex+camAlbumItems.size()-1)%camAlbumItems.size(); camAlbumLoad(); }
  }
}
