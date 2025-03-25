#ifndef _RESTCLIENT_H
#define _RESTCLIENT_H

#include "SmartSwitch.h"

#define REQUEST_TIMEOUT 8000

class RestClient {

public:

  RestClient();

  String lastError();
  bool fetch(String url, JsonDocument& doc, String hName = "", String hValue = "");

protected:
  WiFiClientSecure mWiFiClientSecure;
  WiFiClient mWiFiClient;

private:
  String _lastError;

  WiFiClient& getClient(String url) {

    if (url.startsWith("https")) {
      mWiFiClientSecure.setInsecure();
      return (WiFiClient&)mWiFiClientSecure;
    }
    return mWiFiClient;
  }
};

#endif