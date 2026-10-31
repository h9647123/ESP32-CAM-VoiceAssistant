#pragma once
#include <Arduino.h>
// Keep the SDK override in a header so Arduino's prototype generator does not
// move sketch prototypes ahead of the project's enum and structure types.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);
