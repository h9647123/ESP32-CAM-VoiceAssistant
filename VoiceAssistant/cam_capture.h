#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <SD_MMC.h>
#include <esp_camera.h>
#include <esp_heap_caps.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <time.h>
#include "cam_board.h"
#include "cam_photo_order.h"

struct CamPhoto {
  uint8_t* jpeg = nullptr;
  size_t size = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  String imagePath;
  int64_t capturedAtEpoch = 0;
  bool confirmed = false;
  uint32_t generation = 0;
  bool ready() const { return size && (jpeg || imagePath.length()); }
  void clear() {
    if (jpeg) heap_caps_free(jpeg);
    jpeg = nullptr;
    size = 0;
    width = height = 0;
    imagePath = "";
    capturedAtEpoch = 0;
    confirmed = false;
    generation = 0;
  }
};

static CamPhoto camPhoto;
static constexpr uint8_t CAM_QUESTION_PHOTO_LIMIT=3;
static constexpr size_t CAM_QUESTION_PHOTO_RAM_LIMIT=2U*1024U*1024U;
static CamPhoto camEarlierPhotos[CAM_QUESTION_PHOTO_LIMIT-1];
static uint8_t camEarlierPhotoCount=0;
static bool camVoiceOnlyQuestion=false;
static bool camQuestionReadyForVoice=false;
static bool camVisionEnabled = false;
static bool camFullPreview = true;
static uint32_t camPhotoGeneration = 0;
static bool camQuestionRequiresPhoto = false;
static bool camCameraReady = false;
static bool camSdReady = false;
static uint64_t camSdTotalBytes = 0;
static uint64_t camSdUsedBytes = 0;
static uint32_t camSdUsageCheckedAt = 0;
static bool camSdUsageChecked = false;
static bool camSdUsageValid = false;

static void camRefreshSdUsage(bool force = false) {
  if (!camSdReady) {
    camSdUsageChecked = camSdUsageValid = false;
    camSdTotalBytes = camSdUsedBytes = 0;
    return;
  }
  if (!force && camSdUsageChecked && millis() - camSdUsageCheckedAt < 10000U) return;
  camSdUsageCheckedAt = millis();
  camSdUsageChecked = true;
  const uint64_t total = SD_MMC.totalBytes(), used = SD_MMC.usedBytes();
  camSdUsageValid = total > 0 && used <= total;
  camSdTotalBytes = total;
  camSdUsedBytes = used;
}

static String camSdCapacityText() {
  if (!camSdReady || !camSdUsageValid) return "--";
  return String((unsigned long)(camSdUsedBytes/1000000ULL))+"/"+
      String((unsigned long)(camSdTotalBytes/1000000ULL))+"MB";
}
static uint32_t camBootTag = 0;
static uint64_t camPhotoSequence = 0;
static bool camPhotoSequenceReady = false;
static framesize_t camFrameSize = FRAMESIZE_VGA;
static uint32_t camXclkHz = 20000000;
static bool camColorbarEnabled = false; // Diagnostic only; reset restores real imagery.
static bool camNativeFrameInit = false;
static framesize_t camAllocatedFrameSize = FRAMESIZE_UXGA;

static size_t camQuestionPhotoCount() {
  return camEarlierPhotoCount+(camPhoto.ready() && camPhoto.confirmed ? 1:0);
}
static CamPhoto& camQuestionPhotoAt(size_t index) {
  return index<camEarlierPhotoCount?camEarlierPhotos[index]:camPhoto;
}
static void camMovePhoto(CamPhoto& target,CamPhoto& source) {
  target.clear();target=std::move(source);source.jpeg=nullptr;source.clear();
}
static bool camPhotoPathPinned(const String& path) {
  if(camPhoto.imagePath==path && camPhoto.ready())return true;
  for(uint8_t i=0;i<camEarlierPhotoCount;++i)if(camEarlierPhotos[i].imagePath==path)return true;
  return false;
}
static void camRestoreLastPhoto() {
  if(!camPhoto.confirmed && camEarlierPhotoCount)camMovePhoto(camPhoto,camEarlierPhotos[--camEarlierPhotoCount]);
  camQuestionRequiresPhoto=camQuestionPhotoCount()>0;
}
static bool camStashCurrentPhoto() {
  if(camQuestionPhotoCount()>=CAM_QUESTION_PHOTO_LIMIT)return false;
  if(camPhoto.ready() && camPhoto.confirmed)camMovePhoto(camEarlierPhotos[camEarlierPhotoCount++],camPhoto);
  else camPhoto.clear();
  return true;
}
static void camRemoveQuestionPhoto(size_t index) {
  if(index>=camQuestionPhotoCount())return;
  if(index==camEarlierPhotoCount) {camPhoto.clear();camRestoreLastPhoto();}
  else {
    camEarlierPhotos[index].clear();
    for(size_t i=index;i+1<camEarlierPhotoCount;++i)camMovePhoto(camEarlierPhotos[i],camEarlierPhotos[i+1]);
    --camEarlierPhotoCount;
  }
  camQuestionRequiresPhoto=camQuestionPhotoCount()>0;
  if(camPhoto.ready())camPhoto.generation=++camPhotoGeneration;
  if(!camQuestionPhotoCount()) {camQuestionReadyForVoice=false;camVoiceOnlyQuestion=false;}
}
static bool camBoundQuestionPhotoRam() {
  size_t bytes=camPhoto.jpeg?camPhoto.size:0;
  for(uint8_t i=0;i<camEarlierPhotoCount;++i)if(camEarlierPhotos[i].jpeg)bytes+=camEarlierPhotos[i].size;
  for(size_t i=0;bytes>CAM_QUESTION_PHOTO_RAM_LIMIT && i<=camEarlierPhotoCount;++i) {
    auto& photo=i<camEarlierPhotoCount?camEarlierPhotos[i]:camPhoto;
    if(!photo.jpeg || !camSdReady || !photo.imagePath.length())continue;
    File check=SD_MMC.open(photo.imagePath,FILE_READ);
    const bool good=check && check.size()==photo.size;check.close();
    if(good) {heap_caps_free(photo.jpeg);photo.jpeg=nullptr;bytes-=photo.size;}
  }
  return bytes<=CAM_QUESTION_PHOTO_RAM_LIMIT;
}

static void camReleasePhoto() {
  camPhoto.clear();
  for(auto& photo:camEarlierPhotos)photo.clear();camEarlierPhotoCount=0;
  camVoiceOnlyQuestion=false;
  camQuestionReadyForVoice=false;
  camQuestionRequiresPhoto = false;
}

static void camPrintMemory(const char* tag) {
  Serial.printf("[CAM-MEM] %s internal=%u largest=%u DMA=%u PSRAM=%u\n", tag,
      unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
      unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
      unsigned(heap_caps_get_free_size(MALLOC_CAP_DMA)),
      unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
}

static bool camMountSd() {
  if (camSdReady) return true;
  SD_MMC.end();
  camPhotoSequenceReady = false;
  // CLK=46 has a hardware pulldown. Initialize only after the ROM boot phase.
  Serial.printf("[CAM-SD] mount 1-bit CLK=%d CMD=%d D0=%d frequency_khz=%u format_on_failure=0\n",
      CAM_SD_CLK, CAM_SD_CMD, CAM_SD_D0, unsigned(SDMMC_FREQ_DEFAULT));
  if (!SD_MMC.setPins(CAM_SD_CLK, CAM_SD_CMD, CAM_SD_D0) ||
      !SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 5)) {
    SD_MMC.end();
    Serial.println("[CAM-SD] unavailable; PSRAM requests remain enabled");
    return false;
  }
  camSdReady = SD_MMC.cardType() != CARD_NONE &&
      (SD_MMC.exists("/photo") || SD_MMC.mkdir("/photo")) &&
      (SD_MMC.exists("/history") || SD_MMC.mkdir("/history"));
  if (!camSdReady) SD_MMC.end();
  camRefreshSdUsage(true);
  Serial.printf("[CAM-SD] ready=%d 1-bit CLK=46 CMD=40 D0=41 card=%llu MB\n",
      camSdReady, camSdReady ? SD_MMC.cardSize() / (1024ULL * 1024ULL) : 0ULL);
  return camSdReady;
}

static void camSdFailed() {
  camSdReady = false;
  // The caller closes its File before invalidating the mount.
  SD_MMC.end();
  Serial.println("[CAM-SD] I/O failed; use 'sd mount' after reinserting the card");
}

static String camNextPhotoPath(int64_t capturedAt) {
  if (!camPhotoSequenceReady) {
    File directory = SD_MMC.open("/photo");
    for (File f = directory.openNextFile(); f; f = directory.openNextFile()) {
      if (!f.isDirectory()) {
        const auto key = camParsePhotoKey(f.name());
        if (key.order > camPhotoSequence) camPhotoSequence = key.order;
      }
      f.close();
    }
    directory.close();
    camPhotoSequenceReady = true;
  }
  if (!camBootTag) camBootTag = esp_random() | 1U;
  char name[64];
  do {
    snprintf(name, sizeof(name), "/photo/P%016llx_T%010llu.jpg",
        (unsigned long long)++camPhotoSequence,
        (unsigned long long)(camValidPhotoTime(capturedAt) ? capturedAt : 0));
  } while (SD_MMC.exists(name) || SD_MMC.exists(String(name) + ".tmp"));
  return String(name);
}

static bool camArchiveJpeg(const uint8_t* bytes, size_t size, String& path, int64_t capturedAt = 0) {
  path = "";
  if (!camSdReady) return false;
  const String finalPath = camNextPhotoPath(capturedAt);
  const String temporary = finalPath + ".tmp";
  File out = SD_MMC.open(temporary, FILE_WRITE);
  bool ok = bool(out);
  for (size_t offset = 0; ok && offset < size;) {
    const size_t chunk = min<size_t>(4096, size - offset);
    ok = out.write(bytes + offset, chunk) == chunk;
    offset += chunk;
    yield();
  }
  if (out) {
    out.flush();
    ok = ok && out.size() == size;
    out.close();
  }
  if (ok) ok = SD_MMC.rename(temporary, finalPath);
  if (!ok) {
    SD_MMC.remove(temporary);
    camSdFailed();
    return false;
  }
  path = finalPath;
  // Archive limits bound the browser list only; saved photos need explicit deletion.
  Serial.printf("[CAM-SD] JPEG archived bytes=%u path=%s\n", unsigned(size), path.c_str());
  return true;
}

static bool camInitCamera(String& error) {
  if (camCameraReady && camNativeFrameInit && camAllocatedFrameSize != camFrameSize) {
    if (esp_camera_deinit() != ESP_OK) { error = "Camera resize deinit failed"; return false; }
    camCameraReady = false;
  }
  if (camCameraReady) return true;
  if (!psramFound()) { error = "CAM requires OPI PSRAM"; return false; }
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0; // TFT PWM uses channel 7, a different timer.
  c.pin_d0 = CAM_PIN_D0; c.pin_d1 = CAM_PIN_D1;
  c.pin_d2 = CAM_PIN_D2; c.pin_d3 = CAM_PIN_D3;
  c.pin_d4 = CAM_PIN_D4; c.pin_d5 = CAM_PIN_D5;
  c.pin_d6 = CAM_PIN_D6; c.pin_d7 = CAM_PIN_D7;
  c.pin_xclk = CAM_PIN_XCLK; c.pin_pclk = CAM_PIN_PCLK;
  c.pin_vsync = CAM_PIN_VSYNC; c.pin_href = CAM_PIN_HREF;
  c.pin_sccb_sda = CAM_PIN_SIOD; c.pin_sccb_scl = CAM_PIN_SIOC;
  c.pin_pwdn = CAM_PIN_PWDN; c.pin_reset = CAM_PIN_RESET;
  c.xclk_freq_hz = camXclkHz;
  c.pixel_format = PIXFORMAT_JPEG;
  // Allocate a framebuffer large enough for the supported resolutions once.
  c.frame_size = camNativeFrameInit ? camFrameSize : FRAMESIZE_UXGA;
  c.jpeg_quality = 12;
  c.fb_count = 1;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  esp_err_t result = esp_camera_init(&c);
  if (result != ESP_OK) {
    error = "Camera init 0x" + String(uint32_t(result), HEX);
    return false;
  }
  sensor_t* sensor = esp_camera_sensor_get();
  if (!sensor || sensor->id.PID != OV2640_PID || sensor->set_framesize(sensor, camFrameSize) != 0) {
    esp_camera_deinit();
    error = "OV2640 sensor/resolution unavailable";
    return false;
  }
  // Correct the module's vertical and horizontal orientation at the sensor, so preview,
  // archived JPEG and uploaded JPEG all use the same pixels.
  if (sensor->set_vflip(sensor, 1) != 0 || sensor->set_hmirror(sensor, 1) != 0) {
    esp_camera_deinit();
    error = "Camera orientation failed";
    return false;
  }
  camCameraReady = true;
  camAllocatedFrameSize = c.frame_size;
  camPrintMemory("camera initialized");
  return true;
}

static void camPrintSignals() {
  String error;
  if (!camInitCamera(error)) {
    Serial.println("[CAM-SIGNAL] " + error);
    return;
  }
  const gpio_num_t pins[] = {gpio_num_t(CAM_PIN_PCLK), gpio_num_t(CAM_PIN_VSYNC), gpio_num_t(CAM_PIN_HREF)};
  const char* names[] = {"PCLK", "VSYNC", "HREF"};
  uint32_t transitions[3] = {}, highs[3] = {}, samples = 0;
  int previous[3];
  for (int i = 0; i < 3; ++i) previous[i] = gpio_get_level(pins[i]);
  const uint32_t started = micros();
  while (uint32_t(micros() - started) < 500000U) {
    for (int i = 0; i < 3; ++i) {
      const int level = gpio_get_level(pins[i]);
      highs[i] += level;
      transitions[i] += level != previous[i];
      previous[i] = level;
    }
    if ((++samples & 1023U) == 0) yield();
  }
  sensor_t* sensor = esp_camera_sensor_get();
  Serial.printf("[CAM-SIGNAL] PID=0x%04x LEDC_timer0=%lu Hz (not S3 XCLK) window_us=%lu samples=%lu\n",
      sensor ? sensor->id.PID : 0, (unsigned long)ledc_get_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0),
      (unsigned long)(micros() - started), (unsigned long)samples);
  for (int i = 0; i < 3; ++i) {
    Serial.printf("[CAM-SIGNAL] %s GPIO%d transitions=%lu high_samples=%lu last=%d\n",
        names[i], int(pins[i]), (unsigned long)transitions[i], (unsigned long)highs[i], previous[i]);
  }
  Serial.println("[CAM-SIGNAL] Software polling only: transitions are not a frequency or waveform measurement.");
}

static void camCheckD6Input() {
  if (camCameraReady) {
    Serial.println("[CAM-PIN] Reboot first; pincheck is unavailable while the camera driver is active.");
    return;
  }
  const gpio_num_t pin = gpio_num_t(CAM_PIN_D6);
  gpio_io_config_t saved = {};
  if (gpio_get_io_config(pin, &saved) != ESP_OK || saved.oe) {
    Serial.println("[CAM-PIN] Cannot check: GPIO unavailable or configured as an output.");
    return;
  }
  Serial.println("[CAM-PIN] Isolation test: disconnect camera with all power OFF before this command.");
  gpio_input_enable(pin);
  const gpio_pull_mode_t modes[] = {GPIO_PULLUP_ONLY, GPIO_PULLDOWN_ONLY};
  const char* names[] = {"weak_pullup", "weak_pulldown"};
  for (unsigned i = 0; i < 2; ++i) {
    const esp_err_t result = gpio_set_pull_mode(pin, modes[i]);
    delay(5);
    uint32_t high = 0, count = 0;
    const uint32_t began = micros();
    while (uint32_t(micros() - began) < 100000U) {
      high += gpio_get_level(pin);
      if ((++count & 1023U) == 0) yield();
    }
    Serial.printf("[CAM-PIN] GPIO%d %s result=0x%x high=%lu samples=%lu\n", int(pin), names[i],
        unsigned(result), (unsigned long)high, (unsigned long)count);
  }
  gpio_set_pull_mode(pin, saved.pu ? (saved.pd ? GPIO_PULLUP_PULLDOWN : GPIO_PULLUP_ONLY) :
      (saved.pd ? GPIO_PULLDOWN_ONLY : GPIO_FLOATING));
  if (!saved.ie) gpio_set_direction(pin, GPIO_MODE_DISABLE); // saved.oe was checked false above
  Serial.println("[CAM-PIN] Original input-enable and pull settings restored; no output drive used.");
}

static void camWarmup() {
  String error;
  if (!camInitCamera(error)) { Serial.println("[CAM-WARMUP] " + error); return; }
  unsigned frames = 0;
  const uint32_t started = millis();
  for (; frames < 30; ++frames) {
    camera_fb_t* frame = esp_camera_fb_get();
    if (!frame) break;
    esp_camera_fb_return(frame);
    delay(10);
  }
  Serial.printf("[CAM-WARMUP] frames=%u/30 elapsed_ms=%lu\n", frames, (unsigned long)(millis() - started));
}

static bool camCapture(String& error, bool archive = true) {
  error = "";
  camQuestionRequiresPhoto = true; // Even a failed retake must not become text-only.
  camPhoto.clear();
  const bool needsInit = !camCameraReady ||
      (camNativeFrameInit && camAllocatedFrameSize != camFrameSize);
  if (!camInitCamera(error)) return false;
  sensor_t* sensor = esp_camera_sensor_get();
  const bool changed = sensor && sensor->status.framesize != camFrameSize;
  if (!sensor || (changed && sensor->set_framesize(sensor, camFrameSize) != 0)) {
    error = "Camera resolution failed"; return false;
  }
  if (sensor->set_colorbar(sensor, camColorbarEnabled ? 1 : 0) != 0) {
    error = "Camera test-pattern setting failed"; return false;
  }
  // Preview and shutter share the same readout. Drain only the queued frame;
  // resolution-comparison diagnostics can use the existing cam warmup command.
  // VGA preview -> UXGA shutter needs several fresh exposure frames. The full
  // preview mode only discards the one queued frame and never changes readout.
  for (int i = 0; i < (changed ? 4 : 1); ++i) {
    camera_fb_t* stale = esp_camera_fb_get();
    if (stale) esp_camera_fb_return(stale);
  }
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { error = "Camera capture failed"; return false; }
  const size_t bytes = fb->len;
  const bool valid = fb->format == PIXFORMAT_JPEG && bytes >= 4 &&
      bytes <= CAM_JPEG_MAX_BYTES && fb->buf[0] == 0xff && fb->buf[1] == 0xd8 &&
      fb->buf[bytes - 2] == 0xff && fb->buf[bytes - 1] == 0xd9;
  if (!valid) {
    esp_camera_fb_return(fb);
    error = "Invalid/oversized JPEG"; return false;
  }
  const int64_t capturedAt = time(nullptr);
  camPhoto.capturedAtEpoch = camValidPhotoTime(capturedAt) ? capturedAt : 0;
  camPhoto.width = fb->width;
  camPhoto.height = fb->height;
  camPhoto.jpeg = static_cast<uint8_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (camPhoto.jpeg) {
    memcpy(camPhoto.jpeg, fb->buf, bytes);
    esp_camera_fb_return(fb); // Never retain fb across ASR or HTTPS.
    camPhoto.size = bytes;
    if (archive) camArchiveJpeg(camPhoto.jpeg, bytes, camPhoto.imagePath, camPhoto.capturedAtEpoch);
  } else {
    // Last-resort file source: archive while fb is owned, return before ASR.
    const bool archived = archive && camArchiveJpeg(fb->buf, bytes, camPhoto.imagePath, camPhoto.capturedAtEpoch);
    esp_camera_fb_return(fb);
    if (!archived) {
      camPhoto.clear();
      error = "JPEG allocation failed; TF fallback unavailable";
      return false;
    }
    camPhoto.size = bytes;
  }
  camPhoto.confirmed = archive;
  camPhoto.generation = ++camPhotoGeneration;
  Serial.printf("[CAM] captured %ux%u JPEG=%u source=%s archive=%s resized=%d init=%d\n",
      camPhoto.width, camPhoto.height, unsigned(bytes), camPhoto.jpeg ? "PSRAM" : "TF",
      camPhoto.imagePath.length() ? camPhoto.imagePath.c_str() : "none", changed, needsInit);
  camPrintMemory("capture complete / framebuffer returned");
  return true;
}

static bool camAppendHistory(uint8_t session, const String& role, const String& content,
    const String& provider, const String& model, const char* status, const String& imagePath) {
  if (!camSdReady) return false;
  const String path = "/history/session" + String(session) + ".jsonl";
  File probe = SD_MMC.open(path, FILE_READ);
  const bool rotate = probe && probe.size() >= CAM_HISTORY_FILE_LIMIT;
  bool brokenTail = false;
  if (probe && probe.size()) {
    probe.seek(probe.size() - 1);
    brokenTail = probe.read() != '\n';
  }
  probe.close();
  if (rotate) {
    SD_MMC.remove(path + ".3");
    for (int i = 2; i >= 1; --i) {
      if (SD_MMC.exists(path + "." + String(i)) &&
          !SD_MMC.rename(path + "." + String(i), path + "." + String(i + 1))) {
        camSdFailed(); return false;
      }
    }
    if (!SD_MMC.rename(path, path + ".1")) { camSdFailed(); return false; }
    brokenTail = false;
  }
  JsonDocument record;
  record["timestamp"] = int64_t(time(nullptr));
  record["boot_id"] = camBootTag;
  record["millis"] = millis();
  record["role"] = role;
  record["content"] = content;
  record["image_path"] = imagePath;
  if(camQuestionPhotoCount()) {
    JsonArray paths=record["image_paths"].to<JsonArray>();
    for(size_t i=0;i<camQuestionPhotoCount();++i)paths.add(camQuestionPhotoAt(i).imagePath);
    record["image_count"]=camQuestionPhotoCount();
  }
  record["provider"] = provider;
  record["model"] = model;
  record["status"] = status;
  String line;
  serializeJson(record, line);
  if (record.overflowed() || line.length() > 65536) return false;
  File out = SD_MMC.open(path, FILE_APPEND);
  bool ok = bool(out);
  if (ok && brokenTail) ok = out.write('\n') == 1; // isolate any power-loss fragment
  if (ok) ok = out.write(reinterpret_cast<const uint8_t*>(line.c_str()), line.length()) == line.length();
  if (ok) ok = out.write('\n') == 1;
  if (out) { out.flush(); out.close(); }
  if (!ok) camSdFailed();
  return ok;
}
