#pragma once
#include <stdint.h>

#ifndef CUSTOM_IAT_MAX_RECORD_SECONDS
#define CUSTOM_IAT_MAX_RECORD_SECONDS 60
#endif
static_assert(CUSTOM_IAT_MAX_RECORD_SECONDS >= 1 && CUSTOM_IAT_MAX_RECORD_SECONDS <= 120,
    "CUSTOM IAT recording limit must be 1..120 seconds");

static constexpr uint32_t camAsrRecordLimitMs(bool customIat,uint32_t legacySeconds) {
  return (customIat ? uint32_t(CUSTOM_IAT_MAX_RECORD_SECONDS) : legacySeconds)*1000U;
}
// CUSTOM's server limit includes release-key tail audio. Reserve the tail from
// the interactive body rather than sending 60s + tail into a 60s gateway.
static constexpr uint32_t camAsrLiveBodyLimitMs(bool customIat,uint32_t legacySeconds,uint32_t tailMs) {
  return customIat ? (camAsrRecordLimitMs(true,legacySeconds)>tailMs ? camAsrRecordLimitMs(true,legacySeconds)-tailMs : 0U)
                   : camAsrRecordLimitMs(false,legacySeconds);
}
static constexpr uint32_t camMicCapacitySamples(uint32_t rate,uint32_t recordingMs,uint32_t tailMs) {
  // Allocate the entire recording plus tail and 2s scheduler headroom. Use
  // milliseconds so a fractional-second tail is never rounded down.
  return uint32_t((uint64_t(rate)*(recordingMs+tailMs+2000U)+999U)/1000U);
}
static constexpr uint32_t camAsrUploadLimitBytes(uint32_t rate,bool customIat,uint32_t legacySeconds,uint32_t tailMs) {
  return customIat ? camMicCapacitySamples(rate,camAsrRecordLimitMs(true,legacySeconds),tailMs)*2U : rate*2U*60U;
}
