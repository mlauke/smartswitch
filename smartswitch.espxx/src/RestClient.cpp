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
#include "RestClient.h"
#include "debug.h"

RestClient::RestClient() {}

String RestClient::lastError()
{
  return _lastError;
}

int RestClient::lastResponseCode()
{
  return _lastResponseCode;
}

bool RestClient::get(String url, JsonDocument &doc, JsonDocument *filter, String hName, String hValue)
{

  HTTPClient http;

  WiFiClient *wifiClient = newWiFiClient(url);

  DEBUGF("url: '%s' - hdr: '%s' : '%s'\n", url.c_str(), hName.c_str(), hValue.c_str());

  _lastError.clear();

  bool res = http.begin(*wifiClient, url);
  if (res)
  {
    http.setTimeout(REQUEST_TIMEOUT);
    http.setReuse(false);
    http.setFollowRedirects(followRedirects_t::HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setUserAgent(String(F("SmartSwitch - ")));
    if (hName.length() != 0 && hValue.length() != 0)
    {
      http.addHeader(hName, hValue);
    }
    http.addHeader(F("accept"), F("application/json"));

    _lastResponseCode = http.GET();
    if ((res = _lastResponseCode == HTTP_CODE_OK))
    {
      DeserializationError error;
      if (filter == NULL)
      {
        error = deserializeJson(doc, http.getString());
      }
      else
      {
        error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(*filter));
      }
      if (error)
      {
        _lastError.concat(String(F("json error: ")) + url + F(" - ") + error.f_str());
        DEBUGP(_lastError + " " + http.getString());
      }
      res = error == DeserializationError::Ok;
    }
    else
    {
      _lastError.concat(url + F(" code: ") + _lastResponseCode + " " + http.errorToString(_lastResponseCode));
      DEBUGF("%s\n", _lastError.c_str());
    }
  }
  http.end();

  releaseWiFiClient(wifiClient);

  return res;
}
