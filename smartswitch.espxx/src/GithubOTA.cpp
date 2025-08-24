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

#include <GithubOTA.h>
#include <WiFiUtil.h>

GithubOTA::GithubOTA(const char *host, const char *url, const char *type, const char *filename)
{
  update_host = host;
  update_url = url;
  update_type = type;
  update_filename = filename;
}

bool GithubOTA::checkUpdate(const char *current_release_tag)
{

  JsonDocument doc;
  RestClient restClient;

  updateStatus.clear();

  String url = String(update_host) + update_url;
  if (restClient.fetch(url, doc))
  {
    if (!doc["tag_name"].is<const char *>())
    {
      Serial.println("no release tag found");
      return false;
    }

    release_tag = doc["tag_name"].as<String>();

    Serial.printf("Found release %s - Current release %s - Prerelease: %d\n", release_tag.c_str(), current_release_tag, doc["prerelease"].as<bool>());

    if (strncmp(release_tag.c_str(), current_release_tag, strlen(current_release_tag)) == 0 || doc["prerelease"].as<bool>())
    {
      Serial.println("no newer release tag found");
      return false;
    }

    for (auto asset : doc["assets"].as<JsonArray>())
    {
      const char *asset_type = asset["content_type"];
      const char *asset_name = asset["name"];
      const char *asset_url = asset["browser_download_url"];

      Serial.printf("asset found: Name: [%s], Type: [%s], URL: [%s]\n", asset_name, asset_type, asset_url);
      Serial.printf("expected: [%s], Type: [%s]\n", update_filename, update_type);

      if (strcmp(asset_type, update_type) == 0 && strcmp(asset_name, update_filename) == 0)
      {
        download_url = String(asset_url);

        Serial.println("Found Update URL: " + download_url);

        return true;
      }
    }
  }
  else
  {
    updateStatus = restClient.lastError();
    Serial.println(updateStatus);
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
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

String GithubOTA::getUpdateStatus()
{
  return updateStatus;
}

bool GithubOTA::doUpdate(void (*fnOTABegin)(void))
{
  bool success = false;

  updateStatus.clear();

  if (download_url.length() == 0)
  {
    Serial.println("No download URL");
    return false;
  }

  WiFiClient *updateClient = newWiFiClient(download_url);

#ifdef ESP32

  HTTPClient http;

  http.begin(*updateClient, download_url);
  http.setConnectTimeout(3000);
  http.setTimeout(5000);
  http.setFollowRedirects(followRedirects_t::HTTPC_STRICT_FOLLOW_REDIRECTS);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK)
  {
    int contentLength = http.getSize();

    Update.clearError();
    Update.onProgress(onOTAProgress);

    bool canBegin = Update.begin(contentLength, U_FLASH, BUILTIN_LED);
    if (canBegin)
    {
      fnOTABegin();

      WiFiClient *stream = http.getStreamPtr();
      if (!stream || !stream->available())
      {
        Serial.println("ERROR: Stream not available");
        return success;
      }
      Serial.println("Begin OTA...");
      size_t written = Update.writeStream(*stream);

      if (written == contentLength)
      {
        Serial.println("Written : " + String(written) + " successfully");
      }
      else
      {
        Update.abort();
        Serial.println("Written only : " + String(written) + "/" + String(contentLength) + "!");
      }
      if (Update.end())
      {
        Serial.println("OTA done!");
        if (Update.isFinished())
        {
          Serial.println("Update successfully completed.");
          success = true;
        }
        else
        {
          updateStatus = String("Update not finished? Something went wrong!");
        }
      }
      else
      {
        updateStatus = String("Error Occurred. Error #: ") + Update.getError();
      }
    }
    else
    {
      updateStatus = String("Not enough space to begin OTA");
    }
  }
  else
  {
    updateStatus = String("Failed to download firmware. HTTP code: ") + String(httpCode);
  }
  http.end();

#elif defined(ESP8266)

  ESPhttpUpdate.onStart(fnOTABegin);
  ESPhttpUpdate.onProgress(onOTAProgress);
  ESPhttpUpdate.setLedPin(LED_BUILTIN, HIGH);
  ESPhttpUpdate.setClientTimeout(10000);
  ESPhttpUpdate.rebootOnUpdate(false); // restart from outside
  ESPhttpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  success = ESPhttpUpdate.update(*updateClient, download_url) == HTTP_UPDATE_OK;
  if (!success)
  {
    updateStatus = String("Update Failed: ") + ESPhttpUpdate.getLastErrorString();
  }
#endif

  delete updateClient;

  if (!success)
  {
    Serial.println(updateStatus);
  }

  return success;
}