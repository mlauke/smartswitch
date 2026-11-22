#ifndef CODEBERGOTA_H
#define CODEBERGOTA_H

#include "GitOTA.h"

#define CODEBERG_UPDATE_URL "https://codeberg.org/api/v1/repos/mlauke/smartswitch/releases/latest"

class CodebergOTA : public GitOTA {
public:
  CodebergOTA();
};

#endif // CODEBERGOTA_H
