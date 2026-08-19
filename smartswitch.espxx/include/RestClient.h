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

#define REQUEST_TIMEOUT 8000

struct JsonField; // caller supplied set of expected response fields, see JsonFields.h

class RestClient {

public:

  RestClient();

  String lastError();
  int lastResponseCode();
  bool get(String url, JsonDocument& doc, JsonDocument* filter,
           const JsonField* expectedFields = NULL, uint8_t expectedFieldCount = 0,
           String hName = "", String hValue = "");

private:
  String _lastError;
  int _lastResponseCode;
};

#endif