#include "RestClient.h"
#include <ESP8266HTTPClient.h>

RestClient::RestClient() {
}

bool RestClient::fetch(String url, JsonDocument& doc) {
  return fetch(url, doc, "", "");
}

bool RestClient::fetch(String url, JsonDocument& doc, String hName, String hValue) {

  bool res = false;

  HTTPClient http;

  http.begin(mWiFiClient, url);

  if(hName.length() != 0 && hValue.length() != 0){
    http.addHeader(hName, hValue);
  }

  int httpResponseCode = http.GET();
  Serial.printf("url %s %d\n", url.c_str(), httpResponseCode);
  if (httpResponseCode == 200) {
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (error) {
      Serial.printf("parsing json error: %s %s\n", url, error.c_str());
    }
    res = error == DeserializationError::Ok;
  } else {
    Serial.printf("no data %s - code: %d %s\n", url, httpResponseCode, http.getString());
  }
  http.end();  // Free resources

  return res;
}
