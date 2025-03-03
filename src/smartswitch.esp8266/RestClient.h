#ifndef _RESTCLIENT_H
#define _RESTCLIENT_H

#include <ArduinoJson.h>
#include <WiFiClient.h>

class RestClient {

  public:

    RestClient();

    bool fetch(String url, JsonDocument& doc);
    bool fetch(String url, JsonDocument& doc, String hName, String hValue);

  protected:
    WiFiClient mWiFiClient;
      
};

#endif