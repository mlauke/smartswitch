#include "RestClient.h"

RestClient::RestClient() {
}

bool RestClient::fetch(String url, JsonDocument& doc, String hName, String hValue) {

  HTTPClient http;

  DEBUG("url: '%s' - hdr: '%s' : '%s'\n", url.c_str(), hName.c_str(), hValue.c_str());

  bool res = http.begin(getClient(url), url);
  if (res) {

    http.setTimeout(5000);
    http.setUserAgent("SmartSwitch");
    if (hName.length() != 0 && hValue.length() != 0) {
      http.addHeader(hName, hValue);
    }
    http.addHeader("accept", "application/json", true, true);

    int httpResponseCode = http.GET();
    if ((res = httpResponseCode == 200)) {
      DeserializationError error = deserializeJson(doc, http.getString());
      if (error) {
        Serial.printf("json error: %s - %s - response :\n", url.c_str(), error.c_str());
        Serial.println(http.getString());
      }
      res = error == DeserializationError::Ok;
    } else {
      Serial.printf("WARN '%s' code (%d) %s\n", url.c_str(), httpResponseCode, http.errorToString(httpResponseCode).c_str());
    }
  }
  http.end();

  return res;
}
