#pragma once
#include <Arduino.h>
#include <atomic>
#include <algorithm>
#include <driver/i2s.h>
#include <esp_heap_caps.h>
#include <freertos/semphr.h>

// One producer owns I2S and PCM until completion. No UI, sockets or logging on
// this task. The consumer may read the immutable recording only after wait().
class BufferedMic {
 public:
  struct Stats {
    uint32_t startedMs=0, endedMs=0, samples=0, clips=0, rawPeak=0, peak=0;
    uint32_t i2sOverflow=0, bufferOverflow=0, readErrors=0;
    uint64_t sumSquares=0;
  } stats;
  std::atomic<uint32_t> samples{0}, liveRms{0};
  std::atomic<bool> done{true};
  int16_t* data=nullptr;
  size_t capacity=0;
  bool active() const { return !done.load(std::memory_order_acquire); }
  bool resumeDriver() {
    return !active();
  }
  bool begin(i2s_port_t port, QueueHandle_t events, uint32_t maxSamples, unsigned shift, uint32_t testInitialDelayMs=0) {
    if(active() || !maxSamples) return false;
    if(pending_ && !wait(500)) return false;
    if(capacity<maxSamples) {
      auto* replacement=static_cast<int16_t*>(heap_caps_malloc(size_t(maxSamples)*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
      if(!replacement) return false;
      free(data);data=replacement;capacity=maxSamples;
    }
    if(!finished_) finished_=xSemaphoreCreateBinary();
    if(!finished_) return false;
    while(xSemaphoreTake(finished_,0)==pdTRUE) {}
    port_=port;events_=events;shift_=shift;limit_=maxSamples;testInitialDelayMs_=testInitialDelayMs;
    // Keep BCLK/WS running: stopping the clocks restarts INMP441 settling and
    // produces a large decaying transient. Discard stale data without gating
    // the clocks. Consumption is much faster than the 16ms DMA arrival rate.
    int32_t discard[256];size_t n=0;
    for(unsigned i=0;i<32;++i) {
      n=0;
      if(i2s_read(port_,discard,sizeof(discard),&n,0)!=ESP_OK) return false;
      if(!n) break;
      if(i==31) return false;
    }
    if(events_) xQueueReset(events_);
    stats=Stats();samples=0;liveRms=0;target_=UINT32_MAX;cancel_=false;
    done.store(false,std::memory_order_release);
    pending_=true;
    if(xTaskCreatePinnedToCore(run,"mic-capture",6144,this,4,nullptr,0)!=pdPASS) {
      done=true;pending_=false;return false;
    }
    return true;
  }
  void stopAfter(uint32_t tailSamples) {
    if(active() && target_.load()==UINT32_MAX)
      target_.store(samples.load(std::memory_order_acquire)+tailSamples,std::memory_order_release);
  }
  void finishAt(uint32_t sampleCount) {
    if(active()) target_.store(std::min<uint32_t>(sampleCount,limit_),std::memory_order_release);
  }
  bool wait(uint32_t timeoutMs) {
    if(!pending_) return true;
    if(xSemaphoreTake(finished_,pdMS_TO_TICKS(timeoutMs))!=pdTRUE) return false;
    pending_=false;return true;
  }
  void cancel() { cancel_=true; wait(500); }
  bool good() const {
    return !active() && stats.samples && !stats.i2sOverflow && !stats.bufferOverflow && !stats.readErrors;
  }
 private:
  i2s_port_t port_=I2S_NUM_0;
  QueueHandle_t events_=nullptr;
  SemaphoreHandle_t finished_=nullptr;
  std::atomic<uint32_t> target_{UINT32_MAX};
  std::atomic<bool> cancel_{false};
  uint32_t limit_=0,testInitialDelayMs_=0;unsigned shift_=16;bool pending_=false;
  void drainEvents() {
    i2s_event_t event;
    while(events_ && xQueueReceive(events_,&event,0)==pdTRUE) {
      if(event.type==I2S_EVENT_RX_Q_OVF) ++stats.i2sOverflow;
      if(event.type==I2S_EVENT_DMA_ERROR) ++stats.readErrors;
    }
  }
  static void run(void* context) {
    auto& self=*static_cast<BufferedMic*>(context);
    int32_t raw[320];
    self.stats.startedMs=millis();
    {
      if(self.testInitialDelayMs_) vTaskDelay(pdMS_TO_TICKS(self.testInitialDelayMs_));
      while(!self.cancel_.load() && !self.stats.readErrors) {
        const uint32_t target=self.target_.load(std::memory_order_acquire);
        if(self.stats.samples>=target) break;
        if(self.stats.samples>=self.limit_) { ++self.stats.bufferOverflow;break; }
        size_t bytes=0;
        const esp_err_t err=i2s_read(self.port_,raw,sizeof(raw),&bytes,pdMS_TO_TICKS(100));
        self.drainEvents();
        if(err!=ESP_OK || !bytes || bytes%4) { ++self.stats.readErrors;break; }
        const uint32_t latestTarget=self.target_.load(std::memory_order_acquire);
        if(latestTarget<=self.stats.samples) break;
        const uint32_t remaining=latestTarget-self.stats.samples;
        const size_t count=std::min<size_t>(bytes/4,std::min<size_t>(remaining,self.limit_-self.stats.samples));
        uint64_t blockSquares=0;
        for(size_t i=0;i<count;++i) {
          const uint32_t magnitude=raw[i]<0?uint32_t(-int64_t(raw[i])):uint32_t(raw[i]);
          self.stats.rawPeak=std::max(self.stats.rawPeak,magnitude);
          int32_t pcm=raw[i]>>self.shift_;
          if(pcm>=32767 || pcm<=-32768) ++self.stats.clips;
          pcm=std::max<int32_t>(-32768,std::min<int32_t>(32767,pcm));
          self.data[self.stats.samples++]=int16_t(pcm);
          self.stats.peak=std::max<uint32_t>(self.stats.peak,pcm<0?-pcm:pcm);
          blockSquares+=int64_t(pcm)*pcm;
        }
        self.stats.sumSquares+=blockSquares;
        self.samples.store(self.stats.samples,std::memory_order_release);
        if(count) self.liveRms.store(uint32_t(sqrt(double(blockSquares)/count)));
      }
    }
    self.drainEvents();
    self.stats.endedMs=millis();
    self.done.store(true,std::memory_order_release);
    xSemaphoreGive(self.finished_);
    vTaskDelete(nullptr);
  }
};
