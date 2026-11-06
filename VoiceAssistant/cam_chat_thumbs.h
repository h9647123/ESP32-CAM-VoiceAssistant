#pragma once
#include "cam_chat_thumb_policy.h"

struct CamChatThumbCache {
  LGFX_Sprite sprite;
  String key;
  uint32_t used=0;
  bool ready=false;
  CamChatThumbCache():sprite(&tft){}
};
static CamChatThumbCache camChatThumbCache[3];
static uint32_t camChatThumbEpoch=0,camChatThumbHits=0,camChatThumbBuilds=0,camChatThumbMaxMs=0;
static void camChatThumbInvalidate() {
  ++camChatThumbEpoch;
  for(auto& item:camChatThumbCache)item.key="";
}
static void camChatThumbCorners(LGFX_Sprite& sprite,int x,int y,int w,int h) {
  constexpr int radius=6;
  for(int cy=0;cy<radius;++cy)for(int cx=0;cx<radius;++cx)
    if((radius-1-cx)*(radius-1-cx)+(radius-1-cy)*(radius-1-cy)>(radius-1)*(radius-1)) {
      sprite.drawPixel(x+cx,y+cy,TFT_BLACK);sprite.drawPixel(x+w-1-cx,y+cy,TFT_BLACK);
      sprite.drawPixel(x+cx,y+h-1-cy,TFT_BLACK);sprite.drawPixel(x+w-1-cx,y+h-1-cy,TFT_BLACK);
    }
}
static CamChatThumbCache* camChatThumbGet(size_t index) {
  if(index>=chatHistory.size() || chatHistory[index].role!="user" || chatHistory[index].images.empty())return nullptr;
  const auto& message=chatHistory[index];
  const size_t count=min<size_t>(CAM_CHAT_THUMB_LIMIT,message.images.size());
  String key=String(cfg.currentSession)+"|"+String(camChatThumbEpoch)+"|"+String(camSdReady);
  for(size_t i=0;i<count;++i) {
    key+="|"+message.images[i].path+":"+String(unsigned(message.images[i].bytes));
    if(!message.images[i].path.length())key+="@"+String(message.images[i].generation);
  }
  for(auto& item:camChatThumbCache)if(item.key==key) {++camChatThumbHits;item.used=millis();return item.ready?&item:nullptr;}
  auto* slot=&camChatThumbCache[0];
  for(auto& item:camChatThumbCache)if(!item.key.length() || item.used<slot->used)slot=&item;
  slot->key=key;slot->used=millis();
  if(!slot->ready) {
    slot->sprite.setPsram(true);slot->sprite.setColorDepth(16);
    slot->ready=slot->sprite.createSprite(CAM_CHAT_THUMB_ROW_WIDTH,CAM_CHAT_THUMB_ROW_HEIGHT)!=nullptr;
  }
  if(!slot->ready)return nullptr;
  const uint32_t started=millis();++camChatThumbBuilds;unsigned missing=0;
  auto& canvas=slot->sprite;canvas.fillSprite(TFT_BLACK);
  size_t latest=chatHistory.size();for(size_t i=chatHistory.size();i>0;--i)if(chatHistory[i-1].role=="user"){latest=i-1;break;}
  for(size_t i=0;i<count;++i) {
    const int x=int(i)*(CAM_CHAT_THUMB_WIDTH+CAM_CHAT_THUMB_GAP),y=3;
    constexpr int innerWidth=CAM_CHAT_THUMB_WIDTH-2,innerHeight=CAM_CHAT_THUMB_HEIGHT-2;
    bool drawn=false;const auto& image=message.images[i];
    canvas.fillRoundRect(x,y,CAM_CHAT_THUMB_WIDTH,CAM_CHAT_THUMB_HEIGHT,6,canvas.color565(18,26,34));
    canvas.setClipRect(x+1,y+1,innerWidth,innerHeight);
    // Archived photos are decoded directly from TF; no full JPEG/RGB copy.
    if(camSdReady && image.path.startsWith("/photo/") && image.path.endsWith(".jpg") && image.path.indexOf("..")<0) {
      File file=SD_MMC.open(image.path,FILE_READ);uint16_t w=0,h=0;
      const bool valid=file && file.size()==image.bytes && image.bytes<=CAM_JPEG_MAX_BYTES && camChatThumbJpegSize(file,w,h);
      file.close();
      if(valid) {
        const float scale=min(float(innerWidth)/w,float(innerHeight)/h);
        const int left=x+1+(innerWidth-int(w*scale))/2,top=y+1+(innerHeight-int(h*scale))/2;
        drawn=canvas.drawJpgFile(static_cast<fs::FS&>(SD_MMC),image.path.c_str(),left,top,innerWidth,innerHeight,0,0,scale,scale);
      }
    }
    if(!drawn && image.generation && index==latest && i<camQuestionPhotoCount()) {
      const auto& photo=camQuestionPhotoAt(i);
      if(photo.jpeg && photo.generation==image.generation && photo.size==image.bytes && photo.imagePath==image.path && photo.width && photo.height) {
        const float scale=min(float(innerWidth)/photo.width,float(innerHeight)/photo.height);
        drawn=canvas.drawJpg(photo.jpeg,photo.size,x+1+(innerWidth-int(photo.width*scale))/2,
          y+1+(innerHeight-int(photo.height*scale))/2,innerWidth,innerHeight,0,0,scale,scale);
      }
    }
    canvas.clearClipRect();
    if(!drawn) {
      ++missing;
      canvas.setFont(&fonts::efontCN_12);canvas.setTextColor(TFT_LIGHTGREY,canvas.color565(18,26,34));
      const String label=uiText("不可用","Missing","Missing");
      canvas.setCursor(x+(CAM_CHAT_THUMB_WIDTH-canvas.textWidth(label))/2,y+17);canvas.print(label);
    }
    camChatThumbCorners(canvas,x,y,CAM_CHAT_THUMB_WIDTH,CAM_CHAT_THUMB_HEIGHT);
    canvas.drawRoundRect(x,y,CAM_CHAT_THUMB_WIDTH,CAM_CHAT_THUMB_HEIGHT,6,canvas.color565(70,94,112));
    canvas.fillCircle(x+8,y+CAM_CHAT_THUMB_HEIGHT-8,6,canvas.color565(12,20,28));
    canvas.setFont(&fonts::Font0);canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(x+6,y+CAM_CHAT_THUMB_HEIGHT-11);canvas.print(i+1);
  }
  camChatThumbMaxMs=max(camChatThumbMaxMs,millis()-started);
  Serial.printf("[CHAT-THUMB] row_built=%u images=%u missing=%u decode_ms=%u psram_bytes=%u\n",unsigned(index),unsigned(count),missing,unsigned(millis()-started),CAM_CHAT_THUMB_ROW_WIDTH*CAM_CHAT_THUMB_ROW_HEIGHT*2);
  return slot;
}
static void camChatThumbDrawBand(size_t index,unsigned band,int y) {
  auto* item=camChatThumbGet(index);
  if(chatSpriteReady) {
    const int top=max(0,y),bottom=min(CHAT_AREA_HEIGHT,y+CHAT_LINE_HEIGHT);
    if(top>=bottom)return;
    chatSprite.setClipRect(0,top,CHAT_AREA_WIDTH,bottom-top);
    if(item)item->sprite.pushSprite(&chatSprite,14,y-int(band)*CHAT_LINE_HEIGHT);
    chatSprite.clearClipRect();
  } else {
    const int top=max(CHAT_AREA_TOP,CHAT_AREA_TOP+y),bottom=min(CHAT_AREA_BOTTOM,CHAT_AREA_TOP+y+CHAT_LINE_HEIGHT);
    if(top>=bottom)return;
    tft.setClipRect(0,top,CHAT_AREA_WIDTH,bottom-top);
    if(item)item->sprite.pushSprite(&tft,14,CHAT_AREA_TOP+y-int(band)*CHAT_LINE_HEIGHT);
    tft.setClipRect(0,CHAT_AREA_TOP,CHAT_AREA_WIDTH,CHAT_AREA_HEIGHT);
  }
}
