#ifndef GITHUBOTA_H
#define GITHUBOTA_H

#include "RestClient.h"

#define UPDATE_HOST     "https://api.github.com"
#define UPDATE_URL      "/repos/mlauke/smartswitch/releases/latest"
#define UPDATE_TYPE     "application/gzip"
#define UPDATE_FILENAME "smartswitch.esp8266.ino.bin.gz"
#define CONFIGFILE      "/config.bin"

class GithubOTA
{
public:
  const char * release_tag;

  GithubOTA(const char * , const char * , const char * , const char * );
  bool checkUpdate(const char *);
  bool doUpdate();
  String getUpdateError();

protected:
  const char * update_type;
  const char * update_filename;
  const char * update_host;
  const char * update_url;

  String download_url;
};


#endif //GITHUBOTA_H