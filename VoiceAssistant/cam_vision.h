#pragma once
// Included after LlmSseSink and the existing HTTP/SSE helpers in the .ino.
#include "cam_capture.h"
#include "cam_jpeg_stream.h"
#include "cam_tls.h"
#include "cam_vision_policy.h"

static bool camOtherModelAllowed = false;
static String camVisionDetail = "low";

static bool camCheckVisionConfig() {
  if (!camVisionEnabled && !camPhoto.confirmed && !camHasImageContext()) { lastError = "Photo questions disabled; use voice questions"; return false; }
  if (!cfg.llmApiKey.length() || !cfg.llmApiUrl.length() || !cfg.llmModelId.length()) {
    lastError = "Configure LLM provider/key/model first"; return false;
  }
  if (!cfg.llmApiUrl.startsWith("https://") || cfg.llmApiKey.indexOf('\r') >= 0 ||
      cfg.llmApiKey.indexOf('\n') >= 0) {
    lastError = "Vision requires HTTPS and a valid key"; return false;
  }
  const int capability=camModelVision(cfg.llmApiUrl,cfg.llmModelId);
  if(capability!=1) {
    lastError=capability==0 ? uiText("当前模型不支持图像识别，请切换识图模型","Current model does not support images; select a vision model","Current model does not support images; select a vision model")
        : uiText("模型识图能力未标注，请先标记是否支持识图","Model image support is unmarked; configure its capability first","Model image support is unmarked; configure its capability first");
    return false;
  }
  return true;
}

static bool camBuildVisionParts(size_t assistantIndex, bool stream, String* parts,
    CamJpegSource* sources,size_t& count) {
  count=0;
  String body = buildLlmRequestBodyBudget(assistantIndex, stream, "",
      min<size_t>(cfg.llmContextMaxBytes, 8192),true);
  if (!body.length()) { lastError = "Vision context allocation failed"; return false; }
  JsonDocument req;
  if (deserializeJson(req, body)) { lastError = "Vision JSON allocation failed"; return false; }
  body = "";
  JsonArray messages=req["messages"].as<JsonArray>();
  std::vector<uint8_t> included(messages.size(),0);
  size_t remaining=CAM_CONTEXT_IMAGE_LIMIT;
  for(size_t i=messages.size();i>0;--i) {
    JsonObject message=messages[i-1];
    if(!message["_cam_history_index"].is<size_t>())continue;
    const size_t index=message["_cam_history_index"].as<size_t>();
    if(index>=chatHistory.size() || chatHistory[index].role!="user")continue;
    const size_t n=chatHistory[index].images.size();
    if(n && n<=remaining){included[i-1]=n;remaining-=n;}
  }
  size_t currentUserIndex=chatHistory.size();
  for(size_t i=assistantIndex;i>0;--i)if(chatHistory[i-1].role=="user"){currentUserIndex=i-1;break;}
  for(size_t i=0;i<messages.size();++i) {
    JsonObject message=messages[i];
    const size_t index=message["_cam_history_index"]|size_t(-1);
    message.remove("_cam_history_index"); // Device bookkeeping never reaches the provider.
    if(index>=chatHistory.size() || chatHistory[index].images.empty())continue;
    const String question=message["content"].as<String>();
    if(!included[i]) {
      message["content"]=question+"\n[Earlier photos omitted from this bounded context; do not infer unseen image details.]";
      continue;
    }
    JsonArray content=message["content"].to<JsonArray>();
    JsonObject text=content.add<JsonObject>();text["type"]="text";text["text"] = question;
    for(size_t j=0;j<included[i];++j) {
      const auto& reference=chatHistory[index].images[j];
      sources[count].path=reference.path;sources[count].size=reference.bytes;
      if(index==currentUserIndex && j<camQuestionPhotoCount()) {
        const auto& photo=camQuestionPhotoAt(j);
        if(photo.imagePath==reference.path && photo.size==reference.bytes)sources[count].jpeg=photo.jpeg;
      }
      JsonObject image=content.add<JsonObject>();image["type"] = "image_url";
      image["image_url"]["url"]="__CAM_JPEG_STREAM_01A0F6BF_"+String(count)+"__";
      image["image_url"]["detail"]=camVisionDetail;
      ++count;
    }
  }
  if(!count){lastError="Images outside context window; select photos again";return false;}
  req["stream"] = stream;
  serializeJson(req, body);
  if (req.overflowed() || body.length() != measureJson(req)) {
    lastError = "Vision JSON overflow"; return false;
  }
  const String imagePrefix = "\"data:image/jpeg;base64,";
  size_t cursor=0;
  for(size_t i=0;i<count;++i) {
    const String marker="\"__CAM_JPEG_STREAM_01A0F6BF_"+String(i)+"__\"";
    const int offset=body.lastIndexOf(marker);
    if(offset<0 || size_t(offset)<cursor) {lastError="Vision JSON marker missing";return false;}
    parts[i]=(i?String("\""):String())+body.substring(cursor,offset)+imagePrefix;
    if(parts[i].length()!=(i?1:0)+size_t(offset)-cursor+imagePrefix.length()) {lastError="Vision framing allocation failed";return false;}
    cursor=offset+marker.length();
  }
  parts[count]="\""+body.substring(cursor);
  if(parts[count].length()!=1+body.length()-cursor){lastError="Vision framing allocation failed";return false;}
  return true;
}

static bool camVisionOnce(size_t assistantIndex, bool stream, bool& retryable) {
  retryable = false;
  if (!camPhoto.ready() && !camHasImageContext()) { lastError = "No CAM image context"; return false; }
  size_t count=0;
  String parts[CAM_CONTEXT_IMAGE_LIMIT+1];
  CamJpegSource sources[CAM_CONTEXT_IMAGE_LIMIT];
  if (!camBuildVisionParts(assistantIndex, stream, parts, sources, count)) return false;
  size_t jpegBytes=0;
  for(size_t i=0;i<count;++i) {
    jpegBytes+=sources[i].size;
    if(!sources[i].jpeg) {
      File check;if(camSdReady && sources[i].path.length())check=SD_MMC.open(sources[i].path,FILE_READ);
      const bool valid=check && check.size()==sources[i].size;check.close();
      if(!valid){lastError=uiText("历史图片缺失，请检查TF卡或新建会话","Context photo unavailable; check TF or start a new chat","Context photo unavailable; check TF or start a new chat");return false;}
    }
  }
  CamMultiJpegRequestStream upload(parts,sources,count,SD_MMC);
  if(upload.failed()){lastError="Invalid image request size";return false;}
  WiFiClientSecure client;
  if (!camConfigureTls(client)) {
    lastError = "NTP required for verified TLS"; return false;
  }
  client.setTimeout(8);
  HTTPClient http;
  http.setTimeout(min<uint32_t>(60000,LLM_STREAM_IDLE_TIMEOUT_MS));
  http.setConnectTimeout(ASR_CONNECT_TIMEOUT_MS);
  http.setReuse(false);
  if (!http.begin(client, cfg.llmApiUrl)) {
    lastError = "Vision http.begin failed"; return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + cfg.llmApiKey);
  http.addHeader("Accept", stream ? "text/event-stream" : "application/json");
  http.addHeader("Accept-Encoding", "identity");
  http.addHeader("Connection", "close");
  const char* headers[] = {"Transfer-Encoding", "Content-Type"};
  http.collectHeaders(headers, 2);
  camPrintMemory("before vision TLS/upload");
  const uint32_t started = millis();
  Serial.printf("[CAM-VISION] model=%s stream=%d images=%u JPEG=%u request=%u detail=%s\n",
      cfg.llmModelId.c_str(), stream, unsigned(count), unsigned(jpegBytes), unsigned(upload.size()), camVisionDetail.c_str());
  const int code = http.sendRequest("POST", &upload, upload.size());
  if (upload.failed()) {
    http.end(); lastError = "JPEG source read failed"; return false;
  }
  if (code < 200 || code >= 300) {
    // Do not capture arbitrary response bodies; error pages can be very large.
    http.end();
    lastError = "Vision HTTP " + String(code);
    retryable = code < 0 || code == 429 || code >= 500;
    Serial.println("[CAM-VISION] " + lastError);
    return false;
  }
  std::unique_ptr<LlmSseSink> sinkOwner(new(std::nothrow)LlmSseSink(assistantIndex));
  if(!sinkOwner){http.end();lastError="Vision stream memory unavailable";return false;}
  LlmSseSink& sink=*sinkOwner;
  const uint32_t spent=millis()-camVisionRequestStarted;
  if(spent>=LLM_STREAM_TOTAL_TIMEOUT_MS){http.end();lastError="Vision request deadline reached";return false;}
  const uint32_t remaining=LLM_STREAM_TOTAL_TIMEOUT_MS-spent;
  const int result = readLlmBodyToSink(http, sink, min<uint32_t>(LLM_STREAM_IDLE_TIMEOUT_MS,remaining), remaining);
  sink.finish();
  http.end();
  if(camVisionCancelRequested) {lastError=uiText("已取消，图片保留供重试","Cancelled; photos kept for retry","Cancelled; photos kept for retry");return false;}
  if(sink.outputLimited()) {lastError=uiText("回复达到上限，请缩小问题范围","Reply limit reached; narrow the question","Reply limit reached; narrow the question");return false;}
  if (result < 0 || sink.failed() || sink.truncated() || (sink.sawSse() && !sink.done())) {
    lastError = "Vision stream incomplete " + String(result);
    // A response already in progress must not restart its long reasoning.
    retryable = camVisionMayRetryResponse(sink.sawSse(),result,sink.outputLimited(),camVisionCancelRequested);
    return false;
  }
  String answer;
  if (sink.sawSse()) answer = sink.answer();
  else {
    JsonDocument metadata;
    JsonDocument filter;
    filter["choices"][0]["finish_reason"] = true;
    if (deserializeJson(metadata, sink.fallback(), DeserializationOption::Filter(filter))) {
      lastError = "Vision full JSON invalid"; return false;
    }
    const String finish = metadata["choices"][0]["finish_reason"] | "";
    if (finish.length() && finish != "stop") {
      lastError = "Vision response incomplete: " + finish; return false;
    }
    if (!parseLlmFullJson(sink.fallback(), answer)) return false;
  }
  answer = sanitizeUtf8(answer);
  answer.trim();
  if (!answer.length()) { lastError = "Vision response has no text"; return false; }
  updateChatMessageContent(assistantIndex, answer);
  lastLlmResponseWasSse = sink.sawSse();
  Serial.printf("[CAM-VISION] complete SSE=%d answer=%u total_ms=%lu\n",
      sink.sawSse(), unsigned(answer.length()), (unsigned long)(millis() - started));
  return true;
}

static bool camAskAndStoreVision(String question) {
  if (chatSessionOnTf() && !chatStorageReady()) {setState(AppState::Error,lastError);return false;}
  if (camQuestionRequiresPhoto && (!camPhoto.ready() || !camPhoto.confirmed)) {
    lastError = "Photo missing; retake or cancel the image question";
    setState(AppState::Error, lastError); return false;
  }
  question = sanitizeUtf8(question);
  question.trim();
  if (!question.length()) {
    lastError = "Empty image question";
    setState(AppState::Error, lastError); return false;
  }
  if (!camCheckVisionConfig() || !connectWifi()) {
    lastFailedQuestion = question;
    lastFailedUseSearch = false;
    setState(AppState::Error, lastError); return false;
  }
  // The SDK trust bundle validates certificate dates; don't proceed on a 1970 clock.
  if (!camEnsureTlsClock()) {
    lastError = "NTP required for verified TLS";
    lastFailedQuestion = question;
    setState(AppState::Error, lastError); return false;
  }
  lastError = "";
  composingQuestion = false;
  pendingQuestion = "";
  pendingSegments.clear();
  transcript = asrBestTranscript = asrStableTranscript = asrPartialTranscript = "";
  const bool retrySame = lastFailedQuestion == question && !chatHistory.empty() &&
      chatHistory.back().role == "user" && String(chatHistory.back().content) == question;
  if (!retrySame) addChatMessage("user", question, false);
  if(camQuestionPhotoCount() && !chatHistory.empty() && chatHistory.back().role=="user") {
    auto& references=chatHistory.back().images;references.clear();
    for(size_t i=0;i<camQuestionPhotoCount();++i) {
      const auto& photo=camQuestionPhotoAt(i);references.push_back({photo.imagePath,photo.size});
    }
  }
  addChatMessage("assistant", "", false);
  const size_t assistantIndex = chatHistory.size() - 1;
  // SPIFFS retains existing bounded text/context semantics. TF gets a separate audit record.
  saveChatHistory();
  if (uiPage == UiPage::Page1 && !screenSleeping) {
    chatAutoScrollBottom = true;
    drawChatWindow(true);
  }
  setState(AppState::Thinking, uiText("正在发送图片与问题...", "Uploading photo + question...", "Uploading photo + question..."));
  struct RequestScope {
    RequestScope(){camVisionRequestActive=true;camVisionCancelRequested=false;camVisionRequestStarted=millis();++buttonUiGeneration;}
    ~RequestScope(){camVisionRequestActive=false;++buttonUiGeneration;}
  } requestScope;
  bool ok = false;
  bool retryable = false;
  // At most one fresh-client retry before any response; never repeatedly restart thinking.
  for (uint8_t attempt = 0; attempt < 2; ++attempt) {
    if(camPollVisionCancel() || millis()-camVisionRequestStarted>=LLM_STREAM_TOTAL_TIMEOUT_MS) {
      retryable=false;lastError=camVisionCancelRequested?"Vision request cancelled":"Vision request deadline reached";break;
    }
    if (attempt) {
      delay(700U << (attempt - 1));
      updateChatMessageContent(assistantIndex, "");
    }
    ok = camVisionOnce(assistantIndex, true, retryable);
    if (ok || !retryable) break;
  }
  // Full JSON fallback still contains the same image, never a text-only retry.
  if (!ok && retryable && !cfg.enableThinking && !camVisionCancelRequested) {
    updateChatMessageContent(assistantIndex, "");
    ok = camVisionOnce(assistantIndex, false, retryable);
  }
  const String imagePath = camPhoto.imagePath;
  camAppendHistory(cfg.currentSession, "user", question, cfg.llmProviderName,
      cfg.llmModelId, ok ? "complete" : "pending", imagePath);
  if (!ok) {
    const String partial = String(chatHistory[assistantIndex].content);
    if (partial.length()) {
      camAppendHistory(cfg.currentSession, "assistant", partial, cfg.llmProviderName,
          cfg.llmModelId, "incomplete", imagePath);
    }
    // Partial replies are archived as incomplete, excluded from successful context.
    removeChatMessage(assistantIndex);
    saveChatHistory();
    lastFailedQuestion = question;
    lastFailedUseSearch = false;
    setState(AppState::Error, lastError);
    return false; // Keep PSRAM JPEG + question for A/cam retry.
  }
  const String answer = String(chatHistory[assistantIndex].content);
  camAppendHistory(cfg.currentSession, "assistant", answer, cfg.llmProviderName,
      cfg.llmModelId, "complete", imagePath);
  camReleasePhoto();
  // Album chat can continue with voice while the global capture-only mode stays on.
  camQuestionReadyForVoice=camPhotoOnly && camHasImageContext();
  lastError = lastFailedQuestion = "";
  lastFailedUseSearch = false;
  saveChatHistory();
  paginate(answer);
  chatScroll = SIZE_MAX;
  showPage();
  camPrintMemory("vision complete / JPEG released");
  return true;
}
