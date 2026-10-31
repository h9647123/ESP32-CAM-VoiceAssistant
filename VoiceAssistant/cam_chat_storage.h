#pragma once

static bool chatSessionOnTf() { return cfg.currentSession > INTERNAL_SESSION_COUNT; }
static String chatTfBoundIdentity;
static bool chatStorageNotice=false;
static fs::FS& chatStore() {
  return chatSessionOnTf() ? static_cast<fs::FS&>(SD_MMC) : static_cast<fs::FS&>(SPIFFS);
}
static bool chatStorageFailure() {
  chatStorageNotice=true;
  lastError=uiText("会话存储不可用：请检查原TF卡及剩余空间",
      "Chat storage unavailable: check original TF card and free space",
      "Chat storage unavailable: check original TF card and free space");
  footerStatusDetail=lastError;
  Serial.println("[ChatHist] storage unavailable; RAM and original files retained");
  return false;
}
// A marker distinguishes hot-swapped cards without depending on an SDK-private CID.
static String chatReadTfIdentity() {
  if (!camMountSd() || !(SD_MMC.exists("/chat") || SD_MMC.mkdir("/chat"))) return String();
  const char* path="/chat/.voiceassistant-card";
  if (!SD_MMC.exists(path)) {
    char id[17];snprintf(id,sizeof(id),"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());
    File out=SD_MMC.open(path,FILE_WRITE);
    const bool ok=out && out.write((const uint8_t*)id,16)==16;out.flush();out.close();
    if (!ok) return String();
  }
  File in=SD_MMC.open(path,FILE_READ);
  if (!in || in.size()!=16) {in.close();return String();}
  char id[17]={};const bool ok=in.read((uint8_t*)id,16)==16;in.close();
  if (!ok) return String();
  for (unsigned i=0;i<16;++i) if (!((id[i]>='0'&&id[i]<='9')||(id[i]>='a'&&id[i]<='f'))) return String();
  return String(id);
}
static bool chatStorageReady() {
  if (!chatSessionOnTf()) {const bool ready=initFileSystem();if(ready)chatStorageNotice=false;return ready;}
  const String identity=chatReadTfIdentity();
  if (!identity.length() || (chatTfBoundIdentity.length() && identity!=chatTfBoundIdentity)) return chatStorageFailure();
  chatTfBoundIdentity=identity;chatStorageNotice=false;return true;
}

// Keep a last-good file across replacement. A failed/new partial .tmp is never loaded.
static bool chatRecoverFile(fs::FS& store,const String& path) {
  const String backup=path+".bak";
  return store.exists(path) || !store.exists(backup) || store.rename(backup,path);
}
static bool chatCommitFile(fs::FS& store,const String& path) {
  const String backup=path+".bak",temp=path+".tmp";
  if (store.exists(backup) && !store.remove(backup)) return false;
  const bool hadOriginal=store.exists(path);
  if (hadOriginal && !store.rename(path,backup)) return false;
  if (!store.rename(temp,path)) {
    if (hadOriginal) store.rename(backup,path);
    return false;
  }
  if (hadOriginal) store.remove(backup);
  return true;
}
