#ifndef GITOTA_H
#define GITOTA_H

#include <pgmspace.h>

#include "RestClient.h"

#if defined(ESP32) && defined(NET_ETHERNET) // a wifi image would leave an ethernet device unreachable
#define UPDATE_FILENAME "smartswitch.esp32_eth.ino.bin"
#elif defined(ESP32)
#define UPDATE_FILENAME "smartswitch.esp32.ino.bin"
#elif defined(ESP8266)
#define UPDATE_FILENAME "smartswitch.esp8266.ino.bin.gz"
#endif

// property names shared by the github and the forgejo release api - kept in flash, not in RAM
static const char KEY_GIT_TAG_NAME[] PROGMEM = "tag_name";
static const char KEY_GIT_PRERELEASE[] PROGMEM = "prerelease";
static const char KEY_GIT_ASSETS[] PROGMEM = "assets";
static const char KEY_GIT_NAME[] PROGMEM = "name";
static const char KEY_GIT_DOWNLOAD_URL[] PROGMEM = "browser_download_url";

// fetches the latest release of a git provider and flashes its firmware asset.
// the providers only differ in how an asset is recognized, see GithubOTA / CodebergOTA
class GitOTA {
public:
  String release_tag;

  GitOTA(const char *url, const char *filename);
  virtual ~GitOTA() = default;
  bool checkUpdate(const char *current_release_tag);
  bool doUpdate(String userAgent, void (*fnOTABegin)(void), void (*fnOTAEnd)(bool));
  String getUpdateStatus();

protected:
  const char *update_url;
  const char *update_filename;

  virtual void addAssetFilter(JsonObject asset);
  virtual bool isUpdateAsset(JsonObjectConst asset);

private:
  String download_url;
  String updateStatus;

  bool findUpdateAsset(JsonArrayConst assets);
};

#endif // GITOTA_H
