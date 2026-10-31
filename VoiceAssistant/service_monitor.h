#pragma once
#include "service_probe_policy.h"

// This worker owns its sockets and an immutable snapshot. It never touches
// microphone/ASR sessions, drafts, cfg, display or NVS, and sends no audio/LLM prompt.
struct ServiceProbe {
  IatEndpoint asr, ai;
  String asrPath, asrHeaders, aiKey, fingerprint;
  String asrDetail,aiDetail;
  bool asrConfigured=false, aiConfigured=false, full=false,customIat=false;
  bool asrOk=false, aiOk=false;
  int asrHttp=0, aiHttp=0;
  uint32_t started=0, elapsed=0;
  std::atomic<bool> cancel{false}, done{false};
};
static ServiceProbe* serviceProbe=nullptr;
static bool serviceWasOnline=false, servicePending=false, serviceFullPending=false;
static bool serviceAiKnown=false, serviceAiOk=false;
static uint32_t serviceLastStarted=0, serviceCompleted=0, serviceStarted=0;
static bool serviceWifiPaused=false, serviceSavedReconnect=false;
static constexpr uint32_t ASR_MONITOR_INTERVAL_MS=60000;

static void cancelServiceProbe() {
  if(serviceProbe) serviceProbe->cancel=true;
}

static bool serviceReadHeaders(NetworkClient& client, ServiceProbe& job,
    String& headers) {
  headers="";
  const uint32_t started=millis();
  while (!job.cancel && WiFi.status()==WL_CONNECTED && millis()-started<SERVICE_HEADER_TIMEOUT_MS) {
    while(client.available()) {
      headers+=char(client.read());
      if(headers.length()>8192) return false;
      if(headers.endsWith("\r\n\r\n")) return true;
    }
    if(!client.connected()) break;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  return false;
}

static bool serviceConnect(NetworkClient& client, WiFiClientSecure& tls,
    const IatEndpoint& endpoint, ServiceProbe& job,String& detail,bool allowDnsFallback=false) {
  detail="transport";
  if(job.cancel || WiFi.status()!=WL_CONNECTED) {detail="offline_or_cancelled";return false;}
  if(endpoint.tls) {
    if(time(nullptr)<1700000000) {detail="clock_not_ready";return false;}
    if(!camConfigureTls(tls)) {detail="tls_clock_or_trust";return false;}
    tls.setHandshakeTimeout(SERVICE_TLS_HANDSHAKE_SECONDS);
  }
  client.setTimeout(250);
  client.setConnectionTimeout(SERVICE_CONNECT_TIMEOUT_MS);
  const bool ok=iatConnectWithDnsFallback(client,tls,endpoint,SERVICE_CONNECT_TIMEOUT_MS,allowDnsFallback,&job.cancel) && !job.cancel;
  if(ok)detail="connected";
  return ok;
}

static bool serviceProbeAsr(ServiceProbe& job) {
  if(!job.asrConfigured) {job.asrDetail="not_configured";return false;}
  WiFiClientSecure tls;
  WiFiClient plain;
  NetworkClient& client=job.asr.tls ? static_cast<NetworkClient&>(tls) : static_cast<NetworkClient&>(plain);
  if(!serviceConnect(client,tls,job.asr,job,job.asrDetail,job.customIat)) { client.stop(); return false; }
  uint8_t nonce[16]; esp_fill_random(nonce,sizeof(nonce));
  const String key=base64::encode(nonce,sizeof(nonce));
  String request="GET "+job.asrPath+" HTTP/1.1\r\nHost: "+job.asr.authority+
      "\r\nConnection: Upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: "+key+
      "\r\n"+job.asrHeaders+"User-Agent: esp32-cam-health\r\n\r\n";
  String response;
  bool ok=client.write(reinterpret_cast<const uint8_t*>(request.c_str()),request.length())==request.length()
      && serviceReadHeaders(client,job,response);
  job.asrDetail=ok?"websocket_verification":"http_headers_timeout_or_closed";
  if(ok) {
    job.asrHttp=response.substring(9,12).toInt();
    const String material=key+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    uint8_t digest[20];
    ok=mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA1),
        reinterpret_cast<const uint8_t*>(material.c_str()),material.length(),digest)==0;
    const String expected=base64::encode(digest,sizeof(digest));
    String lower=response; lower.toLowerCase();
    const int at=lower.indexOf("\r\nsec-websocket-accept:");
    String accept=at<0 ? String("") : response.substring(at+23,response.indexOf("\r\n",at+2));
    accept.trim();
    ok=ok && job.asrHttp==101 && lower.indexOf("upgrade: websocket")>=0 && accept==expected;
    if(ok) {
      // Masked, normal-close frame; no recognition session/audio is started.
      uint8_t closeFrame[]={0x88,0x82,0x27,0x31,0x45,0x19,uint8_t(0x03^0x27),uint8_t(0xe8^0x31)};
      client.write(closeFrame,sizeof(closeFrame));
    }
  }
  client.stop();
  if(ok)job.asrDetail="ok";
  return ok && !job.cancel;
}

static bool serviceProbeAiNetwork(ServiceProbe& job) {
  if(!job.aiConfigured || job.cancel) {job.aiDetail="not_configured_or_cancelled";return false;}
  WiFiClientSecure tls;
  WiFiClient plain;
  NetworkClient& client=job.ai.tls ? static_cast<NetworkClient&>(tls) : static_cast<NetworkClient&>(plain);
  if(!serviceConnect(client,tls,job.ai,job,job.aiDetail)) { client.stop(); return false; }
  const String request="HEAD "+job.ai.path+" HTTP/1.1\r\nHost: "+job.ai.authority+
      "\r\nAuthorization: Bearer "+job.aiKey+"\r\nConnection: close\r\n\r\n";
  String headers;
  const bool received=client.write(reinterpret_cast<const uint8_t*>(request.c_str()),request.length())==request.length()
      && serviceReadHeaders(client,job,headers);
  if(received) job.aiHttp=headers.substring(9,12).toInt();
  job.aiDetail=received?"http_status":"http_headers_timeout_or_closed";
  client.stop();
  // HEAD may be unsupported (405). This is an endpoint/network verdict only.
  return received && job.aiHttp>=200 && job.aiHttp<500 && job.aiHttp!=401 && job.aiHttp!=403 && job.aiHttp!=404;
}

static void serviceProbeWorker(void* argument) {
  auto& job=*static_cast<ServiceProbe*>(argument);
  job.asrOk=serviceProbeAsr(job);
  if(job.full && !job.cancel) job.aiOk=serviceProbeAiNetwork(job);
  job.elapsed=millis()-job.started;
  job.done.store(true,std::memory_order_release);
  vTaskDelete(nullptr);
}

static void requestBackgroundServiceCheck() {
  if(WiFi.status()!=WL_CONNECTED) {
    serviceCheckSummary="OFFLINE";
    footerStatusDetail=uiText("网络未连接", "Offline", "Offline");
    Serial.println("[HEALTH] manual rejected: offline; no reconnect attempted");
    return;
  }
  servicePending=serviceFullPending=true;
  Serial.println("[HEALTH] manual check queued");
}

static bool serviceProbeBusy() {
  return recording || stoppingRecording || asrSessionActive || asrConnected ||
      bufferedMic.active() || appState==AppState::Thinking || camPhotoUiActive();
}

static void serviceMonitorTick() {
  const bool online=WiFi.status()==WL_CONNECTED;
  if(serviceProbe && serviceProbe->done.load(std::memory_order_acquire)) {
    auto& job=*serviceProbe;
    if(!job.cancel && online && job.fingerprint==serviceCheckConfigFingerprint()) {
      if(job.full) { serviceAiKnown=true; serviceAiOk=job.aiOk; }
      serviceCheckSummary=String("ASR ")+(job.asrOk?"OK":"FAIL")+
          (serviceAiKnown ? (serviceAiOk?" / AI NET OK":" / AI NET FAIL") : "");
      serviceCheckFingerprint=job.fingerprint;
      serviceCheckUptimeMs=millis(); serviceCheckBootId=runtimeBootId;
      serviceCheckLastAt=dateText()+" "+timeText();
      ++serviceCompleted;
      Serial.printf("[HEALTH] done sequence=%lu asr=%d ws_http=%d full=%d ai_net=%d ai_http=%d elapsed_ms=%lu\n",
          (unsigned long)serviceCompleted,job.asrOk,job.asrHttp,job.full,job.aiOk,job.aiHttp,(unsigned long)job.elapsed);
      Serial.printf("[HEALTH-PHASE] asr=%s ai=%s\n",job.asrDetail.c_str(),job.full?job.aiDetail.c_str():"not_checked");
      if(!serviceProbeBusy() && !screenSleeping) {
        if(uiPage==UiPage::Page2) drawPage2Body();
        else if(uiPage==UiPage::Info) drawInfoPage();
      }
    } else {
      servicePending=true; serviceFullPending=serviceFullPending || job.full;
      Serial.println("[HEALTH] canceled or stale result discarded");
    }
    delete serviceProbe; serviceProbe=nullptr;
  }
  if(!online) {
    if(serviceProbe) serviceProbe->cancel=true;
    serviceWasOnline=false; serviceCheckSummary="OFFLINE";
    return;
  }
  if(!serviceWasOnline) {
    serviceWasOnline=true; servicePending=serviceFullPending=true;
    syncOfflineState();
  }
  if(serviceProbe) {
    if(serviceProbeBusy()) serviceProbe->cancel=true;
    return;
  }
  if(!servicePending && serviceLastStarted && millis()-serviceLastStarted<ASR_MONITOR_INTERVAL_MS) return;
  if(serviceProbeBusy()) return; // Deferred, never interrupt audio or photography.
  if(time(nullptr)<1700000000) { serviceCheckSummary="Waiting for time"; return; }
  auto* job=new(std::nothrow) ServiceProbe;
  if(!job) return;
  job->started=millis(); job->full=serviceNeedsFullProbe(serviceFullPending,serviceAiKnown,serviceAiOk);
  job->fingerprint=serviceCheckConfigFingerprint();
  if(serviceCheckFingerprint.length() && job->fingerprint!=serviceCheckFingerprint) {
    job->full=true; serviceAiKnown=false;
  }
  job->asrConfigured=isAsrConfigured();job->customIat=cfg.asrProvider==AsrProvider::CustomIat;
  if(usesIatV2()) {
    job->asrConfigured=job->asrConfigured && job->asr.parse(iatUrl());
    job->asrPath=xfyunAsrPathV2();
  } else if(cfg.asrProvider==AsrProvider::Xfyun) {
    job->asr.parse(String("wss://")+XFYUN_ASR_HOST+XFYUN_ASR_PATH);
    job->asrPath=xfyunAsrPath();
  } else {
    job->asr.parse("wss://"+cfg.asrHost+cfg.asrPath); job->asrPath=cfg.asrPath;
    if(cfg.asrAppKey.length() && cfg.asrAccessKey.length())
      job->asrHeaders="X-Api-App-Key: "+cfg.asrAppKey+"\r\nX-Api-Access-Key: "+cfg.asrAccessKey+"\r\n";
    else job->asrHeaders="X-Api-Key: "+cfg.asrApiKey+"\r\n";
    job->asrHeaders+="X-Api-Resource-Id: "+cfg.asrResourceId+"\r\nX-Api-Request-Id: "+makeId("health")+"\r\n";
  }
  String aiUrl=cfg.llmApiUrl;
  if(aiUrl.startsWith("https://")) aiUrl="wss://"+aiUrl.substring(8);
  else if(aiUrl.startsWith("http://")) aiUrl="ws://"+aiUrl.substring(7);
  job->aiConfigured=job->ai.parse(aiUrl) && cfg.llmApiKey.length(); job->aiKey=cfg.llmApiKey;
  if(xTaskCreatePinnedToCore(serviceProbeWorker,"service-probe",10240,job,1,nullptr,0)!=pdPASS) {
    delete job; return;
  }
  serviceProbe=job; serviceLastStarted=job->started;
  servicePending=serviceFullPending=false; ++serviceStarted;
  serviceCheckSummary="Checking connections";
  Serial.printf("[HEALTH] start sequence=%lu full=%d interval_ms=60000 at_ms=%lu\n",
      (unsigned long)serviceStarted,job->full,(unsigned long)serviceLastStarted);
}

static void serviceMonitorStatus() {
  Serial.printf("[HEALTH] running=%d pending=%d started=%lu completed=%lu last_start_ms=%lu interval_ms=60000 online=%d\n",
      serviceProbe!=nullptr,servicePending,(unsigned long)serviceStarted,(unsigned long)serviceCompleted,
      (unsigned long)serviceLastStarted,WiFi.status()==WL_CONNECTED);
}

// Temporary device-side regression controls. No credentials are erased or saved.
static void serviceWifiPause(bool pause) {
  if(pause && !serviceWifiPaused) {
    cancelServiceProbe();
    serviceSavedReconnect=WiFi.getAutoReconnect();
    WiFi.setAutoReconnect(false); WiFi.disconnect(false,false);
    serviceWifiPaused=true;
    Serial.println("[HEALTH-WIFI] paused; stored credentials retained");
  } else if(!pause && serviceWifiPaused) {
    WiFi.setAutoReconnect(serviceSavedReconnect); WiFi.reconnect();
    serviceWifiPaused=false;
    Serial.println("[HEALTH-WIFI] reconnect requested");
  }
}
