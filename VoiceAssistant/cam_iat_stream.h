#pragma once
#include <stdint.h>
#include <stddef.h>

// Consume only the release/acquire-published immutable microphone prefix.
// Normal calls send full 200ms CUSTOM batches (40ms for official IAT). A flush
// sends the exact remaining tail, without rounding or manufacturing samples.
static inline size_t camIatPublishedBatch(uint32_t published,uint32_t sent,bool custom,bool flush) {
  if(sent>=published)return 0;
  const uint32_t batch=custom?3200U:640U;
  const uint32_t available=published-sent;
  return available>=batch?batch:(flush?available:0U);
}
