#ifndef _WIFI_UTIL_H
#define _WIFI_UTIL_H

#include <WiFiClient.h>
#include <WiFiClientSecure.h>

inline WiFiClient* newWiFiClient(String url) {

  WiFiClient* wifiClient;

  if (url.startsWith("https")) {
    wifiClient = new WiFiClientSecure();
    ((WiFiClientSecure*)wifiClient)->setInsecure();
  } else {
    wifiClient = new WiFiClient();
  }
  return wifiClient;
}

#endif