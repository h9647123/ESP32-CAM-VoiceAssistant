#pragma once
struct CamModelCapability {String url,model;bool vision;};
static std::vector<CamModelCapability> camModelCapabilities;
static int camModelVision(const String& url,const String& model) {
  for (const auto& entry:camModelCapabilities) if(entry.url==url && entry.model==model) return entry.vision?1:0;
  return -1; // Legacy/unspecified is distinct from explicitly unsupported.
}
static bool camSetModelVision(const String& url,const String& model,bool vision) {
  if(!url.length() || !model.length())return false;
  for(auto& entry:camModelCapabilities)if(entry.url==url && entry.model==model){entry.vision=vision;return true;}
  if(camModelCapabilities.size()>=64)return false;
  camModelCapabilities.push_back({url,model,vision});return true;
}
static void camExportModelCapabilities(JsonArray array) {
  for(const auto& entry:camModelCapabilities){auto item=array.add<JsonObject>();item["api_url"]=entry.url;item["model"]=entry.model;item["vision"]=entry.vision;}
}
static void camImportModelCapabilities(JsonArrayConst array) {
  for(JsonObjectConst item:array)if(item["vision"].is<bool>())
    camSetModelVision(item["api_url"].as<String>(),item["model"].as<String>(),item["vision"].as<bool>());
}
static void camLoadModelCapabilities() {
  camModelCapabilities.clear();JsonDocument doc;
  if(!deserializeJson(doc,runtimePrefs.getString("model_caps","[]")))camImportModelCapabilities(doc.as<JsonArrayConst>());
}
static void camSaveModelCapabilities() {
  JsonDocument doc;camExportModelCapabilities(doc.to<JsonArray>());String json;serializeJson(doc,json);
  runtimePrefs.putString("model_caps",json);
}
static bool camParseModelCapability(String& model,bool& vision) {
  model.trim();const int space=model.lastIndexOf(' ');if(space<1)return false;
  String tag=model.substring(space+1);tag.toLowerCase();
  if(tag!="vision" && tag!="text")return false;
  model=model.substring(0,space);model.trim();vision=tag=="vision";return model.length()>0;
}
