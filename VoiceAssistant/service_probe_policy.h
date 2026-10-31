#pragma once
#include <stdint.h>
static constexpr uint32_t SERVICE_CONNECT_TIMEOUT_MS=8000;
static constexpr uint32_t SERVICE_HEADER_TIMEOUT_MS=8000;
static constexpr uint32_t SERVICE_TLS_HANDSHAKE_SECONDS=12;
static constexpr uint32_t CUSTOM_IAT_HEADER_TIMEOUT_MS=8000;
static constexpr bool serviceNeedsFullProbe(bool requested,bool aiKnown,bool aiOk) {
  // A failed boot-time AI probe must be rechecked by the next periodic probe.
  return requested || (aiKnown && !aiOk);
}
