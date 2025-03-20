#include "RestClient.h"

RestClient::RestClient() {
}

String RestClient::lastError() {
  return _lastError;
}

bool RestClient::fetch(String url, JsonDocument& doc, String hName, String hValue) {

  HTTPClient http;

  DEBUG("url: '%s' - hdr: '%s' : '%s'\n", url.c_str(), hName.c_str(), hValue.c_str());

  bool res = http.begin(getClient(url), url);
  if (res) {

    http.setTimeout(8000);
    http.setReuse(false);
    http.setUserAgent("SmartSwitch");
    if (hName.length() != 0 && hValue.length() != 0) {
      http.addHeader(hName, hValue);
    }
    http.addHeader("accept", "application/json", true, true);

    int httpResponseCode = http.GET();
    if ((res = httpResponseCode == 200)) {
      DeserializationError error = deserializeJson(doc, http.getString());
      if (error) {
        _lastError = "json error: " + url + " - " + String(error.c_str());
        Serial.println(_lastError);
        Serial.println(http.getString());
      }
      res = error == DeserializationError::Ok;
    } else {
      _lastError = "WARN " + url + " code (" + httpResponseCode + ") " + http.errorToString(httpResponseCode);
      Serial.println(_lastError);
    }
  }
  http.end();

  return res;
}
