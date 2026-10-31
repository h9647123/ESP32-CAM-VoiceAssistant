#pragma once
#include <unistd.h>
#include "cam_avi_format.h"
#include "cam_video_mic.h"
static File camVideoFile,camVideoPlayback;
static String camVideoPart,camVideoLastPath;
static uint32_t camVideoStarted=0,camVideoFrames=0,camVideoLargest=0,camVideoLastFrame=0;
static bool camVideoPreviewOnly=false;
static uint32_t camVideoAudioBytes=0;
static uint32_t camVideoPlayAt=0,camVideoPlayPeriod=100,camVideoPlayEnd=0,camVideoPlayFrame=0;
static uint16_t camVideoWidth=0,camVideoHeight=0;
static std::vector<CamAlbumItem> camVideoItems;
static size_t camVideoIndex=0;
static bool camVideoClickPending=false;
static uint32_t camVideoClickAt=0;
static String camVideoDeleteTarget;
static uint8_t* camVideoDecode=nullptr;
static size_t camVideoDecodeCapacity=0;
static String camVideoSizeText() {
  return camVideoResolution==0 ? "QVGA 320x240" : camVideoResolution==2 ? "UXGA 1600x1200" : "VGA 640x480";
}
static bool camFinalizeAvi(const String& path,uint32_t elapsed) {
  File scan=SD_MMC.open(path,FILE_READ);
  uint8_t head[CAM_AVI_HEADER];
  if(!scan || scan.read(head,sizeof(head))!=sizeof(head) || memcmp(head,"RIFF",4) || memcmp(head+CAM_AVI_MOVI,"movi",4)) { scan.close(); return false; }
  uint32_t at=CAM_AVI_HEADER,count=0,largest=0,videos=0,audioBytes=0; const uint32_t bytes=scan.size();
  uint8_t chunk[8],ends[2];
  while(at+8<=bytes && count<40000) {
    scan.seek(at); if(scan.read(chunk,8)!=8)break;
    const bool video=!memcmp(chunk,"00dc",4),audio=!memcmp(chunk,"01wb",4);
    if(!video && !audio)break;
    uint32_t n=aviRead32(chunk+4);
    if(n<4 || n>CAM_JPEG_MAX_BYTES || n>bytes-at-8)break;
    if(video) {
      scan.seek(at+8); if(scan.read(ends,2)!=2 || ends[0]!=255 || ends[1]!=216)break;
      scan.seek(at+8+n-2); if(scan.read(ends,2)!=2 || ends[0]!=255 || ends[1]!=217)break;
      ++videos;largest=max(largest,n);
    } else {if(n&1)break;audioBytes+=n;}
    at+=8+n+(n&1); ++count;yield();
  }
  if(!videos) { scan.close();return false; }
  File out=SD_MMC.open(path,"r+");
  bool ok=out && out.seek(at);
  uint8_t index[16];memcpy(index,"idx1",4);aviPut32(index+4,count*16);
  ok=ok && out.write(index,8)==8;
  for(uint32_t pos=CAM_AVI_HEADER,i=0;ok && i<count;++i) {
    scan.seek(pos);ok=scan.read(chunk,8)==8;uint32_t n=aviRead32(chunk+4);
    memcpy(index,chunk,4);aviPut32(index+4,0x10);aviPut32(index+8,pos-CAM_AVI_MOVI);aviPut32(index+12,n);
    ok=ok && out.write(index,16)==16;pos+=8+n+(n&1);yield();
  }
  aviHeader(head,aviRead32(head+176),aviRead32(head+180),videos,max<uint32_t>(1,elapsed),at-CAM_AVI_HEADER,largest,true,audioBytes,count);
  ok=ok && out.seek(0) && out.write(head,sizeof(head))==sizeof(head);
  out.flush();const uint32_t actualSize=out.size();out.close();scan.close();
  if(ok && actualSize!=at+8+count*16)ok=::truncate((String("/sdcard")+path).c_str(),at+8+count*16)==0;
  return ok;
}
static bool camVideoDrainAudio() {
  for(unsigned part=0;part<32 && camVideoMic.available();++part) {
    const uint32_t tail=camVideoMic.consumed.load(),available=camVideoMic.available();
    const size_t n=min<uint32_t>(6400,min<uint32_t>(available,CamVideoMic::CAPACITY-tail%CamVideoMic::CAPACITY));
    uint8_t header[8];memcpy(header,"01wb",4);aviPut32(header+4,n);
    if(!camVideoFile || camVideoFile.write(header,8)!=8 || camVideoFile.write(camVideoMic.pcm+tail%CamVideoMic::CAPACITY,n)!=n)return false;
    camVideoAudioBytes+=n;camVideoMic.consumed.store(tail+n,std::memory_order_release);
  }
  return true;
}
static void camVideoStop() {
  if(!camVideoRecording)return;
  camVideoRecording=false;const bool micStopped=camVideoMic.stop();
  const bool audioWritten=micStopped && camVideoDrainAudio();
  camVideoFile.flush();camVideoFile.close();
  const uint32_t elapsed=camVideoAudioBytes?max<uint32_t>(1,camVideoAudioBytes/32):max<uint32_t>(1,millis()-camVideoStarted);
  const bool ok=camSdReady && camFinalizeAvi(camVideoPart,elapsed);
  const String final=camVideoPart.substring(0,camVideoPart.length()-5)+".avi";
  const bool saved=ok && SD_MMC.rename(camVideoPart,final);
  if(saved)camVideoLastPath=final;
  const bool audioOk=audioWritten && camVideoMic.good();
  camVideoNotice=saved ? (audioOk?uiText("有声录像已保存","Video + audio saved","Video + audio saved"):uiText("录像已保存，音频不完整","Video saved; incomplete audio","Video saved; incomplete audio")) : uiText("保存失败，保留待恢复录像","Save failed; recovery file kept","Save failed; recovery file kept");
  Serial.printf("[VIDEO] stopped saved=%d frames=%lu elapsed_ms=%lu fps_x100=%lu path=%s\n",saved,(unsigned long)camVideoFrames,(unsigned long)elapsed,
      (unsigned long)(uint64_t(camVideoFrames)*100000/elapsed),saved?final.c_str():camVideoPart.c_str());
  Serial.printf("[VIDEO-AUDIO] ok=%d samples=%lu bytes=%lu ring_overflow=%lu i2s_overflow=%lu read_errors=%lu clips=%lu\n",audioOk,
    (unsigned long)(camVideoAudioBytes/2),(unsigned long)camVideoAudioBytes,(unsigned long)camVideoMic.overflow,(unsigned long)camVideoMic.i2sOverflow,(unsigned long)camVideoMic.readErrors,(unsigned long)camVideoMic.clips);
  if(!camVideoFrames && camSdReady)SD_MMC.remove(camVideoPart); // Only this session's empty file.
  camStopPreviewCamera();
}
static void camVideoLeave() {
  camVideoClickPending=false;camVideoDeleteTarget="";camMediaDeleteClose();
  camVideoStop();camVideoPlaying=false;camVideoPlayback.close();
  if(camVideoDecode)heap_caps_free(camVideoDecode);camVideoDecode=nullptr;camVideoDecodeCapacity=0;
  std::vector<CamAlbumItem>().swap(camVideoItems);
}
static void camVideoStart() {
  if(!camPhotoOnly || camPhotoStage!=CamPhotoStage::Preview || !camSdReady)return;
  cancelServiceProbe();
  camRefreshSdUsage(true);
  if(!camSdUsageValid || camSdTotalBytes-camSdUsedBytes<4*1024*1024) {
    camVideoNotice=uiText("TF空间不足","TF storage full","TF storage full");return;
  }
  camStopPreviewCamera();camFrameSize=camVideoResolution==0?FRAMESIZE_QVGA:camVideoResolution==2?FRAMESIZE_UXGA:FRAMESIZE_VGA;
  String error;
  if(!camInitCamera(error)) { camVideoNotice=uiErrorText(error);return; }
  for(int i=0;i<2;++i) {camera_fb_t* stale=esp_camera_fb_get();if(stale)esp_camera_fb_return(stale);}
  if(!SD_MMC.exists("/video") && !SD_MMC.mkdir("/video"))return;
  (void)camNextPhotoPath(time(nullptr));
  File videoDir=SD_MMC.open("/video");
  for(File f=videoDir.openNextFile();f;f=videoDir.openNextFile()) {
    String key=f.name();key=key.substring(key.lastIndexOf('/')+1);
    if(key.startsWith("V") && (key.endsWith(".avi") || key.endsWith(".part"))) {
      key.setCharAt(0,'P');key=key.substring(0,key.lastIndexOf('.'))+".jpg";
      camPhotoSequence=max(camPhotoSequence,camParsePhotoKey(key.c_str()).order);
    }f.close();yield();
  }videoDir.close();
  char path[96];const int64_t epoch=time(nullptr);
  // Shared persisted photo sequence keeps recordings ordered without NTP.
  do { snprintf(path,sizeof(path),"/video/V%016llx_T%010llu.part",(unsigned long long)++camPhotoSequence,
      (unsigned long long)(camValidPhotoTime(epoch)?epoch:0)); } while(SD_MMC.exists(path) || SD_MMC.exists(String(path).substring(0,strlen(path)-5)+".avi"));
  camVideoPart=path;camVideoFile=SD_MMC.open(path,FILE_WRITE);
  camVideoWidth=camVideoResolution==0?320:camVideoResolution==2?1600:640;camVideoHeight=camVideoWidth*3/4;
  uint8_t header[CAM_AVI_HEADER];aviHeader(header,camVideoWidth,camVideoHeight,0,100,0,0,false);
  if(!camVideoFile || camVideoFile.write(header,sizeof(header))!=sizeof(header)) { camVideoFile.close();camVideoNotice=uiText("无法写入TF卡","Cannot write TF","Cannot write TF");return; }
  if(!camVideoMic.begin()) {camVideoFile.close();SD_MMC.remove(camVideoPart);camVideoNotice=uiText("麦克风启动失败","Microphone start failed","Microphone start failed");return;}
  camVideoFile.flush();camVideoFrames=camVideoLargest=camVideoAudioBytes=0;camVideoStarted=millis();camVideoLastFrame=0;
  camVideoNotice="";camVideoRecording=true;
  Serial.printf("[VIDEO] started width=%u height=%u audio=PCM16_16k_mono stop_on_B_release=1 path=%s\n",camVideoWidth,camVideoHeight,path);
}
static void camVideoDrawOverlay() {
  camViewSprite.fillRect(0,0,240,23,TFT_BLACK);camViewSprite.setFont(&fonts::efontCN_12);camViewSprite.setTextSize(1);
  camViewSprite.setTextColor(TFT_RED,TFT_BLACK);camViewSprite.setCursor(3,3);
  camViewSprite.print(String("REC ")+String((millis()-camVideoStarted)/1000)+"s  "+camVideoSizeText());
  camViewSprite.fillRect(0,217,240,23,TFT_BLACK);camViewSprite.setTextColor(TFT_WHITE,TFT_BLACK);camViewSprite.setCursor(3,221);
  camViewSprite.print(uiText("松开B结束录像","Release B to stop","Release B to stop"));
  if(camVideoResolution==2) {camViewSprite.setTextColor(TFT_YELLOW,TFT_BLACK);camViewSprite.setCursor(3,24);camViewSprite.print(uiText("性能受限，帧率可能过低","Limited performance; low FPS","Limited performance; low FPS"));}
  camViewSprite.pushSprite(0,0);
}
static void camVideoLibraryDraw() {
  if(!camViewReady)return;
  camViewSprite.fillScreen(TFT_BLACK);camViewSprite.setTextSize(1);camViewSprite.setFont(&fonts::efontCN_16);
  camViewSprite.setTextColor(TFT_CYAN,TFT_BLACK);camViewSprite.setCursor(3,2);
  camViewSprite.print(uiText("视频集 ","Videos ","Videos ")+String(camVideoItems.empty()?0:camVideoIndex+1)+"/"+String(camVideoItems.size()));
  camViewSprite.setFont(&fonts::efontCN_12);camViewSprite.setCursor(3,23);
  if(!camVideoItems.empty())camViewSprite.print(camAlbumTimeLabel(camVideoItems[camVideoIndex].key));
  else {camViewSprite.setCursor(3,100);camViewSprite.print(uiText("暂无视频或TF卡未就绪","No videos or TF unavailable","No videos or TF unavailable"));}
  camViewSprite.setCursor(3,223);camViewSprite.setTextColor(TFT_WHITE,TFT_BLACK);
  camViewSprite.print(uiText("A播放 B旧 C新 双C删 长A退","A:play B:old C:new 2C:del Hold A:back","A:play B:old C:new 2C:del Hold A:back"));
}
static bool camVideoLoad(bool autoplay=false) {
  camVideoPlayback.close();camVideoPlaying=false;camVideoPlayFrame=0;
  if(camVideoItems.empty()) {camVideoLibraryDraw();camViewSprite.pushSprite(0,0);return false;}
  camVideoPlayback=SD_MMC.open(camVideoItems[camVideoIndex].path,FILE_READ);uint8_t header[CAM_AVI_HEADER];
  if(!camVideoPlayback || camVideoPlayback.read(header,sizeof(header))!=sizeof(header) || memcmp(header+CAM_AVI_MOVI,"movi",4)) {camVideoPlayback.close();return false;}
  camVideoPlayPeriod=max<uint32_t>(1,aviRead32(header+32)/1000);
  camVideoPlayEnd=CAM_AVI_MOVI+aviRead32(header+CAM_AVI_MOVI-4);
  if(camVideoPlayEnd>camVideoPlayback.size() || camVideoPlayEnd<CAM_AVI_HEADER+8) {camVideoPlayback.close();return false;}
  camVideoPlayAt=0;camVideoPlaying=true;camVideoPreviewOnly=!autoplay;return true;
}
static void camOpenVideos() {
  if(recording || stoppingRecording || asrSessionActive || appState==AppState::Thinking)return;
  cancelServiceProbe();camVideoLeave();camStopPreviewCamera();camAlbumClear();
  camViewSprite.setPsram(true);camViewSprite.setColorDepth(16);if(!camViewReady)camViewReady=camViewSprite.createSprite(240,240)!=nullptr;
  if(!camViewReady)return;
  menuSelectItem(MenuVideos);camVideoIndex=0;
  if(camSdReady) {
    File dir=SD_MMC.open("/video");
    for(File f=dir.openNextFile();f;f=dir.openNextFile()) {
      String path=f.path();
      if(!f.isDirectory() && path.endsWith(".avi")) {
        String key=path.substring(path.lastIndexOf('/')+1);key.setCharAt(0,'P');key=key.substring(0,key.length()-4)+".jpg";
        CamAlbumItem item{path,camParsePhotoKey(key.c_str(),f.getLastWrite())};
        auto pos=std::lower_bound(camVideoItems.begin(),camVideoItems.end(),item,[](const CamAlbumItem&a,const CamAlbumItem&b){return camPhotoNewer(a.key,a.path.c_str(),b.key,b.path.c_str());});
        camVideoItems.insert(pos,item);if(camVideoItems.size()>200)camVideoItems.pop_back();
      }
      f.close();yield();
    }dir.close();
  }
  ++buttonUiGeneration;camPhotoStage=CamPhotoStage::VideoAlbum;uiPage=UiPage::CameraView;
  camVideoLibraryDraw();camViewSprite.pushSprite(0,0);
  camVideoLoad(); // First frame is drawn by tick, then paused below.
  Serial.printf("[VIDEOS] count=%u newest_first=1\n",unsigned(camVideoItems.size()));
}
static bool camVideoHandleKey(char key,bool click,bool longPress,bool released) {
  if(camPhotoStage==CamPhotoStage::VideoAlbum) {
    if(camMediaDelete.active) {
      const auto action=camMediaDelete.action(key,click,longPress);
      if(action==CamMediaDeleteState::Action::Cancel) {
        camMediaDeleteClose();camVideoLibraryDraw();camViewSprite.pushSprite(0,0);camVideoLoad();
      } else if(action==CamMediaDeleteState::Action::Confirm) {
        const bool targetValid=camVideoIndex<camVideoItems.size() &&
            camMediaDelete.matches(true,camVideoIndex,camVideoItems[camVideoIndex].path.c_str());
        if(!targetValid) {camMediaDeleteClose();camVideoLibraryDraw();camViewSprite.pushSprite(0,0);camVideoLoad();return true;}
        if(!camSdReady || !SD_MMC.remove(camMediaDelete.path)) {camMediaDeleteDraw(true);return true;}
        Serial.println(String("[VIDEOS] deleted ")+camMediaDelete.path);
        camVideoItems.erase(camVideoItems.begin()+camVideoIndex);
        if(camVideoIndex>=camVideoItems.size())camVideoIndex=camVideoItems.empty()?0:camVideoItems.size()-1;
        camMediaDeleteClose();camVideoLibraryDraw();camViewSprite.pushSprite(0,0);camVideoLoad();
      }
      return true;
    }
    if(key=='A' && longPress){camClosePhotoUi(true);return true;}
    if(!click)return true;
    if(key=='C' && !camVideoItems.empty()) {
      if(camVideoClickPending && millis()-camVideoClickAt<=350 && camVideoItems[camVideoIndex].path==camVideoDeleteTarget) {
        camVideoClickPending=false;camVideoPlaying=false;camVideoPlayback.close();
        camMediaDeleteOpen(true,camVideoIndex,camVideoDeleteTarget);
      } else {camVideoClickPending=true;camVideoClickAt=millis();camVideoDeleteTarget=camVideoItems[camVideoIndex].path;}
      return true;
    }
    camVideoClickPending=false;
    if(key=='A') {if(!camVideoPlayback || camVideoPlayback.position()>=camVideoPlayEnd)camVideoLoad(true);else {camVideoPlaying=!camVideoPlaying;camVideoPreviewOnly=false;camVideoPlayAt=millis();}}
    else if(key=='B' && !camVideoItems.empty()) {if(camVideoIndex+1<camVideoItems.size())++camVideoIndex;camVideoLoad();}
    return true;
  }
  if(camVideoRecording) { if(key=='B' && released)camVideoStop();else if(key=='A' && longPress){camVideoStop();camClosePhotoUi(true);}return true; }
  if(camPhotoOnly && camPhotoStage==CamPhotoStage::Preview && key=='B' && longPress) {camVideoStart();return true;}
  return false;
}
static void camVideoTick() {
  if(camPhotoStage==CamPhotoStage::VideoAlbum && camMediaDelete.active)return;
  if(camPhotoStage==CamPhotoStage::VideoAlbum && camVideoClickPending && millis()-camVideoClickAt>350) {
    camVideoClickPending=false;
    if(camVideoIndex) {--camVideoIndex;camVideoLoad();}
  }
  if(camVideoRecording) {
    if(!camVideoMic.running || !camVideoDrainAudio()) {camVideoStop();return;}
    if(millis()-camVideoStarted>=600000 || camVideoFrames>=18000 || camVideoFile.position()>512UL*1024*1024) {camVideoStop();return;}
    const uint32_t interval=camVideoResolution==2?1000:camVideoResolution==0?100:200;
    if(camVideoLastFrame && millis()-camVideoLastFrame<interval)return;
    camVideoLastFrame=millis();camera_fb_t* frame=esp_camera_fb_get();
    if(!frame) {camVideoStop();camVideoNotice=uiText("采集失败，已结束录像","Capture failed; stopped","Capture failed; stopped");return;}
    uint8_t chunk[8];memcpy(chunk,"00dc",4);aviPut32(chunk+4,frame->len);
    bool ok=frame->format==PIXFORMAT_JPEG && frame->width==camVideoWidth && frame->height==camVideoHeight && frame->len<=CAM_JPEG_MAX_BYTES;
    ok=ok && camVideoFile.write(chunk,8)==8 && camVideoFile.write(frame->buf,frame->len)==frame->len;
    if(ok && (frame->len&1)) {const uint8_t zero=0;ok=camVideoFile.write(&zero,1)==1;}
    if(ok) {++camVideoFrames;camVideoLargest=max<uint32_t>(camVideoLargest,frame->len);}
    // Preview decoding is limited to 2 Hz so SD capture gets the other frames.
    if(ok && (camVideoFrames==1 || millis()-camPreviewLastAt>=500)) {camDrawPhotoView(frame->buf,frame->len,frame->width,frame->height);camVideoDrawOverlay();camPreviewLastAt=millis();}
    esp_camera_fb_return(frame);
    if(!ok) {camVideoStop();camVideoNotice=uiText("TF写入失败，录像已结束","TF write failed; stopped","TF write failed; stopped");}
    else if(camVideoFrames%8==0)camVideoFile.flush();
    return;
  }
  if(camPhotoStage!=CamPhotoStage::VideoAlbum || !camVideoPlaying || !camVideoPlayback)return;
  if(camVideoPlayAt && millis()-camVideoPlayAt<camVideoPlayPeriod)return;
  const uint32_t started=millis();
  if(camVideoPlayback.position()+8>camVideoPlayEnd) {camVideoPlaying=false;return;}
  uint8_t chunk[8];
  for(unsigned i=0;;++i) {
    if(i>64 || camVideoPlayback.position()+8>camVideoPlayEnd || camVideoPlayback.read(chunk,8)!=8) {camVideoPlaying=false;return;}
    if(!memcmp(chunk,"00dc",4))break;
    const size_t skip=aviRead32(chunk+4);
    if(memcmp(chunk,"01wb",4) || skip>camVideoPlayEnd-camVideoPlayback.position()) {camVideoPlaying=false;return;}
    camVideoPlayback.seek(camVideoPlayback.position()+skip+(skip&1));
  }
  const size_t n=aviRead32(chunk+4);
  if(n<4 || n>CAM_JPEG_MAX_BYTES || n>camVideoPlayEnd-camVideoPlayback.position()) {camVideoPlaying=false;return;}
  if(n>camVideoDecodeCapacity) {if(camVideoDecode)heap_caps_free(camVideoDecode);camVideoDecode=(uint8_t*)heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);camVideoDecodeCapacity=camVideoDecode?n:0;}
  if(!camVideoDecode || camVideoPlayback.read(camVideoDecode,n)!=n) {camVideoPlaying=false;return;}
  if(n&1)camVideoPlayback.seek(camVideoPlayback.position()+1);
  uint16_t w,h;if(!camAlbumJpegDimensions(camVideoDecode,n,w,h)) {camVideoPlaying=false;return;}
  camVideoLibraryDraw();const float scale=min(240.0f/w,180.0f/h);
  camViewSprite.setClipRect(0,35,240,180);camViewSprite.drawJpg(camVideoDecode,n,(240-int(w*scale))/2,35+(180-int(h*scale))/2,0,0,0,0,scale,scale);camViewSprite.clearClipRect();camViewSprite.pushSprite(0,0);
  camVideoPlayAt=started;++camVideoPlayFrame;
  if(camVideoPreviewOnly)camVideoPlaying=false;
  Serial.printf("[VIDEO-PLAY] index=%u frame=%lu\n",unsigned(camVideoIndex+1),(unsigned long)camVideoPlayFrame);
}
