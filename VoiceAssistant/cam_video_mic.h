#pragma once
// Independent bounded PCM ring. The task only reads I2S/converts samples; SD,
// JPEG decoding and display stay on the consumer core. No ASR/network traffic.
class CamVideoMic {
 public:
  static constexpr uint32_t CAPACITY=131072;
  uint8_t* pcm=nullptr;
  std::atomic<uint32_t> produced{0},consumed{0};
  std::atomic<bool> running{false},stopRequested{false};
  uint32_t overflow=0,readErrors=0,i2sOverflow=0,clips=0,startedMs=0,endedMs=0;
  bool begin() {
    if(running || bufferedMic.active())return false;
    if(!pcm)pcm=(uint8_t*)heap_caps_malloc(CAPACITY,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!pcm)return false;
    int32_t stale[256];size_t bytes=0;
    for(unsigned i=0;i<32;++i) {if(i2s_read(MIC_I2S_PORT,stale,sizeof(stale),&bytes,0)!=ESP_OK)return false;if(!bytes)break;if(i==31)return false;}
    if(micEvents)xQueueReset(micEvents);
    produced=consumed=0;overflow=readErrors=i2sOverflow=clips=0;startedMs=endedMs=0;
    stopRequested=false;running=true;
    if(xTaskCreatePinnedToCore(run,"video-mic",4096,this,4,nullptr,0)!=pdPASS){running=false;return false;}return true;
  }
  uint32_t available() const {return produced.load(std::memory_order_acquire)-consumed.load(std::memory_order_acquire);}
  bool stop() {
    stopRequested=true;uint32_t at=millis();while(running && millis()-at<300)delay(1);
    return !running;
  }
  bool good() const {return !running && produced.load()>0 && !overflow && !readErrors && !i2sOverflow;}
 private:
  static void run(void* context) {
    auto& self=*static_cast<CamVideoMic*>(context);int32_t raw[320];self.startedMs=millis();
    while(!self.stopRequested) {
      size_t bytes=0;const esp_err_t error=i2s_read(MIC_I2S_PORT,raw,sizeof(raw),&bytes,pdMS_TO_TICKS(100));
      i2s_event_t event;while(micEvents && xQueueReceive(micEvents,&event,0)==pdTRUE) {
        if(event.type==I2S_EVENT_RX_Q_OVF)++self.i2sOverflow;
        if(event.type==I2S_EVENT_DMA_ERROR)++self.readErrors;
      }
      if(error!=ESP_OK || !bytes || bytes%4) {++self.readErrors;break;}
      const uint32_t head=self.produced.load(),tail=self.consumed.load(std::memory_order_acquire);
      const size_t count=bytes/4;
      if(head-tail+count*2>CAPACITY){++self.overflow;break;}
      for(size_t i=0;i<count;++i) {
        int32_t value=raw[i]>>MIC_SAMPLE_SHIFT;
        if(value>32767 || value< -32768)++self.clips;
        value=max<int32_t>(-32768,min<int32_t>(32767,value));
        self.pcm[(head+2*i)%CAPACITY]=uint8_t(value);self.pcm[(head+2*i+1)%CAPACITY]=uint16_t(value)>>8;
      }
      self.produced.store(head+count*2,std::memory_order_release);
      if(self.i2sOverflow || self.readErrors)break;
    }
    self.endedMs=millis();self.running.store(false,std::memory_order_release);vTaskDelete(nullptr);
  }
};
static CamVideoMic camVideoMic;
