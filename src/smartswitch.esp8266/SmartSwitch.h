#ifndef SMARTSWITCH_H
#define SMARTSWITCH_H

#include <ArduinoJson.h>
#include <MBusinoLib.h>
#include <SoftwareSerial.h>
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

//#define DEBUG_ENABLED
#ifdef DEBUG_ENABLED
#define DEBUG(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

#define SERIAL_BAUDRATE 115200
#define WEBSERVER_PORT 80

#define PIN_MBUS_RX 4  // GPIO4 (D2 on NodeMCU)
#define PIN_MBUS_TX 6  // GPIO5 (D1 on NodeMCU)
#define PIN_SSR 5      // GPIO 6 (D3)

#define SONNEN_API_URI "api/v2"
#define SONNEN_API_CONFIGURATIONS "configurations"
#define SONNEN_API_LATEST_DATA "latestdata"
#define SONNEN_API_STATUS "status"
#define SONNEN_INVERTER_LATENCY_MS 5000  // assume latency until battery inverter compensates the load
#define SONNEN_INVERTER_LATENCY_COUNT MAX(1, (SONNEN_INVERTER_LATENCY_MS + (SYSTEM_UPDATE_INTERVAL_MS >> 1)) / SYSTEM_UPDATE_INTERVAL_MS)

#define URL_LOCATION "http://ip-api.com/json/"

#define SOLAR_FORECAST_INTERVAL 12 * 60 * 1000  // every 10min
#define URL_SOLAR_FORECAST "http://api.forecast.solar/estimate/watthours/period/%.4f/%.4f/%d/%d/%.2f?time=seconds&no_sun=0&full=1"
//#define URL_SOLAR_FORECAST "http://192.168.188.20:8080/estimate/watthours/period/%.4f/%.4f/%d/%d/%.2f?time=seconds&no_sun=0&full=1"

#define SYSTEM_UPDATE_INTERVAL_MS 2000  //update intervall millis
#define GRID_PURCHASE_THRESHOLD_W 100

#define CFG_SZ_HOSTNAME 32
#define CFG_SZ_REL_TAG 5
#define CFG_SZ_SONNENHOST 32
#define CFG_SZ_SONNENTOKEN 37
#define CFG_SZ_LOCATION 64
#define CFG_SZ_TZ 32

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define _cs(a) sizeof(((configStruct*)0)->a)
#define setConfigStr(cfg, property, str) \
  if (strlen(str)) { \
    strncpy((cfg).property, (str), MIN(_cs(property), strlen((str)))); \
  } \
  (cfg).property[MIN(_cs(property) - 1, strlen((str)))] = '\0';

typedef struct {

  char hostname[CFG_SZ_HOSTNAME];
  char release_tag[CFG_SZ_REL_TAG];
  char sonnenHostname[CFG_SZ_SONNENHOST];
  char sonnenApiToken[CFG_SZ_SONNENTOKEN];

  uint8_t boiler_T_nom;
  uint8_t boiler_T_max;

  uint16_t loadPower_W;
  uint16_t cap_bat_min_Wh;  // battery min capacity - custom min capacity
  uint16_t gridMin_W;
  float lat;
  float lon;
  float kWp;    // installed PV power
  uint8_t dec;  // PV panel declination (0..90°)
  uint16_t az;  // PV panel Azimuth
  char location[CFG_SZ_LOCATION];
  char tz[CFG_SZ_TZ + 1];
  uint8_t mode;         // 0 - off, 1 - on, 2 - automatic
  bool update_startup;  // release update check on startup
} configStruct;

typedef struct {
  uint32_t ts;
  String msg;
} logEntry;

typedef struct {
  uint32_t ts;       // current system time, taken from battery status
  uint16_t tm_yday;  // day of year
  uint16_t dstOffset;

  bool switchEnabled = false;
  uint8_t boiler_T_cur;

  int inv_max_w = -1;        // inverter power max
  uint8_t usoc;              // 0..100 user state of charge - battery capacity in %
  uint16_t prod_W;           // prodcution (Watt)
  uint16_t cons_W;           // consumption (Watt)
  uint16_t cons_avg_W;       // consumption average (W)
  uint16_t cap_bat_max_Wh;   // max battery capacity (system)
  int gridFeedIn_W;          // current grid feed in - negative is consumption, positive is fedd in
  bool dischargeNotAllowed;  // e.g. true due to battery maintenance
  short charge;              // battery charge state 0 - none, 1 - charge, -1 - discharge

  long pv_forecast_ts;               // last update timestamp in ms since mcu start
  uint32_t pv_forecast_wh_h[49][2];  // pair of timestamp and pv production (Wh/h) for today and tomorrow

  logEntry events[16];  // event buffer
  uint8_t eventIx = 0;

  logEntry error_bs;  // last boiler system error
  logEntry error_bt;  // last battery error
  logEntry error_lc;  // last solar forecast or location error
  uint8_t errorIx = 0;

} systemDataStruct;

#endif