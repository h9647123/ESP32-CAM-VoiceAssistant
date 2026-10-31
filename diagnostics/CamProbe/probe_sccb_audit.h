#pragma once
// Passive link wrappers: forward each existing operation once, unchanged.
// No extra sensor reads, bank changes, GPIO writes, retries, or clock changes.
// Build with tools/build-cam-probe.ps1; its --wrap flags are mandatory for audit.
#include <stddef.h>
#include <driver/i2c_master.h>

static_assert(offsetof(i2c_master_bus_config_t, flags) == 28,
              "Re-review binary SCCB_Init layout for this IDF ABI");
static_assert(sizeof(i2c_master_bus_config_t) == 32,
              "Re-review binary SCCB_Init layout for this IDF ABI");

struct ProbeReadObservation {
  bool seen = false, txSeen = false, rxSeen = false;
  uint8_t value = 0;
  int bank = -1;
  esp_err_t txError = ESP_OK, rxError = ESP_OK;
};
struct ProbeSccbAudit {
  bool initSeen = false, busSeen = false, internalPullupRequested = false;
  int sda = -1, scl = -1, bank = -1;
  esp_err_t initError = ESP_OK, busError = ESP_OK;
  uint32_t writes = 0, writeErrors = 0, reads = 0, readErrors = 0;
  bool reading = false, txSeen = false, rxSeen = false;
  esp_err_t txError = ESP_OK, rxError = ESP_OK;
  gpio_io_config_t afterBus[2] = {};
  esp_err_t gpioErrors[2] = {ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_STATE};
  ProbeReadObservation pid, ver;
};
static ProbeSccbAudit probeSccbAudit;

extern "C" {
int __real_SCCB_Init(int sda, int scl);
int __real_SCCB_Write(uint8_t address, uint8_t reg, uint8_t value);
uint8_t __real_SCCB_Read(uint8_t address, uint8_t reg);
esp_err_t __real_i2c_new_master_bus(const i2c_master_bus_config_t*, i2c_master_bus_handle_t*);
esp_err_t __real_i2c_master_transmit(i2c_master_dev_handle_t, const uint8_t*, size_t, int);
esp_err_t __real_i2c_master_receive(i2c_master_dev_handle_t, uint8_t*, size_t, int);

esp_err_t __wrap_i2c_new_master_bus(const i2c_master_bus_config_t* config,
                                   i2c_master_bus_handle_t* handle) {
  const esp_err_t result = __real_i2c_new_master_bus(config, handle);
  probeSccbAudit.busSeen = true;
  probeSccbAudit.busError = result;
  if (config) probeSccbAudit.internalPullupRequested = config->flags.enable_internal_pullup;
  return result;
}
int __wrap_SCCB_Init(int sda, int scl) {
  const int result = __real_SCCB_Init(sda, scl);
  probeSccbAudit.initSeen = true;
  probeSccbAudit.initError = result;
  probeSccbAudit.sda = sda;
  probeSccbAudit.scl = scl;
  // These APIs only inspect GPIO state; no pull or direction setter is used.
  const int pins[] = {sda, scl};
  for (unsigned i = 0; i < 2; ++i) {
    if (GPIO_IS_VALID_GPIO(pins[i]))
      probeSccbAudit.gpioErrors[i] = gpio_get_io_config(gpio_num_t(pins[i]), &probeSccbAudit.afterBus[i]);
  }
  return result;
}
esp_err_t __wrap_i2c_master_transmit(i2c_master_dev_handle_t handle,
                                    const uint8_t* data, size_t size, int timeout) {
  const esp_err_t result = __real_i2c_master_transmit(handle, data, size, timeout);
  if (probeSccbAudit.reading) {
    probeSccbAudit.txSeen = true;
    probeSccbAudit.txError = result;
  }
  return result;
}
esp_err_t __wrap_i2c_master_receive(i2c_master_dev_handle_t handle,
                                   uint8_t* data, size_t size, int timeout) {
  const esp_err_t result = __real_i2c_master_receive(handle, data, size, timeout);
  if (probeSccbAudit.reading) {
    probeSccbAudit.rxSeen = true;
    probeSccbAudit.rxError = result;
  }
  return result;
}
int __wrap_SCCB_Write(uint8_t address, uint8_t reg, uint8_t value) {
  const int result = __real_SCCB_Write(address, reg, value);
  ++probeSccbAudit.writes;
  probeSccbAudit.writeErrors += result != 0;
  if (address == 0x30) {
    if (reg == 0xff) probeSccbAudit.bank = result == 0 ? (value & 1) : -1;
    // A soft reset invalidates our observed bank history. No attempt is made
    // to restore it with a write or to substitute the driver's bank cache.
    if (probeSccbAudit.bank == 1 && reg == 0x12 && (value & 0x80))
      probeSccbAudit.bank = -1;
  }
  return result;
}
uint8_t __wrap_SCCB_Read(uint8_t address, uint8_t reg) {
  probeSccbAudit.reading = true;
  probeSccbAudit.txSeen = probeSccbAudit.rxSeen = false;
  probeSccbAudit.txError = probeSccbAudit.rxError = ESP_OK;
  const uint8_t value = __real_SCCB_Read(address, reg);
  probeSccbAudit.reading = false;
  ++probeSccbAudit.reads;
  const bool failed = !probeSccbAudit.txSeen || !probeSccbAudit.rxSeen ||
      probeSccbAudit.txError != ESP_OK || probeSccbAudit.rxError != ESP_OK;
  probeSccbAudit.readErrors += failed;
  if (address == 0x30 && (reg == 0x0a || reg == 0x0b)) {
    ProbeReadObservation& sample = reg == 0x0a ? probeSccbAudit.pid : probeSccbAudit.ver;
    sample = {true, probeSccbAudit.txSeen, probeSccbAudit.rxSeen, value,
        probeSccbAudit.bank, probeSccbAudit.txError, probeSccbAudit.rxError};
  }
  return value;
}
}

static void probePrintGpio(const char* stage, int pin, esp_err_t result, const gpio_io_config_t& io) {
  if (result != ESP_OK) {
    Serial.printf("[SCCB-GPIO] stage=%s GPIO%d read_error=0x%x\n", stage, pin, unsigned(result));
    return;
  }
  Serial.printf("[SCCB-GPIO] stage=%s GPIO%d ie=%u oe=%u pu=%u pd=%u od=%u\n",
      stage, pin, unsigned(io.ie), unsigned(io.oe), unsigned(io.pu), unsigned(io.pd), unsigned(io.od));
}
static void probePrintId(const char* name, const ProbeReadObservation& sample) {
  const bool valid = sample.seen && sample.bank == 1 && sample.txSeen && sample.rxSeen &&
      sample.txError == ESP_OK && sample.rxError == ESP_OK;
  Serial.printf("[SCCB-ID] %s observed=%u value=0x%02x bank_from_write=%d tx_seen=%u rx_seen=%u tx_err=0x%x rx_err=0x%x transport_ok=%u fresh_read_added=0\n",
      name, unsigned(sample.seen), sample.value, sample.bank, unsigned(sample.txSeen),
      unsigned(sample.rxSeen), unsigned(sample.txError), unsigned(sample.rxError), unsigned(valid));
}
static void probeReportSccb(const char* stage, sensor_t* sensor) {
  Serial.printf("[SCCB-AUDIT] stage=%s init_observed=%u init_err=0x%x bus_observed=%u bus_err=0x%x internal_pullup_requested=%u writes=%lu write_errors=%lu reads=%lu read_errors=%lu\n",
      stage, unsigned(probeSccbAudit.initSeen), unsigned(probeSccbAudit.initError),
      unsigned(probeSccbAudit.busSeen), unsigned(probeSccbAudit.busError),
      unsigned(probeSccbAudit.internalPullupRequested), (unsigned long)probeSccbAudit.writes,
      (unsigned long)probeSccbAudit.writeErrors, (unsigned long)probeSccbAudit.reads,
      (unsigned long)probeSccbAudit.readErrors);
  const int pins[] = {CAM_PIN_SIOD, CAM_PIN_SIOC};
  for (unsigned i = 0; i < 2; ++i) {
    if (probeSccbAudit.initSeen)
      probePrintGpio("after_SCCB_Init", i == 0 ? probeSccbAudit.sda : probeSccbAudit.scl,
          probeSccbAudit.gpioErrors[i], probeSccbAudit.afterBus[i]);
    gpio_io_config_t current = {};
    const esp_err_t result = gpio_get_io_config(gpio_num_t(pins[i]), &current);
    probePrintGpio(stage, pins[i], result, current);
  }
  if (sensor) Serial.printf("[SCCB-ID-CACHED] PID=0x%04x VER=0x%02x cache_not_fresh_read=1\n", sensor->id.PID, sensor->id.VER);
  probePrintId("PID", probeSccbAudit.pid);
  probePrintId("VER", probeSccbAudit.ver);
  Serial.println("[SCCB-AUDIT] GPIO bits are not resistance/voltage/waveform acceptance; bank is observed write history, not physical readback");
}
