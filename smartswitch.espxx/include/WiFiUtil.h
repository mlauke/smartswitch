#pragma once
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "certs.h"

inline WiFiClient *newWiFiClient(String url)
{

  WiFiClient *wifiClient;

  if (url.startsWith("https"))
  {
    wifiClient = new WiFiClientSecure();
#if defined(ESP32)
    ((WiFiClientSecure *)wifiClient)->setCACert(ROOT_CA_BUNDLE);
#elif defined(ESP8266)
    ((WiFiClientSecure *)wifiClient)->setInsecure();
    ((WiFiClientSecure *)wifiClient)->setBufferSizes(1024, 1024);
#endif
  }
  else
  {
    wifiClient = new WiFiClient();
  }
  return wifiClient;
}

inline void releaseWiFiClient(WiFiClient *client)
{
  client->stop();
  delete client;
}
