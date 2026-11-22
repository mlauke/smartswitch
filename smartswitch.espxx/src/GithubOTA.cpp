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

#include "GithubOTA.h"

GithubOTA::GithubOTA() : GitOTA(GITHUB_UPDATE_URL, UPDATE_FILENAME)
{
}

void GithubOTA::addAssetFilter(JsonObject asset)
{
  GitOTA::addAssetFilter(asset);
  asset[FPSTR(KEY_GH_CONTENT_TYPE)] = true;
}

// github labels each asset with a content type, the firmware has to be uploaded as gzip
bool GithubOTA::isUpdateAsset(JsonObjectConst asset)
{
  const char *asset_type = asset[FPSTR(KEY_GH_CONTENT_TYPE)];
  return GitOTA::isUpdateAsset(asset) && asset_type != NULL && strcmp_P(asset_type, GITHUB_UPDATE_TYPE) == 0;
}
