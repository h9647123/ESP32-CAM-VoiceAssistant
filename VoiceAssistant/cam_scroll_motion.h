#pragma once
#include <stdint.h>
// Integrate subpixel motion without rounding every fast loop up to one pixel.
// A bounded elapsed time avoids huge jumps after unrelated blocking work.
static int32_t camScrollDistance(uint32_t elapsed,uint16_t speed,uint32_t& remainder) {
  const uint32_t dt=elapsed>80?80:elapsed;
  const uint32_t units=dt*uint32_t(speed)+remainder;
  remainder=units%1000;
  return int32_t(units/1000);
}
static int32_t camScrollReverseTarget(int32_t current,int32_t target,int direction) {
  return (direction>0 && target<current) || (direction<0 && target>current)?current:target;
}
