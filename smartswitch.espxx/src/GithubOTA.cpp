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

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

#ifdef ESP32
#include <Update.h>
#elif defined(ESP8266)
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#endif

#include "GithubOTA.h"
#include "WiFiUtil.h"
#include "debug.h"

GithubOTA::GithubOTA(const char *host, const char *url, const char *type, const char *filename)
{
  update_host = host;
  update_url = url;
  update_type = type;
  update_filename = filename;
}

bool GithubOTA::checkUpdate(const char *current_release_tag)
{
  JsonDocument json;
  JsonDocument filter;
  RestClient restClient;

  updateStatus.clear();

  filter[FPSTR(KEY_GH_TAG_NAME)] = true;
  filter[FPSTR(KEY_GH_PRERELEASE)] = true;
  filter[FPSTR(KEY_GH_ASSETS)][0][FPSTR(KEY_GH_CONTENT_TYPE)] = true;
  filter[FPSTR(KEY_GH_ASSETS)][0][FPSTR(KEY_GH_NAME)] = true;
  filter[FPSTR(KEY_GH_ASSETS)][0][FPSTR(KEY_GH_DOWNLOAD_URL)] = true;

  String url = String(update_host) + update_url;
  if (restClient.get(url, json, &filter))
  {
    if (!json[FPSTR(KEY_GH_TAG_NAME)].is<const char *>())
    {
      updateStatus = F("No release tag found");
      return false;
    }

    release_tag = json[FPSTR(KEY_GH_TAG_NAME)].as<String>();

    DEBUGF("Found release %s - Current release %s - Prerelease: %d\n", release_tag.c_str(), current_release_tag, json[FPSTR(KEY_GH_PRERELEASE)].as<bool>());

    if (strncmp(release_tag.c_str(), current_release_tag, strlen(current_release_tag)) == 0 || json[FPSTR(KEY_GH_PRERELEASE)].as<bool>())
    {
      updateStatus = F("No new release tag found");
      return false;
    }

    for (auto asset : json[FPSTR(KEY_GH_ASSETS)].as<JsonArray>())
    {
      const char *asset_type = asset[FPSTR(KEY_GH_CONTENT_TYPE)];
      const char *asset_name = asset[FPSTR(KEY_GH_NAME)];
      const char *asset_url = asset[FPSTR(KEY_GH_DOWNLOAD_URL)];

      DEBUGF("asset found: Name: [%s], Type: [%s], URL: [%s]\n", asset_name, asset_type, asset_url);
      DEBUGF("expected: [%s], Type: [%s]\n", update_filename, update_type);

      if (strcmp(asset_type, update_type) == 0 && strcmp(asset_name, update_filename) == 0)
      {
        download_url = String(asset_url);

        DEBUGF("Found Update URL: %s\n", download_url.c_str());

        return true;
      }
    }
    updateStatus = String(F("No Update URL found for new release tag ")) + release_tag;
  }
  else
  {
    updateStatus = restClient.lastError();
  }
  return false;
}

void onOTAProgress(size_t current, size_t final)
{
  static long ota_progress_millis = 0;
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000)
  {
    ota_progress_millis = millis();
    DEBUGF("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

String GithubOTA::getUpdateStatus()
{
  return updateStatus;
}

bool GithubOTA::doUpdate(String userAgent, void (*fnOTABegin)(void), void (*fnOTAEnd)(bool))
{
  bool success = false;

  updateStatus.clear();

  if (download_url.length() == 0)
  {
    DEBUGLN("No download URL");
    return false;
  }
  fnOTABegin();

  DEBUGF("Heap: %uk\n", ESP.getFreeHeap());

  Update.clearError();
  Update.onProgress(onOTAProgress);

  HTTPClient http;
#if defined(ESP32)
  http.setConnectTimeout(5000);
#endif
  http.setReuse(false);
  http.setUserAgent(userAgent);
  http.setTimeout(5000);
  http.setFollowRedirects(followRedirects_t::HTTPC_DISABLE_FOLLOW_REDIRECTS);

  WiFiClient *updateClient = newWiFiClient(download_url);
  http.begin(*updateClient, download_url);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_TEMPORARY_REDIRECT || httpCode == HTTP_CODE_PERMANENT_REDIRECT || httpCode == HTTP_CODE_FOUND)
  {
    String newUrl = http.getLocation();
    http.end();
    releaseWiFiClient(updateClient);

#if defined(ESP8266) // workaround, fetch firmware via ssl seems too havy for esp8266
    newUrl.replace(F("https://"), F("http://"));
#endif
    DEBUGF("redirect asset url %d %s\n", httpCode, newUrl.c_str());
    updateClient = newWiFiClient(newUrl);
    http.begin(*updateClient, newUrl);
    httpCode = http.GET();

    DEBUGF("get asset url %d\n", httpCode);
  }
  if (httpCode == HTTP_CODE_OK)
  {
    int contentLength = http.getSize();

    bool canBegin = Update.begin(contentLength, U_FLASH, LED_BUILTIN);
    if (canBegin)
    {
      WiFiClient *stream = http.getStreamPtr();
      if (stream == nullptr || !stream->available())
      {
        DEBUGF("ERROR: Stream not available %u %p\n", contentLength, stream);
      }
      else
      {
        DEBUGLN("Begin OTA...");
        size_t written = Update.writeStream(*stream);

        if (written == (size_t)contentLength)
        {
          DEBUGF("Written : %d successfully\n", written);
        }
        else
        {
#if defined(ESP32)
          Update.abort();
#endif
          DEBUGF("Written only : %u/%u !\n", written, contentLength);
        }
        if (Update.end())
        {
          DEBUGLN("OTA done!");
          if (Update.isFinished())
          {
            DEBUGLN("Update successfully completed.");
            success = true;
          }
          else
          {
            updateStatus = F("Update not finished? Something went wrong!");
          }
        }
        else
        {
          updateStatus = String(F("Error Occurred. Error #: ")) + Update.getError();
        }
      }
    }
    else
    {
      updateStatus = F("Not enough space to begin OTA");
    }
  }
  else
  {
    updateStatus = String(F("Failed to download firmware. HTTP code: ")) + http.errorToString(httpCode);
  }
  http.end();
  releaseWiFiClient(updateClient);

  if (!success)
  {
    DEBUGP(updateStatus);
  }
  fnOTAEnd(success);

  return success;
}