#ifndef GITHUBOTA_H
#define GITHUBOTA_H

#include "RestClient.h"

#define UPDATE_HOST "https://api.github.com"
#define UPDATE_URL "/repos/mlauke/smartswitch/releases/latest"
#define UPDATE_TYPE "application/gzip"

#ifdef ESP32
#define UPDATE_FILENAME "smartswitch.esp32.ino.bin"
#elif defined(ESP8266)
#define UPDATE_FILENAME "smartswitch.esp8266.ino.bin.gz"
#endif

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