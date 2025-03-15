#ifndef SMARTSWITCH_H
#define SMARTSWITCH_H

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Ticker.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266httpUpdate.h>

#define DEBUG_ENABLED
#ifdef DEBUG_ENABLED
#define DEBUG(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define STRING(x) #x
#define _cs(a) sizeof(((struct configStruct*)0)->a)
#define setConfigStr(a, b) \
  if (strlen(b)) { \
    strncpy(config.a, b, MIN(_cs(a), strlen(b))); \
    config.a[MIN(_cs(a) - 1, strlen(b))] = '\0'; \
  }

struct configStruct {

  char hostname[CFG_SZ_HOSTNAME + 1];
  char release_tag[CFG_SZ_REL_TAG + 1];
  char sonnenHostname[CFG_SZ_SONNENHOST + 1];
  char sonnenApiToken[CFG_SZ_SONNENTOKEN + 1];

  uint8_t boiler_T_nom;
  uint8_t boiler_T_max;

  uint16_t loadPower_W;
  uint16_t gridMin_W;
  float lat;
  float lon;
  float kWp;    // installed PV power
  uint8_t dec;  // PV panel declination (0..90°)
  uint16_t az;  // PV panel Azimuth
  char location[CFG_SZ_LOCATION + 1];
  char tz[CFG_SZ_TZ + 1];
  uint8_t mode;  // 0 - off, 1 - on, 2 - automatic
  bool update_startup;
};

struct systemDataStruct {
  time_t ts;  // current system time, taken from battery status

  bool switchEnabled = false;

  int inv_max_w = -1;    // inverter power max
  uint8_t usoc;          // 0..100% battery charge
  uint16_t cap;          // battery capacity
  uint16_t prod_w;       // prodcution (Watt)
  uint16_t cons_w;       // consumption (Watt)
  uint16_t cons_avg_w;   // consumption average (Watt)
  uint16_t capacity_wh;  // battery capacity
  int gridFeedIn_W;      // current grid feed in - negative is consumption, positive is fedd in

  long pv_forecast_ts;               // last update timestamp in ms since mcu start
  uint32_t pv_forecast_wh_h[48][2];  //pv production Wh/h for today and tomorrow
};

#endif