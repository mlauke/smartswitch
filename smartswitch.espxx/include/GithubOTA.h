#ifndef GITHUBOTA_H
#define GITHUBOTA_H

#include <pgmspace.h>

#include "RestClient.h"

#define UPDATE_HOST "https://api.github.com"
#define UPDATE_URL "/repos/mlauke/smartswitch/releases/latest"
#define UPDATE_TYPE "application/gzip"

#ifdef ESP32
#define UPDATE_FILENAME "smartswitch.esp32.ino.bin"
#elif defined(ESP8266)
#define UPDATE_FILENAME "smartswitch.esp8266.ino.bin.gz"
#endif

// property names of the github release api - kept in flash, not in RAM
static const char KEY_GH_TAG_NAME[] PROGMEM = "tag_name";
static const char KEY_GH_PRERELEASE[] PROGMEM = "prerelease";
static const char KEY_GH_ASSETS[] PROGMEM = "assets";
static const char KEY_GH_CONTENT_TYPE[] PROGMEM = "content_type";
static const char KEY_GH_NAME[] PROGMEM = "name";
static const char KEY_GH_DOWNLOAD_URL[] PROGMEM = "browser_download_url";

class GithubOTA {
public:
  String release_tag;

  GithubOTA(const char *, const char *, const char *, const char *);
  bool checkUpdate(const char *);
  bool doUpdate(String userAgent, void (*fnOTABegin)(void), void (*fnOTAEnd)(bool));
  String getUpdateStatus();

protected:
  const char *update_type;
  const char *update_filename;
  const char *update_host;
  const char *update_url;

  String download_url;
  String updateStatus;
};


#endif  //GITHUBOTA_H