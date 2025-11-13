#pragma once
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

inline WiFiClient* newWiFiClient(String url) {

  WiFiClient* wifiClient;

  if (url.startsWith("https")) {
    wifiClient = new WiFiClientSecure();
    ((WiFiClientSecure*)wifiClient)->setInsecure();
    #if defined(ESP8266)
    ((WiFiClientSecure*)wifiClient)->setBufferSizes(1024, 1024);
    #endif
  } else {
    wifiClient = new WiFiClient();
  }
  return wifiClient;
}

inline void releaseWiFiClient(WiFiClient *client) {
  client->stop();
  delete client;
}
