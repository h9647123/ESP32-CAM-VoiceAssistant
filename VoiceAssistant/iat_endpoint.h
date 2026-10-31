#pragma once
#include <Arduino.h>

// Unsigned endpoint only. Host authority is shared by HMAC and HTTP Upgrade.
struct IatEndpoint {
  bool tls = true;
  String host, authority, path;
  uint16_t port = 443;
  bool parse(const String& url) {
    *this = IatEndpoint();
    const bool secure = url.startsWith("wss://");
    if (!secure && !url.startsWith("ws://")) return false;
    if (url.length() > 300) return false;
    for (size_t i=0;i<url.length();++i)
      if (uint8_t(url[i]) <= 32 || url[i]=='?' || url[i]=='#' || url[i]=='@' || url[i]=='\\') return false;
    tls = secure; port = tls ? 443 : 80;
    const unsigned start = tls ? 6 : 5;
    const int slash = url.indexOf('/', start);
    authority = slash < 0 ? url.substring(start) : url.substring(start, slash);
    path = slash < 0 ? String("/") : url.substring(slash);
    const int colon = authority.indexOf(':');
    host = colon < 0 ? authority : authority.substring(0, colon);
    if (!host.length()) return false;
    for (size_t i=0;i<host.length();++i) {
      const char c=host[i];
      if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='-')) return false;
    }
    if (colon >= 0) {
      const String number=authority.substring(colon+1);
      if (!number.length() || number.length()>5) return false;
      for(size_t i=0;i<number.length();++i) if(number[i]<'0'||number[i]>'9') return false;
      const long value=number.toInt();
      if (value<1||value>65535) return false;
      port=uint16_t(value);
    }
    return true;
  }
};
