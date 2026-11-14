#pragma once
// A retained reasoning archive (64KB) plus its final answer can exceed 65535.
// Configure BEFORE the first ArduinoJson include; the library encodes this in
// its version namespace. Storage/transport still impose their own byte limits.
#ifndef ARDUINOJSON_STRING_LENGTH_SIZE
#define ARDUINOJSON_STRING_LENGTH_SIZE 4
#endif
static_assert(ARDUINOJSON_STRING_LENGTH_SIZE>=4,"Long CAM history requires 32-bit JSON string lengths");
