// MIT License
//
// Copyright (c) 2024 Marko Lauke
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "ESP8266HTTPClient.h"
#include "RestClient.h"

RestClient::RestClient() {
}

String RestClient::lastError() {
  return _lastError;
}

int RestClient::lastResponseCode() {
  return _lastResponseCode;
}

bool RestClient::fetch(String url, JsonDocument& doc, String hName, String hValue) {

  HTTPClient http;

  DEBUG("url: '%s' - hdr: '%s' : '%s'\n", url.c_str(), hName.c_str(), hValue.c_str());

  bool res = http.begin(getClient(url), url);
  if (res) {

    http.setTimeout(REQUEST_TIMEOUT);
    http.setReuse(false);
    http.setUserAgent("SmartSwitch");
    if (hName.length() != 0 && hValue.length() != 0) {
      http.addHeader(hName, hValue);
    }
    http.addHeader("accept", "application/json", true, true);

    _lastResponseCode = http.GET();
    if ((res = _lastResponseCode == 200)) {
      DeserializationError error = deserializeJson(doc, http.getString());
      if (error) {
        _lastError = "json error: " + url + " - " + String(error.c_str());
        Serial.println(_lastError);
        Serial.println(http.getString());
      }
      res = error == DeserializationError::Ok;
    } else {
      _lastError = url + " code: " + _lastResponseCode + " " + http.errorToString(_lastResponseCode);
      Serial.println(_lastError);
    }
  }
  http.end();

  return res;
}
