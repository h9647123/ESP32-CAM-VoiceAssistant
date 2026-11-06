#pragma once
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
// Large request documents must not compete with mbedTLS for internal RAM.
// Individual small allocation failures remain visible to JsonDocument::overflowed().
class CamJsonAllocator : public ArduinoJson::Allocator {
 public:
  void* allocate(size_t size) override {
    return heap_caps_malloc(size,psramFound()?(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT):MALLOC_CAP_8BIT);
  }
  void deallocate(void* ptr) override {heap_caps_free(ptr);}
  void* reallocate(void* ptr,size_t size) override {
    return heap_caps_realloc(ptr,size,psramFound()?(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT):MALLOC_CAP_8BIT);
  }
};
static CamJsonAllocator camJsonAllocator;
