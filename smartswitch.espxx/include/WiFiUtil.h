#pragma once
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#if defined(ESP8266)
//static X509List *ROOT_CA_x509 = new X509List(ROOT_CA);
#endif

inline WiFiClient* newWiFiClient(String url) {

  WiFiClient* wifiClient;

  if (url.startsWith("https")) {
    wifiClient = new WiFiClientSecure();
    ((WiFiClientSecure*)wifiClient)->setInsecure();
    #if defined(ESP8266)
    ((WiFiClientSecure*)wifiClient)->setBufferSizes(1024, 1024);
    #elif defined(ESP32)
    //((WiFiClientSecure*)wifiClient)->setCACert(ROOT_CA);
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
