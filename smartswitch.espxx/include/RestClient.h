#ifndef _RESTCLIENT_H
#define _RESTCLIENT_H

#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "WiFiUtil.h"

#ifdef ESP32
  #include <HTTPClient.h>
#elif defined(ESP8266)
  #include <ESP8266HTTPClient.h>
#endif

#include "debug.h"

#define REQUEST_TIMEOUT 8000

class RestClient {

public:

  RestClient();

  String lastError();
  int lastResponseCode();
  bool fetch(String url, JsonDocument& doc, String hName = "", String hValue = "");

private:
  String _lastError;
  int _lastResponseCode;
};

#endif