#pragma once
#include <WiFiUdp.h>
#include <atomic>
#include "iat_dns_packet.h"

static bool iatQueryAlternateDns(const String& host,const IPAddress& resolver,
    IPAddress& address,std::atomic<bool>* cancel) {
  uint8_t request[272],response[512];
  const uint16_t id=uint16_t(esp_random());
  const size_t bytes=iatDnsQuery(host.c_str(),id,request,sizeof(request));
  if(!bytes)return false;
  WiFiUDP udp;if(!udp.begin(0))return false;
  if(!udp.beginPacket(resolver,53) || udp.write(request,bytes)!=bytes || !udp.endPacket()) {udp.stop();return false;}
  const uint32_t started=millis();bool ok=false;
  while(millis()-started<1800 && WiFi.status()==WL_CONNECTED && !(cancel && cancel->load())) {
    const int count=udp.parsePacket();
    if(count>0) {
      const bool source=udp.remoteIP()==resolver && udp.remotePort()==53;
      const int received=udp.read(response,sizeof(response));uint8_t ip[4];
      if(source && count<=int(sizeof(response)) && received==count && iatDnsAnswer(response,count,host.c_str(),id,ip)) {
        address=IPAddress(ip[0],ip[1],ip[2],ip[3]);ok=true;break;
      }
    }
    delay(5);
  }
  udp.stop();return ok;
}

// Retry a failed public CUSTOM IAT connection with independently resolved
// addresses. Never change global DNS/NVS, pin a node IP, or weaken TLS/SNI.
static bool iatConnectWithDnsFallback(NetworkClient& client,WiFiClientSecure& tls,
    const IatEndpoint& endpoint,uint32_t timeoutMs,bool allowFallback,std::atomic<bool>* cancel=nullptr) {
  if(client.connect(endpoint.host.c_str(),endpoint.port,timeoutMs))return !(cancel && cancel->load());
  if(endpoint.tls) {
    char error[128]={};const int code=tls.lastError(error,sizeof(error));
    Serial.printf("[IAT-TLS] host=%s code=%d detail=%s\n",endpoint.host.c_str(),code,error);
  }
  IPAddress literal;
  if(!allowFallback || !endpoint.tls || literal.fromString(endpoint.host) ||
      endpoint.host.indexOf('.')<0 || endpoint.host.endsWith(".local") || endpoint.host.endsWith(".lan") ||
      endpoint.host.endsWith(".internal") || (cancel && cancel->load()))return false;
  const IPAddress resolvers[]={IPAddress(223,5,5,5),IPAddress(119,29,29,29)};
  for(const auto& resolver:resolvers) {
    IPAddress address;
    if(!iatQueryAlternateDns(endpoint.host,resolver,address,cancel))continue;
    if(cancel && cancel->load())return false;
    client.stop();
    // Arduino ESP32 stop_ssl_socket() clears bundle_attach_cb. Reinstall trust
    // after every stop/failure before reusing the secure client for a retry.
    if(!camConfigureTls(tls))return false;
    Serial.printf("[IAT-DNS] retry host=%s resolver=%s address=%s port=%u sni_preserved=1\n",
        endpoint.host.c_str(),resolver.toString().c_str(),address.toString().c_str(),unsigned(endpoint.port));
    // The public SDK overload passes the hostname to both SNI and certificate
    // validation. camConfigureTls already installed the CA bundle/optional PEM.
    if(tls.connect(address,endpoint.port,endpoint.host.c_str(),CAM_TLS_CA_PEM,nullptr,nullptr))return !(cancel && cancel->load());
    char error[128]={};const int code=tls.lastError(error,sizeof(error));
    Serial.printf("[IAT-TLS] retry host=%s code=%d detail=%s\n",endpoint.host.c_str(),code,error);
  }
  return false;
}
