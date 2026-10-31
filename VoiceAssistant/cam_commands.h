#pragma once
#include <mbedtls/base64.h>
#include "cam_sd_diagnostics.h"
// Included after the voice/LLM functions; this is an extension to the existing CLI.

static void camSaveSettings() {
  Preferences p;
  if (!p.begin("voice_cam", false)) return;
  p.putBool("vision", camVisionEnabled);
  p.putBool("photoonly", camPhotoOnly);
  p.putUChar("videosize", camVideoResolution);
  p.putBool("fullview", camFullPreview);
  p.putBool("allowmodel", camOtherModelAllowed);
  p.putString("detail", camVisionDetail);
  p.putUInt("framesize", unsigned(camFrameSize));
  p.putUInt("xclk", camXclkHz);
  p.end();
}

static void camLoadSettings() {
  Preferences p;
  if (!p.begin("voice_cam", true)) return;
  camVisionEnabled = p.getBool("vision", false);
  camPhotoOnly = p.getBool("photoonly", false);
  camVideoResolution = p.getUChar("videosize", 1);
  if(camVideoResolution>2) camVideoResolution=1;
  camFullPreview = p.getBool("fullview", true);
  camOtherModelAllowed = p.getBool("allowmodel", false);
  camVisionDetail = p.getString("detail", "low");
  if (camVisionDetail != "low" && camVisionDetail != "original") camVisionDetail = "low";
  const unsigned resolution = p.getUInt("framesize", unsigned(FRAMESIZE_VGA));
  const uint32_t xclk = p.getUInt("xclk", 20000000);
  camXclkHz = xclk == 10000000 ? 10000000 : 20000000;
  if (resolution == FRAMESIZE_QVGA || resolution == FRAMESIZE_VGA || resolution == FRAMESIZE_UXGA) {
    camFrameSize = framesize_t(resolution);
  }
  p.end();
}

static void camPrintStatus() {
  Serial.printf("[CAM-UI] stage=%u confirmed=%d frames=%u errors=%u generation=%u zoom=%d archived=%d\n", unsigned(camPhotoStage), camPhoto.confirmed, unsigned(camPreviewFrames), unsigned(camPreviewErrors), unsigned(camPhoto.generation), camPreviewZoom, camPhoto.imagePath.length() > 0);
  Serial.printf("[CAM] mode=%s camera=%d SD=%d photo=%u bytes %ux%u source=%s detail=%s\n",
      camVisionEnabled ? "vision" : "text", camCameraReady, camSdReady,
      unsigned(camPhoto.size), camPhoto.width, camPhoto.height,
      camPhoto.jpeg ? "PSRAM" : (camPhoto.ready() ? "TF" : "none"), camVisionDetail.c_str());
  if (camPhoto.imagePath.length()) Serial.println("[CAM] image_path=" + camPhoto.imagePath);
  Serial.println("[CAM] TFT 48/47/2/39 CS=-1 RST=-1; MIC 14/21/1; A/B/C 42/38/3; SD 46/40/41");
  camPrintMemory("status");
  Serial.printf("[CAM] configured_XCLK=%lu Hz\n", (unsigned long)camXclkHz);
  Serial.printf("[CAM] colorbar=%d init=%s\n", camColorbarEnabled, camNativeFrameInit ? "native-resolution" : "UXGA-allocation");
  Serial.printf("[CAM-PREVIEW] mode=%s preview=%s capture=UXGA\n",
      camFullPreview ? "full" : "scaled", camFullPreview ? "UXGA" : "VGA");
  if (camCameraReady) {
    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor) Serial.printf("[CAM-SENSOR-SIZE] actual=%s framesize=%d\n",
        sensor->status.framesize == FRAMESIZE_VGA ? "VGA" :
        sensor->status.framesize == FRAMESIZE_UXGA ? "UXGA" : "other", int(sensor->status.framesize));
    if (sensor) Serial.printf("[CAM-SENSOR] PID=0x%04x vflip=%u hmirror=%u autofocus_supported=%d AEC=%u AGC=%u\n",
        sensor->id.PID, sensor->status.vflip, sensor->status.hmirror,
        sensor->af_is_supported ? sensor->af_is_supported(sensor) : 0,
        sensor->status.aec, sensor->status.agc);
  }
}

static void camPrintHelp() {
  Serial.println("CAM draft2: snap [qvga|vga|uxga] | cam status | cam ask <text> | cam retry");
  Serial.println("  cam cancel | cam new | cam mode vision|text (saved) | cam detail low|original");
  Serial.println("  cam allowmodel on|off (mark current model image support; applies only to this endpoint/model) | sd status|mount|off");
  Serial.println("  cam signals (sample PCLK/VSYNC/HREF levels for 500ms; no pin reconfiguration)");
  Serial.println("  cam pincheck (GPIO17 weak-pull isolation check; disconnect camera with power OFF, then reboot)");
  Serial.println("  cam export (base64 JPEG to USB serial; retains pending photo)");
  Serial.println("  cam clock 10|20 (MHz; releases photo and reinitializes camera on next capture)");
  Serial.println("  cam miccheck (5s PCM level statistics only; no audio saved or uploaded)");
  Serial.println("  cam bars on|off | cam warmup | cam init native|uxga (camera diagnostics)");
  Serial.println("  voice ask 5 (ASR + LLM + TF) | sd history (read current session archive)\n  sd info | sd test (3 rounds: 64KiB+append, rename, remount, content+CRC verify; no format)");
  Serial.println("Hold A: voice (photo only in vision mode); release: draft; tap A: send; hold A/B: append.");
}

static void camCancelDraft() {
  if (camPhotoUiActive()) camClosePhotoUi();
  camReleasePhoto();
  composingQuestion = false;
  pendingQuestion = "";
  pendingSegments.clear();
  transcript = asrBestTranscript = asrStableTranscript = asrPartialTranscript = "";
  lastError = lastFailedQuestion = "";
  lastFailedUseSearch = false;
  markChatLinesDirty();
  chatAutoScrollBottom = true;
  setState(AppState::Idle, uiText("图片问题已取消", "Photo question cancelled", "Photo question cancelled"));
}

static void camExportPhoto() {
  if (!camPhoto.ready()) { Serial.println("[CAM-EXPORT-ERROR] No pending photo"); return; }
  File file;
  if (!camPhoto.jpeg) {
    if (camSdReady) file = SD_MMC.open(camPhoto.imagePath, FILE_READ);
    if (!file || file.size() != camPhoto.size) {
      file.close(); Serial.println("[CAM-EXPORT-ERROR] Photo source unavailable"); return;
    }
  }
  Serial.printf("[CAM-EXPORT-BEGIN] bytes=%u width=%u height=%u\n",
      unsigned(camPhoto.size), camPhoto.width, camPhoto.height);
  uint8_t raw[768], encoded[1025];
  bool ok = true;
  for (size_t offset = 0; ok && offset < camPhoto.size;) {
    const size_t count = min<size_t>(sizeof(raw), camPhoto.size - offset);
    const uint8_t* bytes = camPhoto.jpeg ? camPhoto.jpeg + offset : raw;
    if (!camPhoto.jpeg && file.read(raw, count) != count) { ok = false; break; }
    size_t encodedSize = 0;
    if (mbedtls_base64_encode(encoded, sizeof(encoded), &encodedSize, bytes, count) != 0 ||
        Serial.write(encoded, encodedSize) != encodedSize) { ok = false; break; }
    Serial.println();
    offset += count;
    yield();
  }
  file.close();
  Serial.println(ok ? "[CAM-EXPORT-END]" : "[CAM-EXPORT-ERROR] Incomplete transfer");
}

static void camCheckMicLevels() {
  Serial.println("[CAM-MIC] Begin 5s PCM level check; no audio saved/uploaded.");
  int16_t pcm[256];
  const uint32_t began = millis();
  uint32_t window = began, samples = 0, nonzero = 0, peak = 0;
  int64_t sum = 0;
  uint64_t squares = 0;
  while (millis() - began < 5000U) {
    const size_t bytes = readPcmChunk(pcm, 256, 40, nullptr);
    for (size_t i = 0; i < bytes / sizeof(int16_t); ++i) {
      const int32_t value = pcm[i];
      const uint32_t magnitude = value < 0 ? uint32_t(-value) : uint32_t(value);
      ++samples; nonzero += value != 0; peak = max(peak, magnitude);
      sum += value; squares += int64_t(value) * value;
    }
    if (millis() - window >= 1000U || millis() - began >= 5000U) {
      Serial.printf("[CAM-MIC] t_ms=%lu samples=%lu nonzero=%lu peak=%lu rms=%lu mean=%ld\n",
          (unsigned long)(millis() - began), (unsigned long)samples, (unsigned long)nonzero,
          (unsigned long)peak, samples ? (unsigned long)sqrt(double(squares) / samples) : 0UL,
          samples ? (long)(sum / samples) : 0L);
      samples = nonzero = peak = 0; sum = 0; squares = 0; window = millis();
    }
    yield();
  }
  Serial.println("[CAM-MIC] Done.");
}

static bool camHandleCommand(const String& line) {
  if (line != "snap" && !line.startsWith("snap ") && line != "cam" &&
      !line.startsWith("cam ") && line != "sd" && !line.startsWith("sd ")) return false;
  if (line == "cam" || line == "cam status" || line == "sd" || line == "sd status") {
    camPrintStatus();
    if (line == "sd" || line == "sd status") camPrintSdInfo();
    return true;
  }
  if (line == "cam help") { camPrintHelp(); return true; }
  if (recording || stoppingRecording || appState == AppState::Thinking) {
    Serial.println("[CAM] wait until recording/request finishes"); return true;
  }
  if (line == "cam signals") { camPrintSignals(); return true; }
  if (line == "cam pincheck") { camCheckD6Input(); return true; }
  if (line == "cam export") { camExportPhoto(); return true; }
  if (line == "cam miccheck") { camCheckMicLevels(); return true; }
  if (line == "cam bars on" || line == "cam bars off") {
    camCancelDraft(); camColorbarEnabled = line.endsWith("on"); camPrintStatus(); return true;
  }
  if (line == "cam warmup") {
    if (camVisionEnabled) camWarmup();
    else Serial.println("[CAM] camera disabled; use cam mode vision first");
    return true;
  }
  if (line == "cam init native" || line == "cam init uxga") {
    camCancelDraft();
    if (camCameraReady) {
      if (esp_camera_deinit() != ESP_OK) { Serial.println("[CAM] camera deinit failed"); return true; }
      camCameraReady = false;
    }
    camNativeFrameInit = line.endsWith("native"); camPrintStatus(); return true;
  }
  if (line == "cam clock 10" || line == "cam clock 20") {
    camCancelDraft();
    if (camCameraReady) {
      const esp_err_t result = esp_camera_deinit();
      if (result != ESP_OK) { Serial.printf("[CAM] camera deinit failed: 0x%x\n", unsigned(result)); return true; }
      camCameraReady = false;
    }
    camXclkHz = line.endsWith("10") ? 10000000 : 20000000;
    camSaveSettings(); camPrintStatus(); return true;
  }
  if (line == "sd mount") { camMountSd(); camPrintStatus(); return true; }
  if (line == "sd info") { camPrintSdInfo(); return true; }
  if (line == "sd history") {
    if (!camSdReady) { Serial.println("[TF-CHAT] card unavailable"); return true; }
    const String path = "/history/session" + String(cfg.currentSession) + ".jsonl";
    File file = SD_MMC.open(path, FILE_READ);
    if (!file) { Serial.println("[TF-CHAT] no archive for current session"); return true; }
    size_t records = 0, invalid = 0; String lastRole, lastStatus, lastContent;
    while (file.available()) {
      String line = file.readStringUntil('\n');
      if (!line.length()) continue;
      JsonDocument record;
      if (deserializeJson(record, line) || !record["role"].is<String>() || !record["content"].is<String>()) { ++invalid; continue; }
      ++records; lastRole = record["role"].as<String>();
      lastStatus = record["status"].as<String>(); lastContent = record["content"].as<String>();
    }
    Serial.printf("[TF-CHAT-READBACK] path=%s bytes=%u records=%u invalid=%u last_role=%s last_status=%s\n",
        path.c_str(), unsigned(file.size()), unsigned(records), unsigned(invalid), lastRole.c_str(), lastStatus.c_str());
    Serial.println("[TF-CHAT-LAST] " + lastContent);
    file.close(); return true;
  }
  if (line == "sd test") { camRunSdTest(); return true; }
  if (line == "sd off") { SD_MMC.end(); camSdReady = false; camPrintStatus(); return true; }
  if (line == "cam cancel" || line == "cam new") { camCancelDraft(); return true; }
  if (line == "cam mode vision" || line == "cam mode text") {
    camCancelDraft();
    camVisionEnabled = line.endsWith("vision");
    camSaveSettings(); camPrintStatus(); return true;
  }
  if (line == "cam allowmodel on" || line == "cam allowmodel off") {
    camOtherModelAllowed = line.endsWith(" on");
    if(!camSetModelVision(cfg.llmApiUrl,cfg.llmModelId,camOtherModelAllowed)) {Serial.println("Model capability update failed");return true;}
    saveRuntimeConfig();
    camSaveSettings(); camPrintStatus(); return true;
  }
  if (line == "cam detail low" || line == "cam detail original") {
    camVisionDetail = line.endsWith("original") ? "original" : "low";
    camSaveSettings(); camPrintStatus(); return true;
  }
  if (!camVisionEnabled && (line == "snap" || line.startsWith("snap ") ||
      line.startsWith("cam ask ") || line == "cam retry" || line == "cam warmup")) {
    Serial.println("[CAM] photo questions disabled; enable explicitly with: cam mode vision"); return true;
  }
  if (line == "snap" || line.startsWith("snap ")) {
    if (line != "snap") {
      const String resolution = line.substring(5);
      if (resolution == "qvga") camFrameSize = FRAMESIZE_QVGA;
      else if (resolution == "vga") camFrameSize = FRAMESIZE_VGA;
      else if (resolution == "uxga") camFrameSize = FRAMESIZE_UXGA;
      else { Serial.println("Usage: snap [qvga|vga|uxga]"); return true; }
      camSaveSettings();
    }
    lastFailedQuestion = "";
    setState(AppState::Recognizing, uiText("正在拍照...", "Capturing photo...", "Capturing photo..."));
    if (!camCapture(lastError)) setState(AppState::Error, lastError);
    else setState(AppState::Idle, uiText("照片已准备，请录音或提问", "Photo ready; record or ask", "Photo ready; record or ask"));
    return true;
  }
  if (line.startsWith("cam ask ")) {
    String question = line.substring(8);
    question.trim();
    if (!question.length()) { Serial.println("Usage: cam ask <text>"); return true; }
    // Boot Wi-Fi before allocating the camera DMA workspace.
    if (!connectWifi() || (!camPhoto.ready() && !camCapture(lastError))) {
      setState(AppState::Error, lastError); return true;
    }
    camAskAndStoreVision(question); return true;
  }
  if (line == "cam retry") {
    const String question = lastFailedQuestion.length() ? lastFailedQuestion : pendingQuestion;
    if (!camPhoto.ready() || !question.length()) {
      Serial.println("[CAM] no pending image/question to retry"); return true;
    }
    camAskAndStoreVision(question); return true;
  }
  camPrintHelp();
  return true;
}
