#pragma once
#include <ff.h>
#include <diskio.h>
static uint32_t camSdPollAt=0,camSdMounts=0,camSdRemovals=0,camSdChecks=0;
static bool camSdCheckedMount=false,camSdSelfTestOk=false;
static uint32_t camSdFormatChallenge=0,camSdFormatAt=0;
static bool camSdSelfTest() {
  ++camSdChecks;uint8_t expected[256],actual[256];
  for(unsigned i=0;i<sizeof(expected);++i)expected[i]=uint8_t(i^0xa7);
  String path;
  do {path="/selfcheck-"+String(esp_random(),HEX)+".tmp";}while(SD_MMC.exists(path));
  File out=SD_MMC.open(path,FILE_WRITE);bool ok=out && out.write(expected,sizeof(expected))==sizeof(expected);out.flush();out.close();
  File in=SD_MMC.open(path,FILE_READ);ok=ok && in && in.read(actual,sizeof(actual))==sizeof(actual) && !memcmp(expected,actual,sizeof(actual));in.close();
  const bool removed=!SD_MMC.exists(path) || SD_MMC.remove(path);ok=ok && removed;
  camRefreshSdUsage(true);camSdSelfTestOk=ok && camSdUsageValid;
  Serial.printf("[SD-SELFTEST] pass=%d bytes=256 temp_removed=%d capacity=%s no_format=1\n",camSdSelfTestOk,removed,camSdCapacityText().c_str());
  return camSdSelfTestOk;
}
static void camSdHotplugTick() {
  if(recording || stoppingRecording || asrSessionActive || asrConnected || appState==AppState::Thinking || camVideoRecording)return;
  if(camSdPollAt && millis()-camSdPollAt<2000)return;
  camSdPollAt=millis();
  if(camSdReady) {
    alignas(4) uint8_t sector[512];
    if(!SD_MMC.readRAW(sector,0)) {
      camVideoLeave();camAlbumClickPending=false;
      camSdFailed();camSdCheckedMount=camSdSelfTestOk=false;++camSdRemovals;
      camRefreshSdUsage(true);Serial.println("[SD-HOTPLUG] removed or I/O failure; automatic remount pending");
      if(camPhotoStage==CamPhotoStage::Album)camOpenAlbum();
      else if(camPhotoStage==CamPhotoStage::VideoAlbum)camOpenVideos();
      if(uiPage==UiPage::Page1 && !cfg.clockMode)drawPage1InfoLine();return;
    }
    if(!camSdCheckedMount) {camSdCheckedMount=true;++camSdMounts;camSdSelfTest();}
  } else {
    camSdCheckedMount=false;
    if(camMountSd()) {
      ++camSdMounts;camSdCheckedMount=true;camSdSelfTest();
      Serial.println("[SD-HOTPLUG] inserted; mounted; self-test complete");
      if(camPhotoStage==CamPhotoStage::Album)camOpenAlbum();
      else if(camPhotoStage==CamPhotoStage::VideoAlbum)camOpenVideos();
      else if(uiPage==UiPage::Page1 && !cfg.clockMode)drawPage1InfoLine();
    }
  }
}
static bool camSdHotplugCommand(const String& line) {
  if(line=="sd hotplug status") {
    Serial.printf("[SD-HOTPLUG] ready=%d selftest=%d mounts=%lu removals=%lu checks=%lu capacity=%s auto_format=0\n",camSdReady,camSdSelfTestOk,
      (unsigned long)camSdMounts,(unsigned long)camSdRemovals,(unsigned long)camSdChecks,camSdCapacityText().c_str());return true;
  }
  if(line=="sd format") {
    camSdFormatChallenge=esp_random()|1U;camSdFormatAt=millis();
    Serial.printf("[SD-FORMAT] ERASES ALL TF DATA. Within 30s type: sd format ERASE %08lx\n",(unsigned long)camSdFormatChallenge);return true;
  }
  if(!line.startsWith("sd format "))return false;
  char expected[48];snprintf(expected,sizeof(expected),"sd format ERASE %08lx",(unsigned long)camSdFormatChallenge);
  if(!camSdFormatChallenge || millis()-camSdFormatAt>30000 || line!=expected) {Serial.println("[SD-FORMAT] rejected; expired or incorrect confirmation");return true;}
  camSdFormatChallenge=0;
  if(chatSessionOnTf()) {Serial.println("[SD-FORMAT] switch to flash session 1-5 before formatting TF chats");return true;}
  if(recording || stoppingRecording || asrSessionActive || asrConnected || appState==AppState::Thinking || camPhotoUiActive() || debugWebRunning || !camSdReady) {
    Serial.println("[SD-FORMAT] rejected; close camera/web UI, stop ASR and mount TF first");return true;
  }
  // SD_MMC totalBytes()/usedBytes() use this same FatFs volume. No other FAT
  // peripheral is present in this CAM build. SPIFFS and NVS are separate.
  FATFS* fs=nullptr;DWORD freeClusters=0;
  if(f_getfree("0:",&freeClusters,&fs)!=FR_OK || !fs) {Serial.println("[SD-FORMAT] rejected; filesystem unavailable");return true;}
  alignas(4) uint8_t cardSector[512],driveSector[512];
  if(!SD_MMC.readRAW(cardSector,0) || disk_read(fs->pdrv,driveSector,0,1)!=RES_OK || memcmp(cardSector,driveSector,512)) {
    Serial.println("[SD-FORMAT] rejected; volume identity mismatch");return true;
  }
  camReleasePhoto();camAlbumClear();camVideoLeave();
  void* work=heap_caps_malloc(4096,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  if(!work) {Serial.println("[SD-FORMAT] rejected; no workspace memory");return true;}
  MKFS_PARM options={};options.fmt=FM_FAT32;options.au_size=32768;
  const bool unmounted=f_mount(nullptr,"0:",0)==FR_OK;
  const FRESULT result=unmounted?f_mkfs("0:",&options,work,4096):FR_INVALID_DRIVE;
  f_mount(fs,"0:",1);heap_caps_free(work);SD_MMC.end();camSdReady=false;camSdCheckedMount=false;
  const bool mounted=camMountSd();if(mounted){camSdCheckedMount=true;camSdSelfTest();}
  Serial.printf("[SD-FORMAT] result=%d mounted=%d FAT32=1 cluster=32768\n",int(result),mounted);return true;
}
