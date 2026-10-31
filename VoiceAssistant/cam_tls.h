#pragma once
#include <WiFiClientSecure.h>
#include <time.h>
#include "cam_board.h"

// Arduino ESP32 3.3.8 / ESP-IDF ships the Mozilla trust bundle in libmbedtls.
extern const uint8_t camCaBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t camCaBundleEnd[] asm("_binary_x509_crt_bundle_end");

static bool camEnsureTlsClock() {
#if CAM_TLS_DIAGNOSTIC_INSECURE
  return true;
#else
  if (time(nullptr) >= 1700000000) return true;
  configTime(8 * 3600, 0, "ntp.aliyun.com", "ntp1.aliyun.com", "pool.ntp.org");
  const uint32_t started = millis();
  while (time(nullptr) < 1700000000 && millis() - started < 12000) {
    delay(50);
    yield();
  }
  return time(nullptr) >= 1700000000;
#endif
}

static bool camConfigureTls(WiFiClientSecure& client) {
#if CAM_TLS_DIAGNOSTIC_INSECURE
  Serial.println("[CAM-TLS] DIAGNOSTIC ONLY: certificate verification disabled");
  client.setInsecure();
#else
  if (CAM_TLS_CA_PEM) client.setCACert(CAM_TLS_CA_PEM);
  else client.setCACertBundle(camCaBundleStart, camCaBundleEnd - camCaBundleStart);
#endif
  const bool clockReady = camEnsureTlsClock();
  if (!clockReady) Serial.println("[CAM-TLS] NTP failed; certificate verification remains required");
  return clockReady;
}
