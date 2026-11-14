#pragma once
#include <ArduinoJson.h>
// Send vendor-private controls only for known DeepSeek model IDs/endpoints.
// CIYUAN may host Claude/GPT models that reject DeepSeek thinking.type.
static bool camDeepSeekPolicy(const String& model,const String& url) {
  return model.startsWith("deepseek-") || url.startsWith("https://api.deepseek.com/");
}
static const char* camThinkingEffort(uint8_t level) {return level==1?"low":level==3?"max":"high";}
static void camApplyLlmPolicy(JsonDocument& request,const String& model,const String& url,bool thinking,bool tools,uint8_t level=2) {
  request.remove("thinking");request.remove("reasoning_effort");request.remove("temperature");
  if(camDeepSeekPolicy(model,url)) {
    request["temperature"]=tools?0.0:0.3;
    if(tools || !thinking){request["thinking"]=serialized("{\"type\":\"disabled\"}");request["reasoning_effort"]="none";}
    else {request["thinking"]=serialized("{\"type\":\"enabled\"}");request["reasoning_effort"]=camThinkingEffort(level);}
  }
}
