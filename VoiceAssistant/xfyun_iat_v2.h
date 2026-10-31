#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>
#include <map>
#include <vector>

// Pure protocol state. No network, private config, NVS, UI or audio capture here.
struct XfyunIatV2 {
  std::map<int, String> segments;
  String text;
  int errorCode = 0;
  bool acknowledged = false;
  bool finalReceived = false;
  uint32_t audioSequence = 0;

  void reset() { segments.clear(); text = ""; errorCode = 0; acknowledged = finalReceived = false; audioSequence = 0; }

  static String hmac256(const String& key, const String& data) {
    uint8_t digest[32];
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!info || mbedtls_md_hmac(info, reinterpret_cast<const uint8_t*>(key.c_str()), key.length(),
        reinterpret_cast<const uint8_t*>(data.c_str()), data.length(), digest) != 0) return "";
    return base64::encode(digest, sizeof(digest));
  }

  String audioFrame(const String& appId, const uint8_t* data, size_t length, bool last, int languageFilter, bool compatibleSubset = false) {
    const unsigned status = last ? 2 : (audioSequence == 0 ? 0 : 1);
    JsonDocument frame;
    frame["header"]["app_id"] = appId;
    frame["header"]["status"] = status;
    if (audioSequence == 0) {
      JsonObject iat = frame["parameter"]["iat"].to<JsonObject>();
      iat["domain"] = "spark_asr_v2";
      iat["language"] = "zh_cn";
      iat["accent"] = "mandarin";
      if (!compatibleSubset) {
        iat["dwa"] = "wpgs";
        iat["eos"] = 60000;
        iat["bos"] = 60000;
        iat["ltc"] = languageFilter;
      }
      iat["result"]["encoding"] = "utf8";
      iat["result"]["compress"] = "raw";
      iat["result"]["format"] = "json";
    }
    JsonObject audio = frame["payload"]["audio"].to<JsonObject>();
    audio["encoding"] = "raw";
    audio["sample_rate"] = 16000;
    audio["channels"] = 1;
    audio["bit_depth"] = 16;
    audio["seq"] = audioSequence++;
    audio["status"] = status;
    audio["audio"] = length ? base64::encode(data, length) : String("");
    String result;
    serializeJson(frame, result);
    return result;
  }

  bool receive(JsonDocument& packet) {
    if (!packet["header"]["code"].is<int>()) { errorCode = -1; return false; }
    errorCode = packet["header"]["code"].as<int>();
    if (errorCode) return false;
    acknowledged = true;
    JsonVariantConst result = packet["payload"]["result"];
    const String encoded = result["text"] | "";
    if (encoded.length()) {
      if (encoded.length() > 65536) { errorCode = -2; return false; }
      size_t needed = 0;
      const uint8_t* input = reinterpret_cast<const uint8_t*>(encoded.c_str());
      const int probe = mbedtls_base64_decode(nullptr, 0, &needed, input, encoded.length());
      if ((probe && probe != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) || needed > 49152) { errorCode = -3; return false; }
      std::vector<uint8_t> decoded(needed + 1);
      size_t size = 0;
      if (mbedtls_base64_decode(decoded.data(), decoded.size(), &size, input, encoded.length())) { errorCode = -3; return false; }
      JsonDocument words;
      if (deserializeJson(words, decoded.data(), size)) { errorCode = -4; return false; }
      const int ret = words["ret"] | 0;
      if (ret) { errorCode = ret; return false; }
      if (words["ws"].is<JsonArray>()) {
        const int sn = words["sn"] | (result["seq"] | 0);
        if (sn < 0 || sn > 999999) { errorCode = -5; return false; }
        String piece;
        for (JsonObjectConst word : words["ws"].as<JsonArrayConst>()) {
          // cw entries are alternatives, not consecutive words.
          piece += word["cw"][0]["w"] | "";
        }
        const String pgs = words["pgs"] | (result["pgs"] | "apd");
        if (pgs == "rpl") {
          JsonArrayConst range = words["rg"].is<JsonArray>() ? words["rg"].as<JsonArrayConst>() : result["rg"].as<JsonArrayConst>();
          if (range.size() != 2) { errorCode = -6; return false; }
          const int low = range[0], high = range[1];
          if (low < 0 || high < low) { errorCode = -6; return false; }
          for (auto it = segments.lower_bound(low); it != segments.end() && it->first <= high;) it = segments.erase(it);
        }
        if (!segments.count(sn) && segments.size() >= 512) { errorCode = -7; return false; }
        segments[sn] = piece;
        String assembled;
        for (const auto& segment : segments) {
          if (assembled.length() + segment.second.length() > 8192) { errorCode = -8; return false; }
          assembled += segment.second;
        }
        text = assembled; // Dynamic correction may legitimately shorten text.
      }
    }
    finalReceived = (packet["header"]["status"] | -1) == 2 || (result["status"] | -1) == 2;
    return true;
  }
};

static bool xfyunIatV2SelfTest() {
  bool pass = XfyunIatV2::hmac256("key", "The quick brown fox jumps over the lazy dog") == "97yD9DBThCSxMpjmqm+xQ+9NWaFJRhdZl0edvC0aPNg=";
  XfyunIatV2 state;
  const uint8_t pcm[] = {0, 0};
  JsonDocument packet;
  deserializeJson(packet, state.audioFrame("test-app", pcm, sizeof(pcm), false, 1));
  pass = pass && packet["header"]["status"] == 0 && packet["parameter"]["iat"]["domain"] == "spark_asr_v2" && packet["payload"]["audio"]["encoding"] == "raw";
  deserializeJson(packet, state.audioFrame("test-app", nullptr, 0, true, 1));
  pass = pass && packet["header"]["status"] == 2 && packet["payload"]["audio"]["seq"] == 1 && packet["payload"]["audio"]["audio"] == "";
  state.reset();
  deserializeJson(packet, state.audioFrame("test-app", pcm, sizeof(pcm), false, 1, true));
  pass = pass && packet["parameter"]["iat"]["eos"].isNull() && packet["parameter"]["iat"]["bos"].isNull() && packet["parameter"]["iat"]["ltc"].isNull() && packet["parameter"]["iat"]["domain"] == "spark_asr_v2";
  state.reset();
  auto response = [&](int sn, const char* word, bool replace) {
    JsonDocument body;
    body["sn"] = sn;
    body["ws"][0]["cw"][0]["w"] = word;
    if (replace) { body["pgs"] = "rpl"; body["rg"][0] = 0; body["rg"][1] = 1; }
    String json;
    serializeJson(body, json);
    packet.clear(); packet["header"]["code"] = 0; packet["header"]["status"] = 1;
    packet["payload"]["result"]["text"] = base64::encode(reinterpret_cast<const uint8_t*>(json.c_str()), json.length());
    return state.receive(packet);
  };
  pass = response(0, "wrong long text", false) && pass;
  pass = response(1, "tail", false) && pass;
  pass = response(2, "ok", true) && state.text == "ok" && pass;
  pass = response(2, "ok", false) && state.text == "ok" && pass; // No duplicate on repeat SN.
  packet.clear(); packet["header"]["code"] = 0; packet["header"]["status"] = 2;
  pass = state.receive(packet) && state.finalReceived && state.text == "ok" && pass;
  packet.clear(); packet["header"]["code"] = 11201;
  pass = !state.receive(packet) && state.errorCode == 11201 && pass;
  packet.clear(); packet["header"]["code"] = 0; packet["payload"]["result"]["text"] = "%%%";
  pass = !state.receive(packet) && state.errorCode == -3 && pass;
  return pass;
}
