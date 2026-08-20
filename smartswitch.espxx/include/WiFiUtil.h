#pragma once
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#ifdef ESP32
#include <LittleFS.h>
#include <mbedtls/x509_crt.h>
#endif

#include "certs.h"

#define CERTFILE "/certs.pem"

#ifdef ESP32

// runtime trust store, replaces ROOT_CA_BUNDLE when uploaded via the web ui.
// setCACert() only keeps the pointer, so the buffer has to stay alive until the
// connection is established - hence a static instead of a local String
inline String &caTrustStore()
{
  static String pem;
  return pem;
}

inline bool hasCaTrustStore()
{
  return LittleFS.exists(CERTFILE);
}

// loads the uploaded trust store, false if none is available or it holds no certificate
inline bool loadCaTrustStore()
{
  caTrustStore() = String();

  File f = LittleFS.open(CERTFILE, "r");
  if (!f)
  {
    return false;
  }
  caTrustStore() = f.readString();
  f.close();

  if (caTrustStore().indexOf(F("-----BEGIN CERTIFICATE-----")) >= 0)
  {
    return true;
  }
  caTrustStore() = String();
  return false;
}

// a chain that mbedtls cannot parse would lock us out of OTA, so verify before storing.
// returns NULL on success, otherwise the reason the store was left untouched
inline const __FlashStringHelper *saveCaTrustStore(const String &pem)
{
  mbedtls_x509_crt chain;
  mbedtls_x509_crt_init(&chain);
  int parsed = mbedtls_x509_crt_parse(&chain, (const unsigned char *)pem.c_str(), pem.length() + 1);
  mbedtls_x509_crt_free(&chain);

  if (parsed != 0)
  {
    return F("no valid pem chain");
  }

  File f = LittleFS.open(CERTFILE, "w");
  if (!f)
  {
    return F("could not write certificate file");
  }
  size_t written = f.print(pem);
  f.close();

  return written == pem.length() ? NULL : F("certificate file truncated");
}

inline void resetCaTrustStore()
{
  LittleFS.remove(CERTFILE);
}

inline const __FlashStringHelper *caTrustStoreState()
{
  return hasCaTrustStore() ? F("custom") : F("firmware");
}

#elif defined(ESP8266) // esp8266 connects with setInsecure(), it has no trust store

inline const __FlashStringHelper *saveCaTrustStore(const String &pem)
{
  return F("tls verification is disabled on this platform");
}

inline void resetCaTrustStore()
{
}

inline const __FlashStringHelper *caTrustStoreState()
{
  return F("unused");
}

#endif

inline WiFiClient *newWiFiClient(String url)
{

  WiFiClient *wifiClient;

  if (url.startsWith("https"))
  {
    wifiClient = new WiFiClientSecure();
#if defined(ESP32)
    ((WiFiClientSecure *)wifiClient)->setCACert(loadCaTrustStore() ? caTrustStore().c_str() : ROOT_CA_BUNDLE);
#elif defined(ESP8266)
    ((WiFiClientSecure *)wifiClient)->setInsecure();
    ((WiFiClientSecure *)wifiClient)->setBufferSizes(1024, 1024);
#endif
  }
  else
  {
    wifiClient = new WiFiClient();
  }
  return wifiClient;
}

inline void releaseWiFiClient(WiFiClient *client)
{
  client->stop();
  delete client;

#ifdef ESP32
  caTrustStore() = String(); // release the buffer, the client using it is gone
#endif
}
