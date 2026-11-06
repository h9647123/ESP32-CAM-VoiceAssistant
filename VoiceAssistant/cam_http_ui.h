#pragma once
#include <atomic>
#include <type_traits>

#if defined(ARDUINO_ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
static TaskHandle_t camUiOwner=nullptr;
static bool camHttpUiActive=false,camLlmNetworkWaiting=false;
static uint32_t camLlmWaitingSince=0,camBusyHeaderTicks=0;
static void camTickBusyUi();
struct CamHttpUiJob {
  void* operation=nullptr;int(*invoke)(void*)=nullptr;
  std::atomic<bool> done{false};int result=-1001;unsigned stackRemaining=0;
};
static void camHttpUiWorker(void* context) {
  auto* job=static_cast<CamHttpUiJob*>(context);
  job->result=job->invoke(job->operation);
  job->stackRemaining=uxTaskGetStackHighWaterMark(nullptr);
  job->done.store(true,std::memory_order_release);
  vTaskDelete(nullptr);
}
#endif

// HTTP/TLS ownership crosses tasks only at completion. Background health
// workers never call the GUI; only the registered UI task runs the heartbeat.
template<class Operation> static int camRunHttpWithUi(Operation&& operation,bool llmWaiting=false) {
#if defined(ARDUINO_ARCH_ESP32)
  if(!camUiOwner || xTaskGetCurrentTaskHandle()!=camUiOwner)return operation();
  using Callable=typename std::remove_reference<Operation>::type;
  CamHttpUiJob job;job.operation=&operation;
  job.invoke=[](void* context)->int{return (*static_cast<Callable*>(context))();};
  const uint32_t start=millis(),headerBefore=camBusyHeaderTicks;
  const bool previousWaiting=camLlmNetworkWaiting;const uint32_t previousSince=camLlmWaitingSince;
  camHttpUiActive=true;camLlmNetworkWaiting=llmWaiting;camLlmWaitingSince=start;
  const bool launched=xTaskCreatePinnedToCore(camHttpUiWorker,"va-http",8192,&job,1,nullptr,0)==pdPASS;
  if(launched)while(!job.done.load(std::memory_order_acquire)){camTickBusyUi();delay(5);yield();}
  if(launched)delay(5); // Core 0's idle task reclaims the finished worker stack.
  camHttpUiActive=false;camLlmNetworkWaiting=previousWaiting;camLlmWaitingSince=previousSince;
  Serial.printf("[HTTP-UI] elapsed_ms=%lu header_ticks=%lu task_stack_remaining=%u result=%d\n",
      (unsigned long)(millis()-start),(unsigned long)(camBusyHeaderTicks-headerBefore),job.stackRemaining,job.result);
  return launched?job.result:-1001;
#else
  (void)llmWaiting;return operation();
#endif
}
