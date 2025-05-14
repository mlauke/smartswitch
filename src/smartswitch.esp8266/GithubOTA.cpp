#include "ESP8266HTTPClient.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#include "GithubOTA.h"

GithubOTA::GithubOTA(const char* host, const char* url, const char* type, const char* filename) {
  update_host = host;
  update_url = url;
  update_type = type;
  update_filename = filename;
}

bool GithubOTA::checkUpdate(const char* current_release_tag) {

  JsonDocument doc;
  RestClient restClient;

  String url = String(update_host) + update_url;

  if (restClient.fetch(url, doc)) {
    if (!doc["tag_name"].is<const char*>()) {
      Serial.println("no release tag found");
      return false;
    }

    release_tag = String(doc["tag_name"]);

    Serial.printf("Found release %s - Current release %s - Prerelease: %d\n", release_tag.c_str(), current_release_tag, doc["prerelease"].as<bool>());

    if (strncmp(release_tag.c_str(), current_release_tag, strlen(current_release_tag)) == 0 || doc["prerelease"].as<bool>()) {
      return false;
    }

    for (auto asset : doc["assets"].as<JsonArray>()) {
      const char* asset_type = asset["content_type"];
      const char* asset_name = asset["name"];
      const char* asset_url = asset["browser_download_url"];

      Serial.printf("asset found: Name: [%s], Type: [%s], URL: [%s]\n", asset_name, asset_type, asset_url);
      Serial.printf("expected: [%s], Type: [%s]\n", update_filename, update_type);

      if (strcmp(asset_type, update_type) == 0 && strcmp(asset_name, update_filename) == 0) {
        download_url = String(asset_url);

        Serial.println("Update URL: " + download_url);

        return true;
      }
    }
  }
  Serial.println("no update asset found");
  return false;
}

String GithubOTA::getUpdateError() {
  return "Update Failed: " + ESPhttpUpdate.getLastErrorString();
}

bool GithubOTA::doUpdate() {

  WiFiClientSecure updateClient;

  if (download_url.length() == 0) {
    Serial.println("No download URL");
    return false;
  }

  updateClient.setInsecure();

  ESPhttpUpdate.setLedPin(LED_BUILTIN, HIGH);
  ESPhttpUpdate.rebootOnUpdate(false);  // restart from outside
  ESPhttpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  t_httpUpdate_return ret = ESPhttpUpdate.update(updateClient, download_url);

  if (ret != HTTP_UPDATE_OK) {
    Serial.println(getUpdateError());
    return false;
  }
  return true;
}