#pragma once
// Read-only export: no session switch, repair, cleanup or filesystem writes.
static void camExportTfBackup() {
  if(!camSdReady || recording || stoppingRecording || asrSessionActive || appState==AppState::Thinking || camPhotoUiActive()) {
    Serial.println("[SD-BACKUP-ERROR] idle home and mounted TF required");return;
  }
  cancelServiceProbe();
  Serial.println("[SD-BACKUP-BEGIN] version=1");
  unsigned files=0,index=0,errors=0;
  for(const char* folder:{"/chat","/history","/photo","/video"}) {
    std::vector<String> names;
    if(!camScanMediaNames(folder,"",names)){++errors;continue;}
    for(const auto& path:names) {
      struct stat info={};const String native=String("/sdcard")+path;
      if(stat(native.c_str(),&info)!=0){++errors;continue;}
      if(!S_ISREG(info.st_mode))continue;
      JsonDocument metadata;metadata["path"]=path;metadata["bytes"]=info.st_size;metadata["mtime"]=info.st_mtime;
      Serial.print("[SD-INDEX] ");serializeJson(metadata,Serial);Serial.println();++index;
      if((String(folder)=="/photo" || String(folder)=="/video") && !path.endsWith(".meta"))continue;
      File file=SD_MMC.open(path,FILE_READ);if(!file){++errors;continue;}
      Serial.printf("[SD-FILE-BEGIN] %s %u\n",path.c_str(),unsigned(file.size()));
      uint8_t raw[768],encoded[1025];size_t copied=0;uint32_t crc=0;
      while(file.available()) {
        const size_t n=file.read(raw,sizeof(raw));if(!n)break;
        size_t length=0;if(mbedtls_base64_encode(encoded,sizeof(encoded),&length,raw,n))break;
        Serial.write(encoded,length);Serial.println();crc=crc32Update(crc,raw,n);copied+=n;yield();
      }
      const bool ok=copied==file.size();file.close();errors+=!ok;
      Serial.printf("[SD-FILE-END] bytes=%u crc32=%08lx ok=%u\n",unsigned(copied),(unsigned long)crc,unsigned(ok));++files;
    }
  }
  Serial.printf("[SD-BACKUP-END] files=%u index=%u errors=%u media_pixels_exported=0\n",files,index,errors);
}
