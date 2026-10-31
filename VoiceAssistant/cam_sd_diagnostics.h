#pragma once

// Included after cam_capture.h. File-level diagnostics only: never format or
// write raw sectors; only remove files created by this invocation after verify.
static void camPrintSdInfo() {
  if (!camSdReady) {
    Serial.println("[CAM-SD-INFO] mounted=0; use sd mount");
    return;
  }
  const sdcard_type_t type = SD_MMC.cardType();
  const char* name = type == CARD_SDHC ? "SDHC/SDXC" :
      (type == CARD_SD ? "SDSC" : (type == CARD_MMC ? "MMC" : "unknown"));
  Serial.printf("[CAM-SD-INFO] mounted=1 type=%s card_bytes=%llu volume_bytes=%llu used_bytes=%llu sector_bytes=%d bus_width=1 frequency_khz=%u\n",
      name, (unsigned long long)SD_MMC.cardSize(),
      (unsigned long long)SD_MMC.totalBytes(), (unsigned long long)SD_MMC.usedBytes(),
      SD_MMC.sectorSize(), unsigned(SDMMC_FREQ_DEFAULT));
}

static uint32_t camSdCrcUpdate(uint32_t crc, const uint8_t* bytes, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    crc ^= bytes[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ ((crc & 1U) ? 0xedb88320U : 0U);
  }
  return crc;
}

static uint8_t camSdTestByte(uint32_t position, uint32_t seed) {
  if (position >= 65536U) seed ^= 0xa5a55a5aU; // Distinct append payload.
  uint32_t value = position ^ seed;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  return uint8_t(value ^ (value >> 8) ^ (value >> 16) ^ (value >> 24));
}

static bool camSdTestFailure(const char* stage, const String& path) {
  Serial.printf("[CAM-SD-TEST] FAIL stage=%s retained_path=%s\n", stage, path.c_str());
  // All callers have closed their File before this point.
  camSdFailed();
  return false;
}

static bool camRunSdTest() {
  Serial.println("[CAM-SD-TEST] BEGIN rounds=3 payload_bytes=65536 append_bytes=512 no_format=1");
  if (!camMountSd()) {
    Serial.println("[CAM-SD-TEST] FAIL stage=mount");
    return false;
  }
  camPrintSdInfo();
  const uint64_t volume = SD_MMC.totalBytes(), used = SD_MMC.usedBytes();
  if (used > volume || volume - used < 131072ULL) {
    Serial.println("[CAM-SD-TEST] FAIL stage=insufficient_free_space");
    return false;
  }
  uint8_t block[512];
  const uint32_t total = 65536U + 512U;
  const uint32_t began = millis();
  for (unsigned round = 0; round < 3; ++round) {
    char name[80];
    String path, renamed;
    do {
      snprintf(name, sizeof(name), "/cam-test-%08lx-%u.tmp", (unsigned long)esp_random(), round);
      path = name;
      renamed = path + ".verified";
    } while (SD_MMC.exists(path) || SD_MMC.exists(renamed));
    const uint32_t seed = esp_random();
    uint32_t expectedCrc = 0xffffffffU;
    const uint32_t writeStarted = millis();
    File out = SD_MMC.open(path, FILE_WRITE);
    if (!out) return camSdTestFailure("open_write", path);
    bool ok = true;
    for (uint32_t offset = 0; ok && offset < 65536U; offset += sizeof(block)) {
      for (size_t i = 0; i < sizeof(block); ++i) block[i] = camSdTestByte(offset + i, seed);
      expectedCrc = camSdCrcUpdate(expectedCrc, block, sizeof(block));
      ok = out.write(block, sizeof(block)) == sizeof(block);
      yield();
    }
    out.flush();
    ok = ok && out.size() == 65536U;
    out.close();
    if (!ok) return camSdTestFailure("write_flush_size", path);
    out = SD_MMC.open(path, FILE_APPEND);
    if (!out) return camSdTestFailure("open_append", path);
    for (size_t i = 0; i < sizeof(block); ++i) block[i] = camSdTestByte(65536U + i, seed);
    expectedCrc = camSdCrcUpdate(expectedCrc, block, sizeof(block)) ^ 0xffffffffU;
    ok = out.write(block, sizeof(block)) == sizeof(block);
    out.flush();
    ok = ok && out.size() == total;
    out.close();
    if (!ok) return camSdTestFailure("append_flush_size", path);
    if (!SD_MMC.rename(path, renamed)) return camSdTestFailure("rename", path);
    const uint32_t writeMs = millis() - writeStarted;
    // Clear the mount/cache before readback, rather than immediately reading
    // through the same open handle. This is not a power-loss durability test.
    SD_MMC.end();
    camSdReady = false;
    if (!camMountSd()) return camSdTestFailure("remount", renamed);
    File in = SD_MMC.open(renamed, FILE_READ);
    if (!in) return camSdTestFailure("open_read", renamed);
    ok = !in.isDirectory() && in.size() == total;
    uint32_t actualCrc = 0xffffffffU, readBytes = 0;
    const uint32_t readStarted = millis();
    while (ok && readBytes < total) {
      const size_t count = min<size_t>(sizeof(block), total - readBytes);
      if (in.read(block, count) != count) { ok = false; break; }
      actualCrc = camSdCrcUpdate(actualCrc, block, count);
      for (size_t i = 0; i < count; ++i) {
        const uint8_t expected = camSdTestByte(readBytes + i, seed);
        if (block[i] != expected) {
          Serial.printf("[CAM-SD-TEST] mismatch offset=%lu expected=%02x actual=%02x\n",
              (unsigned long)(readBytes + i), unsigned(expected), unsigned(block[i]));
          ok = false; break;
        }
      }
      readBytes += count;
      yield();
    }
    actualCrc ^= 0xffffffffU;
    in.close();
    if (!ok || readBytes != total || actualCrc != expectedCrc)
      return camSdTestFailure("readback_content_crc", renamed);
    if (!SD_MMC.remove(renamed) || SD_MMC.exists(renamed))
      return camSdTestFailure("remove_owned_test_file", renamed);
    Serial.printf("[CAM-SD-TEST] round=%u bytes=%lu expected_crc=%08lx read_crc=%08lx write_ms=%lu read_ms=%lu remount=1 cleanup=1 PASS\n",
        round + 1, (unsigned long)total, (unsigned long)expectedCrc, (unsigned long)actualCrc,
        (unsigned long)writeMs, (unsigned long)(millis() - readStarted));
  }
  Serial.printf("[CAM-SD-TEST] PASS rounds=3 verified_bytes=%lu elapsed_ms=%lu\n",
      (unsigned long)(3U * total), (unsigned long)(millis() - began));
  return true;
}
