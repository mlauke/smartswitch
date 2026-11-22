#ifndef GITHUBOTA_H
#define GITHUBOTA_H

#include "GitOTA.h"

#define GITHUB_UPDATE_URL "https://api.github.com/repos/mlauke/smartswitch/releases/latest"

static const char GITHUB_UPDATE_TYPE[] PROGMEM = "application/gzip";
static const char KEY_GH_CONTENT_TYPE[] PROGMEM = "content_type";

class GithubOTA : public GitOTA {
public:
  GithubOTA();

protected:
  void addAssetFilter(JsonObject asset) override;
  bool isUpdateAsset(JsonObjectConst asset) override;
};

#endif // GITHUBOTA_H
