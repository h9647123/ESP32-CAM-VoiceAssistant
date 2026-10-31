// Standalone, temporary camera diagnostic. No Wi-Fi, I2S, filesystem or NVS.
#include <Arduino.h>
#include <esp_camera.h>
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#include <driver/gpio.h>
#include <soc/gpio_struct.h>
#include <soc/gpio_sig_map.h>
#include <soc/lcd_cam_struct.h>
#include <mbedtls/base64.h>
#include "cam_board.h"
#include "probe_sccb_audit.h"
static bool probeExportJPEG = false;
static bool probeInvertPclk = false;
static bool probeCrcSeries = false;
static bool probeInternalRam = false;
static bool probeIdleRequested = false;
static bool probeIdleActive = false;
static bool probeIdleAttempted = false;
static uint32_t probeIdleWrites = 0, probeIdleReads = 0;
static uint32_t probeIdleWriteErrors = 0, probeIdleReadErrors = 0;

static void reportIdleCounters(const char* stage) {
  Serial.printf("[SCCB-IDLE] stage=%s writes=%lu reads=%lu write_errors=%lu read_errors=%lu delta_writes=%lu delta_reads=%lu delta_write_errors=%lu delta_read_errors=%lu\n",
      stage, (unsigned long)probeSccbAudit.writes, (unsigned long)probeSccbAudit.reads,
      (unsigned long)probeSccbAudit.writeErrors, (unsigned long)probeSccbAudit.readErrors,
      (unsigned long)(probeSccbAudit.writes - probeIdleWrites),
      (unsigned long)(probeSccbAudit.reads - probeIdleReads),
      (unsigned long)(probeSccbAudit.writeErrors - probeIdleWriteErrors),
      (unsigned long)(probeSccbAudit.readErrors - probeIdleReadErrors));
}

static void endIdleMeasurement() {
  probeReportSccb("measurement_end_before_deinit", esp_camera_sensor_get());
  reportIdleCounters("end_before_deinit");
  probeIdleActive = false;
  Serial.printf("[SCCB-IDLE] deinit=0x%x\n", unsigned(esp_camera_deinit()));
  Serial.println("[SCCB-IDLE] ENDED; no capture follows; restore normal application");
}

// CRC-32/ISO-HDLC: reflected polynomial, init/final XOR FFFFFFFF.
// Same algorithm as Python zlib.crc32; covers exactly frame->len bytes.
static uint32_t frameCrc(const uint8_t* data, size_t length) {
  uint32_t crc = 0xffffffffU;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320U : 0U);
  }
  return crc ^ 0xffffffffU;
}

static void exportFrame(camera_fb_t* frame) {
  Serial.printf("[CAM-EXPORT-BEGIN] bytes=%u width=%u height=%u\n",unsigned(frame->len),frame->width,frame->height);
  uint8_t encoded[1025];
  bool ok=true;
  for(size_t offset=0;ok&&offset<frame->len;) {
    const size_t count=min<size_t>(768,frame->len-offset);
    size_t size=0;
    ok=mbedtls_base64_encode(encoded,sizeof(encoded),&size,frame->buf+offset,count)==0;
    if(ok) ok=Serial.write(encoded,size)==size;
    Serial.println();offset+=count;yield();
  }
  Serial.println(ok?"[CAM-EXPORT-END]":"[CAM-EXPORT-ERROR] Incomplete transfer");
}

static void reportInputs() {
  const uint8_t pins[]={CAM_PIN_D0,CAM_PIN_D1,CAM_PIN_D2,CAM_PIN_D3,CAM_PIN_D4,CAM_PIN_D5,CAM_PIN_D6,CAM_PIN_D7};
  Serial.printf("[PROBE] XCLK GPIO%d output_signal=%u expected=%u source=%u div=%u+%u/%u\n",
      CAM_PIN_XCLK,unsigned(GPIO.func_out_sel_cfg[CAM_PIN_XCLK].func_sel),unsigned(CAM_CLK_IDX),
      unsigned(LCD_CAM.cam_ctrl.cam_clk_sel),unsigned(LCD_CAM.cam_ctrl.cam_clkm_div_num),
      unsigned(LCD_CAM.cam_ctrl.cam_clkm_div_b),unsigned(LCD_CAM.cam_ctrl.cam_clkm_div_a));
  uint32_t ones[8]={},edges[8]={},samples=0,mismatch13=0;
  uint32_t previous=GPIO.in;
  const uint32_t start=micros();
  while(uint32_t(micros()-start)<250000U) {
    const uint32_t current=GPIO.in;
    for(unsigned i=0;i<8;++i) {
      ones[i]+=(current>>pins[i])&1;
      edges[i]+=((current^previous)>>pins[i])&1;
    }
    mismatch13+=((current>>pins[1])^(current>>pins[3]))&1;
    previous=current;
    if((++samples&1023U)==0) yield();
  }
  Serial.printf("[PROBE] GPIO polling samples=%lu bit1_bit3_mismatch=%lu (not waveform timing)\n",
      (unsigned long)samples,(unsigned long)mismatch13);
  for(unsigned i=0;i<8;++i) {
    gpio_io_config_t io={}; gpio_get_io_config(gpio_num_t(pins[i]),&io);
    Serial.printf("[PROBE] d%u pin=%u matrix_pin=%u matrix_enabled=%u high=%lu transitions=%lu input=%d output=%d pu=%d pd=%d\n",
      i,unsigned(pins[i]),unsigned(GPIO.func_in_sel_cfg[CAM_DATA_IN0_IDX+i].func_sel),
      unsigned(GPIO.func_in_sel_cfg[CAM_DATA_IN0_IDX+i].sig_in_sel),(unsigned long)ones[i],(unsigned long)edges[i],io.ie,io.oe,io.pu,io.pd);
  }
  gpio_io_config_t saved={};
  const gpio_num_t suspect=gpio_num_t(CAM_PIN_D6);
  if(gpio_get_io_config(suspect,&saved)==ESP_OK) {
    gpio_set_pull_mode(suspect,GPIO_PULLUP_ONLY);
    uint32_t high=0,edges=0,count=0;int prior=gpio_get_level(suspect);
    const uint32_t began=micros();
    while(uint32_t(micros()-began)<100000U) {
      const int level=gpio_get_level(suspect);high+=level;edges+=level!=prior;prior=level;
      if((++count&1023U)==0) yield();
    }
    gpio_set_pull_mode(suspect,saved.pu?(saved.pd?GPIO_PULLUP_PULLDOWN:GPIO_PULLUP_ONLY):(saved.pd?GPIO_PULLDOWN_ONLY:GPIO_FLOATING));
    Serial.printf("[PROBE] D6 weak_pullup samples=%lu high=%lu transitions=%lu original_pulls_restored=1\n",
        (unsigned long)count,(unsigned long)high,(unsigned long)edges);
  }
}

static void testFormat(const char* name, pixformat_t format, uint32_t xclk, bool colorbar) {
  camera_config_t c = {};
  c.pin_d0=CAM_PIN_D0; c.pin_d1=CAM_PIN_D1; c.pin_d2=CAM_PIN_D2; c.pin_d3=CAM_PIN_D3;
  c.pin_d4=CAM_PIN_D4; c.pin_d5=CAM_PIN_D5; c.pin_d6=CAM_PIN_D6; c.pin_d7=CAM_PIN_D7;
  c.pin_xclk=CAM_PIN_XCLK; c.pin_pclk=CAM_PIN_PCLK;
  c.pin_vsync=CAM_PIN_VSYNC; c.pin_href=CAM_PIN_HREF;
  c.pin_sccb_sda=CAM_PIN_SIOD; c.pin_sccb_scl=CAM_PIN_SIOC;
  c.pin_reset=CAM_PIN_RESET; c.pin_pwdn=CAM_PIN_PWDN;
  c.ledc_channel=LEDC_CHANNEL_0; c.ledc_timer=LEDC_TIMER_0;
  c.xclk_freq_hz=xclk; c.pixel_format=format; c.frame_size=FRAMESIZE_QVGA;
  c.jpeg_quality=12; c.fb_count=1;
  c.fb_location=probeInternalRam ? CAMERA_FB_IN_DRAM : CAMERA_FB_IN_PSRAM;
  c.grab_mode=CAMERA_GRAB_WHEN_EMPTY;
  Serial.printf("[PROBE] BEGIN %s XCLK=%lu PSRAM_free=%u internal=%u\n",name,
      (unsigned long)xclk,unsigned(ESP.getFreePsram()),
      unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
  probeSccbAudit = {};
  const esp_err_t result=esp_camera_init(&c);
  Serial.printf("[PROBE] init=0x%x\n",unsigned(result));
  probeReportSccb("after_camera_init", result == ESP_OK ? esp_camera_sensor_get() : nullptr);
  if(result!=ESP_OK) {
    if(probeExportJPEG) Serial.println("[CAM-EXPORT-ERROR] Init failed");
    if(probeIdleRequested) Serial.println("[SCCB-IDLE] INIT_FAILED; no measurement window");
    return;
  }
  if(probeInvertPclk) {
    GPIO.func_in_sel_cfg[CAM_PCLK_IDX].sig_in_inv=1;
    Serial.println("[PROBE] PCLK input inversion enabled; physical pins unchanged");
  }
  sensor_t* s=esp_camera_sensor_get();
  Serial.printf("[PROBE] PID=0x%04x\n",s?s->id.PID:0);
  if (probeCrcSeries) {
    // These CRC commands never call reportInputs(), invert PCLK, or change pulls.
    // Explicitly set either state so the matched scene/bars pair differs only here.
    const int colorbarResult = s && s->set_colorbar ? s->set_colorbar(s, colorbar ? 1 : 0) : -1;
    Serial.printf("[PROBE] native_QVGA=1 fb_count=1 colorbar=%u memory=%s quality=12\n",
        unsigned(colorbar), probeInternalRam ? "DRAM" : "PSRAM");
    Serial.printf("[PROBE] colorbar_set_result=%d readback=unverified\n", colorbarResult);
    probeReportSccb("after_colorbar_before_warmup", s);
    if (colorbarResult != 0) {
      Serial.println("[CAM-EXPORT-ERROR] Colorbar setting failed; no valid experiment");
      Serial.printf("[PROBE] deinit=0x%x\n", unsigned(esp_camera_deinit()));
      return;
    }
    if (probeIdleRequested) {
      if (!probeSccbAudit.initSeen || !probeSccbAudit.busSeen ||
          probeSccbAudit.initError != ESP_OK || probeSccbAudit.busError != ESP_OK) {
        Serial.println("[SCCB-IDLE] AUDIT_OR_BUS_INIT_FAILED; no measurement window");
        Serial.printf("[SCCB-IDLE] deinit=0x%x\n", unsigned(esp_camera_deinit()));
        return;
      }
      if (probeSccbAudit.readErrors || probeSccbAudit.writeErrors)
        Serial.println("[SCCB-IDLE] WARNING: INIT_COMPLETED_WITH_TRANSACTION_ERRORS; do not treat init return as all transactions successful");
      probeIdleWrites = probeSccbAudit.writes;
      probeIdleReads = probeSccbAudit.reads;
      probeIdleWriteErrors = probeSccbAudit.writeErrors;
      probeIdleReadErrors = probeSccbAudit.readErrors;
      probeIdleActive = true;
      reportIdleCounters("start");
      Serial.println("[SCCB-IDLE] MEASURE_READY; infinite wait for sccb end; no new SCCB transactions, warmup, fb_get, GPIO/pull or XCLK changes");
      return; // Keep initialized bus/camera; never fall through to capture/deinit.
    }
    // The local driver has no dedicated verified colorbar readback API. A zero
    // setter result is an acknowledged write, not independent register readback.
    bool ready = true;
    unsigned warmed = 0;
    for (; warmed < 20; ++warmed) {
      camera_fb_t* warm = esp_camera_fb_get();
      if (!warm) { ready = false; break; }
      esp_camera_fb_return(warm);
      delay(10);
    }
    Serial.printf("[PROBE] warmup=%u/20\n", warmed);
    if (!ready) Serial.println("[CAM-EXPORT-ERROR] Warmup capture failed");
    for (unsigned index = 0; ready && index < 3; ++index) {
      camera_fb_t* frame = esp_camera_fb_get();
      if (!frame) { Serial.println("[CAM-EXPORT-ERROR] Capture failed"); break; }
      const size_t bytes = frame->len;
      if (!bytes || bytes > 1024U * 1024U || frame->format != PIXFORMAT_JPEG ||
          frame->width != 320 || frame->height != 240) {
        esp_camera_fb_return(frame);
        Serial.println("[CAM-EXPORT-ERROR] Invalid length/format/dimensions"); break;
      }
      const uint32_t sourceCrc = frameCrc(frame->buf, bytes);
      const bool fbExternal = esp_ptr_external_ram(frame->buf);
      uint8_t* owned = static_cast<uint8_t*>(heap_caps_malloc(bytes,
          MALLOC_CAP_8BIT | (probeInternalRam ? MALLOC_CAP_INTERNAL : MALLOC_CAP_SPIRAM)));
      if (!owned) {
        esp_camera_fb_return(frame);
        Serial.println("[CAM-EXPORT-ERROR] Owned buffer allocation failed"); break;
      }
      memcpy(owned, frame->buf, bytes);
      const uint32_t copyCrc = frameCrc(owned, bytes);
      camera_fb_t exported = *frame;
      exported.buf = owned;
      esp_camera_fb_return(frame);
      // Never read the returned driver framebuffer again.
      const uint32_t returnedCrc = frameCrc(owned, bytes);
      Serial.printf("[PROBE-CRC] frame=%u bytes=%u width=%u height=%u format=JPEG source=%08lx copy=%08lx returned=%08lx fb_external=%u copy_external=%u\n",
          index,unsigned(bytes),exported.width,exported.height,
          (unsigned long)sourceCrc,(unsigned long)copyCrc,(unsigned long)returnedCrc,
          unsigned(fbExternal),unsigned(esp_ptr_external_ram(owned)));
      exportFrame(&exported);
      heap_caps_free(owned);
    }
    Serial.printf("[PROBE] deinit=0x%x\n", unsigned(esp_camera_deinit()));
    return;
  }
  if(colorbar) Serial.printf("[PROBE] colorbar_result=%d\n",s?s->set_colorbar(s,1):-1);
  delay(300);
  if(colorbar) reportInputs();
  for(int attempt=0;attempt<2;++attempt) {
    const uint32_t start=millis();
    camera_fb_t* fb=esp_camera_fb_get();
    if(!fb) {
      Serial.printf("[PROBE] frame=%d NULL wait_ms=%lu\n",attempt,(unsigned long)(millis()-start));
      if(probeExportJPEG) Serial.println("[CAM-EXPORT-ERROR] Capture failed");
      break;
    }
    if(probeExportJPEG) { exportFrame(fb); esp_camera_fb_return(fb); break; }
    Serial.printf("[PROBE] frame=%d %ux%u format=%d bytes=%u wait_ms=%lu head=",attempt,
        fb->width,fb->height,int(fb->format),unsigned(fb->len),(unsigned long)(millis()-start));
    for(size_t i=0;i<min<size_t>(32,fb->len);++i) Serial.printf("%02x",fb->buf[i]);
    Serial.println();
    uint32_t ones[8]={}, mismatch13=0;
    for(size_t i=0;i<fb->len;++i) {
      for(unsigned b=0;b<8;++b) ones[b]+=(fb->buf[i]>>b)&1;
      mismatch13+=((fb->buf[i]>>1)^(fb->buf[i]>>3))&1;
    }
    Serial.print("[PROBE] bit_ones d0..d7:");
    for(unsigned b=0;b<8;++b) Serial.printf(" %lu",(unsigned long)ones[b]);
    Serial.println();
    Serial.printf("[PROBE] bit1_bit3_mismatching_bytes=%lu\n",(unsigned long)mismatch13);
    esp_camera_fb_return(fb);
    delay(100);
  }
  Serial.printf("[PROBE] deinit=0x%x\n",unsigned(esp_camera_deinit()));
  delay(250);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("[PROBE] READY: send probe; temporary camera-only firmware, no NVS writes");
  Serial.printf("[PROBE] CRC32_selftest=%08lx expected=cbf43926\n",
      (unsigned long)frameCrc(reinterpret_cast<const uint8_t*>("123456789"),9));
}
void loop() {
  if(Serial.available()) {
    String command=Serial.readStringUntil('\n'); command.trim();
    if (probeIdleActive) {
      if (command == "sccb end") endIdleMeasurement();
      else if (command == "sccb status") {
        probeReportSccb("measurement_status", esp_camera_sensor_get());
        reportIdleCounters("status");
      } else Serial.println("[SCCB-IDLE] Window remains active; only sccb status / sccb end allowed");
    } else if (command == "sccb idle") {
      if (probeIdleAttempted) {
        Serial.println("[SCCB-IDLE] Already attempted once; no automatic reinitialization");
      } else {
        probeIdleAttempted = true;
        probeIdleRequested = true;
        probeCrcSeries = true;
        probeExportJPEG = false;
        probeInvertPclk = false;
        probeInternalRam = false;
        testFormat("SCCB_IDLE_SCENE20", PIXFORMAT_JPEG, 20000000, false);
        probeIdleRequested = false;
        probeCrcSeries = false;
      }
    } else if(command=="crc-p20"||command=="crc-p10"||command=="crc-d20"||command=="crc-d10"||
        command=="crc-bars-p20"||command=="crc-scene-p20") {
      probeCrcSeries=true; probeExportJPEG=true; probeInvertPclk=false;
      probeInternalRam=command.startsWith("crc-d");
      testFormat(command.c_str(),PIXFORMAT_JPEG,command.endsWith("20")?20000000:10000000,
          command=="crc-bars-p20");
      probeCrcSeries=false; probeInternalRam=false; probeExportJPEG=false;
      Serial.println("[PROBE] DONE");
    } else if(command=="probe") {
      probeExportJPEG=false;
      testFormat("JPEG20",PIXFORMAT_JPEG,20000000,false);
      testFormat("JPEG10",PIXFORMAT_JPEG,10000000,false);
      testFormat("GRAY10",PIXFORMAT_GRAYSCALE,10000000,false);
      testFormat("RGB56510",PIXFORMAT_RGB565,10000000,false);
      Serial.println("[PROBE] DONE");
    } else if(command=="bar") {
      probeExportJPEG=false;
      testFormat("GRAYBAR10",PIXFORMAT_GRAYSCALE,10000000,true);
      testFormat("RGBBAR10",PIXFORMAT_RGB565,10000000,true);
      Serial.println("[PROBE] DONE");
    } else if(command=="photo20"||command=="photo10"||command=="bar20"||command=="bar10"||command=="edge20"||command=="edge10") {
      probeExportJPEG=true;
      probeInvertPclk=command.startsWith("edge");
      testFormat(command.c_str(),PIXFORMAT_JPEG,command.endsWith("20")?20000000:10000000,command.startsWith("bar")||probeInvertPclk);
      probeInvertPclk=false;
      Serial.println("[PROBE] DONE");
    }
  }
  delay(10);
}
