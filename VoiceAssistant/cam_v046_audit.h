#pragma once
static void camDraftLifecycleAudit() {
  if(recording || stoppingRecording || asrSessionActive || asrConnected)return;
  const auto savedSegments=pendingSegments;const String savedQuestion=pendingQuestion,savedTranscript=transcript;
  const String best=asrBestTranscript,stable=asrStableTranscript,partial=asrPartialTranscript;
  const bool composing=composingQuestion,append=asrAppendMode;
  pendingSegments.clear();pendingQuestion=transcript="";composingQuestion=asrAppendMode=false;
  commitAsrDraftSegment("第一段函数题。");refreshComposedTranscript();
  bool ok=transcript=="第一段函数题。" && pendingSegments.size()==1 && !asrBestTranscript.length();
  asrAppendMode=true;asrBestTranscript="第二段自然停顿。";
  commitAsrDraftSegment(asrBestTranscript);refreshComposedTranscript();
  ok=ok && transcript=="第一段函数题。第二段自然停顿。" && pendingSegments.size()==2;
  commitAsrDraftSegment("第二段自然停顿。");
  ok=ok && pendingSegments.size()==3 && transcript=="第一段函数题。第二段自然停顿。第二段自然停顿。";
  pendingSegments=savedSegments;pendingQuestion=savedQuestion;transcript=savedTranscript;
  asrBestTranscript=best;asrStableTranscript=stable;asrPartialTranscript=partial;composingQuestion=composing;asrAppendMode=append;
  markChatLinesDirty();redrawCurrentUiPage();
  Serial.printf("[DRAFT-LIFECYCLE] pass=%d checks=3 preserves_spoken_repeats=1 history_changed=0\n",ok);
}
static void camVideoAudit(const String& path) {
  File f=SD_MMC.open(path,FILE_READ);uint8_t header[CAM_AVI_HEADER],chunk[8],index[16];
  bool ok=f && f.read(header,sizeof(header))==sizeof(header) && !memcmp(header,"RIFF",4) && !memcmp(header+CAM_AVI_MOVI,"movi",4);
  uint32_t count=0,videos=0,audioBytes=0,at=CAM_AVI_HEADER,ending=ok?CAM_AVI_MOVI+aviRead32(header+CAM_AVI_MOVI-4):0;
  if(ok)ok=ending<=f.size() && aviRead32(header+4)+8==f.size();
  while(ok && at+8<=ending) {
    f.seek(at);ok=f.read(chunk,8)==8 && (!memcmp(chunk,"00dc",4) || !memcmp(chunk,"01wb",4));uint32_t n=aviRead32(chunk+4);
    if(!ok || n<4 || n>CAM_JPEG_MAX_BYTES || n>ending-at-8) {ok=false;break;}
    if(!memcmp(chunk,"00dc",4))++videos;else audioBytes+=n;
    f.seek(ending+8+count*16);ok=f.read(index,16)==16 && !memcmp(index,chunk,4) && aviRead32(index+8)==at-CAM_AVI_MOVI && aviRead32(index+12)==n;
    at+=8+n+(n&1);++count;yield();
  }
  ok=ok && videos>0 && audioBytes>0 && at==ending && videos==aviRead32(header+48) && audioBytes/2==aviRead32(header+264);
  Serial.printf("[VIDEO-AUDIT] pass=%d frames=%lu audio_bytes=%lu width=%lu height=%lu bytes=%u indexed=1\n",ok,(unsigned long)videos,(unsigned long)audioBytes,
    ok?(unsigned long)aviRead32(header+176):0,ok?(unsigned long)aviRead32(header+180):0,unsigned(f.size()));f.close();
}
static bool camV046Command(const String& line) {
  if(line=="draft lifecycle audit") {camDraftLifecycleAudit();return true;}
  if(line=="ui motion status") {
    Serial.printf("[UI-MOTION] frames=%lu transitions=%lu group=%d index=%u chord=%d scroll=%ld target=%ld\n",
      (unsigned long)menuAnimationFrames,(unsigned long)pageTransitionCount,menuGroup,menuIndex,chatRoundChordDir,(long)chatScrollPixels,(long)chatScrollTargetPixels);return true;
  }
  if(line=="cam only on" || line=="cam only off") {
    if(camPhotoUiActive() || recording || composingQuestion) {Serial.println("[CAM-ONLY] busy");return true;}
    camPhotoOnly=line.endsWith(" on");camSaveSettings();drawPage1();Serial.printf("[CAM-ONLY] enabled=%d\n",camPhotoOnly);return true;
  }
  if(line.startsWith("video size ")) {
    if(camVideoRecording)return true;
    const String value=line.substring(11);
    if(value!="qvga" && value!="vga" && value!="uxga") {Serial.println("[VIDEO] invalid size");return true;}
    camVideoResolution=value=="qvga"?0:value=="uxga"?2:1;camSaveSettings();
    Serial.println("[VIDEO] size="+camVideoSizeText()+(camVideoResolution==2?" WARNING: limited performance; low FPS":""));return true;
  }
  if(line=="video library") {camOpenVideos();return true;}
  if(line=="video status") {
    Serial.printf("[VIDEO] recording=%d playing=%d count=%u index=%u frames=%lu play_frame=%lu size=%u last=%s\n",camVideoRecording,camVideoPlaying,
      unsigned(camVideoItems.size()),unsigned(camVideoIndex+1),(unsigned long)camVideoFrames,(unsigned long)camVideoPlayFrame,camVideoResolution,camVideoLastPath.c_str());return true;
  }
  if(line=="video audit") {
    camVideoAudit(!camVideoItems.empty()?camVideoItems[camVideoIndex].path:camVideoLastPath);return true;
  }
  if(line=="video export") {
    if(camVideoRecording || camVideoPlaying) {Serial.println("[VIDEO-EXPORT] busy");return true;}
    const String path=!camVideoItems.empty()?camVideoItems[camVideoIndex].path:camVideoLastPath;
    File f=SD_MMC.open(path,FILE_READ);if(!f) {Serial.println("[VIDEO-EXPORT] unavailable");return true;}
    ++buttonUiGeneration;Serial.printf("[VIDEO-BEGIN] bytes=%u\n",unsigned(f.size()));Serial.flush();uint8_t block[1024];
    while(f.available()) {const size_t n=f.read(block,sizeof(block));if(!n)break;Serial.write(block,n);yield();}
    f.close();Serial.println("\n[VIDEO-END]");++buttonUiGeneration;return true;
  }
  if(line.startsWith("video recover ")) {
    const String path=line.substring(14);
    if(camVideoRecording || camVideoPlaying || !path.startsWith("/video/") || !path.endsWith(".part") || path.indexOf("..")>=0) {Serial.println("[VIDEO-RECOVER] rejected");return true;}
    String final=path.substring(0,path.length()-5)+".avi";
    bool ok=!SD_MMC.exists(final) && camFinalizeAvi(path,1000);
    ok=ok && SD_MMC.rename(path,final);Serial.printf("[VIDEO-RECOVER] pass=%d duration_estimated=1\n",ok);return true;
  }
  return false;
}
