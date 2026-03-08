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
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Ticker.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>

// echo -e "const char index_html[] PROGMEM = { $(gzip -9 -c nginx/index.html | hexdump -v -e '1/1 "0x%02X, "') };" > src/smartswitch.esp8266/index_html.h && \
// echo -e "const char app_js[] PROGMEM = { $(gzip -9 -c nginx/app.js | hexdump -v -e '1/1 "0x%02X, "') };" > src/smartswitch.esp8266/app_js.h && \
// echo "const char app_css[] PROGMEM = { $(gzip -9 -c nginx/app.css | hexdump -v -e '1/1 "0x%02X, "') };" > src/smartswitch.esp8266/app_css.h

#include "GithubOTA.h"
#include "RestClient.h"
#include "LPB.h"
#include "Util.h"

#include "app_js.h"
#include "app_css.h"
#include "app_icon.h"
#include "index_html.h"

#ifdef ESP32
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_task_wdt.h>

#elif ESP8266
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266httpUpdate.h>

#endif

#include "SmartSwitch.h"
#include "Logic.h"
#include "debug.h"

#ifdef ESP32
static WebServer server(WEBSERVER_PORT);
#elif ESP8266
static ESP8266WebServer server(WEBSERVER_PORT);
#endif

volatile bool doUpdateFlag = false;

LPB *lpb;
WiFiManager wifiManager;
Ticker timer;
SystemState systemState;
SystemConfig config;
bool saveConfigFile = false;
bool calibrateLoad = false;

void saveConfigCallback()
{
  saveConfigFile = true;
}

void start()
{
  server.begin(); // Actually start the server
  DEBUGLN("HTTP server started");

  uint8_t retries = LPB_RETRIES;
  if (!lpb->enableInterface(retries))
  {
    putBoilerError(String("LPB: No device found after ") + retries + " retries!");
  }
#ifdef ESP32
  // esp_task_wdt_init({ 20000, 0, true });
#elif ESP8266
  ESP.wdtEnable(20000);
#endif
}

void setup()
{
  configDefaults();
  systemDefaults();

  Serial.begin(SERIAL_BAUDRATE);

  lpb = new LPB(PIN_LPB_RX, PIN_LPB_TX, LPB_ADDR_SELF, LPB_ADDR_DEST);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_SSR, OUTPUT);

  updateSwitch(false);

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  WiFiManagerParameter custom_hostname("hostname", "Hostname", config.hostname, CFG_SZ_HOSTNAME);
  wifiManager.addParameter(&custom_hostname);
  wifiManager.setConfigPortalTimeout(10);
  if (!wifiManager.autoConnect("SmartSwitchAP"))
  {
    DEBUGLN("Failed to connect, restarting...");
    restart();
  }

  DEBUGLN("Mounting FS...");
  if (!LittleFS.begin())
  {
    DEBUGLN("Failed to mount FS. Attempting to format...");
    LittleFS.format();
    DEBUGLN(" done.");
  }
  if (!loadConfig())
  {
    DEBUGLN("Error loading config");
    saveConfig();
  }

  if (saveConfigFile)
  {
    String lcHostname = String(custom_hostname.getValue());
    lcHostname.toLowerCase();
    setConfigStr(config, hostname, lcHostname.c_str());
    if (!saveConfig())
    {
      DEBUGLN("Error saving config");
    }
  }

  WiFi.setHostname(config.hostname);
  MDNS.begin(config.hostname);

  timer.attach_ms(SYSTEM_UPDATE_INTERVAL_MS, timerCallback);

  if (config.update_startup)
  {
    handleGithubUpdate();
  }

  updateLocation();

  server.on("/", handleRoot);
  server.on("/favicon.ico", handleFavicon);
  server.on("/app.js", handleAppJs);
  server.on("/app.css", handleAppCss);
  server.on("/api/status", handleStatus);
  server.on("/api/data", handleData);
  server.on("/api/update", handleAPI);
  server.onNotFound(handleNotFound);
#if ESP8266
  server.keepAlive(false);
#endif
  start();
}

void timerCallback()
{
  doUpdateFlag = true;
}

void onOTABegin()
{
  server.close();

  if (timer.active())
    timer.detach();
  lpb->disableInterface();
#if ESP8266
  ESP.wdtDisable();
#endif
  updateSwitch(false);
}

void onOTAEnd(bool success)
{
  if (success)
  {
    DEBUGLN("OTA update finished successfully!");
    return;
  }
  DEBUGLN("There was an error during OTA update!");
  start(); // restart server again
}

void systemDefaults()
{
  systemState.start_ts = 0;
  systemState.pv_forecast_ts = 0;
  memset(systemState.pv_forecast_ts_wh, 0, sizeof(systemState.pv_forecast_ts_wh));
  systemState.eventIx = 0;
  systemState.inv_max_w = -1;
  systemState.utc_offset = -1;
  systemState.switchEnabled = false;
  systemState.skipUpdateCountSysten = 0;
}

void configDefaults()
{ // init config struct with default values

#ifdef ESP32
  setConfigStr(config, hostname, WiFi.getHostname());
#elif ESP8266
  setConfigStr(config, hostname, WiFi.hostname().c_str());
#endif
  setConfigStr(config, release_tag, RELEASE_TAG);
  config.sonnenHostname[0] = '\0';
  config.sonnenApiToken[0] = '\0';
  config.lon = 0.0;
  config.lat = 0.0;
  config.az = 0;    // default azimuth to 0 (south)
  config.dec = 30;  // default panel declination
  config.kWp = 1.0; // default kWp
  setConfigStr(config, tz, "Europe/Berlin");
  config.location[0] = '\0';
  config.loadPower_W = 1000; // initial assume 1kW
  config.cap_bat_min_Wh = 500;
  config.gridMin_W = GRID_PURCHASE_THRESHOLD_W;
  config.mode = 0; // initial set to off
  config.update_startup = false;

  config.version = 0;
}

static bool isDevMode()
{
  return strstr(config.hostname, "-dev") != NULL;
}

bool updateSolarForecast()
{
  static bool lastResult = false;

  long ms = millis();
  if (config.lat != 0.0 && config.lon != 0.0 && (systemState.pv_forecast_ts == 0 || (systemState.pv_forecast_ts + SOLAR_FORECAST_INTERVAL_MS) < ms))
  {

    RestClient restClient;
    JsonDocument json;

    String solarUrl = isDevMode() ? URL_SOLAR_FORECAST_DEV : URL_SOLAR_FORECAST;
    char url[160];
    // solar forecast api requires azimuth with -180 north, -90 east, 0 south, 90 west
    snprintf(url, sizeof(url), solarUrl.c_str(), config.lat, config.lon, config.dec, config.az - 180, config.kWp);

    if ((lastResult = restClient.fetch(String(url), json, NULL)))
    {
      serializeJsonPretty(json, Serial);
      uint8_t i = 0;
      for (JsonPair entry : json[F("result")].as<JsonObject>())
      {
        uint32_t ts = strtoul(entry.key().c_str(), NULL, 10);
        uint32_t wh = entry.value().as<uint32_t>();
        if (i < sizeof(systemState.pv_forecast_ts_wh) / sizeof(systemState.pv_forecast_ts_wh[0]))
        {
          systemState.pv_forecast_ts_wh[i][0] = ts;
          systemState.pv_forecast_ts_wh[i][1] = wh;
          i++;
        }
        else
        {
          putEvent(String(F("WARN: overflow ")) + i);
          break;
        }
      }
      setConfigStr(config, location, json[F("message")][F("info")][F("place")].as<const char *>());

      clearLocationError();
    }
    else
    {
      putLocationError(String(F("WARN solar forecast ")) + restClient.lastError());
    }
    systemState.pv_forecast_ts = restClient.lastResponseCode() > 0 ? ms : 0;
  }
  return lastResult;
}

void updateLocation()
{

  if (config.lat == 0.0 && config.lon == 0.0)
  {

    RestClient restClient;
    JsonDocument json;

    if ((saveConfigFile = restClient.fetch(URL_LOCATION, json, NULL)))
    {
      config.lon = json[F("lon")].as<double>();
      config.lat = json[F("lat")].as<double>();
      snprintf(config.location, CFG_SZ_LOCATION, "%s %s", json[F("zip")].as<const char *>(), json[F("city")].as<const char *>());
      setConfigStr(config, tz, json[F("timezone")]);
      DEBUGF("Location: %f/%f - tz: %s loc: %s\n", config.lon, config.lat, config.tz, config.location);
    }
  }
}

void cacheControlHeader(bool cache)
{
  if (cache)
    server.sendHeader(F("cache-control"), F("max-age=31536000, must-revalidate"));
  else
    server.sendHeader(F("cache-control"), F("no-cache"));
}

void commonHeader()
{
  server.sendHeader(F("content-encoding"), F("gzip"));
}

void handleFavicon()
{
  cacheControlHeader(true);
  server.send_P(200, "image/x-icon", app_icon, sizeof(app_icon));
}

void handleAppJs()
{
  commonHeader();
  cacheControlHeader(true);
  server.send_P(200, "text/javascript", app_js, sizeof(app_js));
}

void handleAppCss()
{
  commonHeader();
  cacheControlHeader(true);
  server.send_P(200, "text/css; charset=utf-8", app_css, sizeof(app_css));
}

void handleRoot()
{
  commonHeader();
  cacheControlHeader(false);
  server.send_P(200, "text/html; charset=utf-8", index_html, sizeof(index_html));
}

void jsonToConfig(JsonDocument &json)
{
  config.mode = json[F("mode")].as<uint8_t>();
  setConfigStr(config, release_tag, json[F("release_tag")]);

  config.update_startup = json[F("update_startup")];

  setConfigStr(config, hostname, json[F("hostname")]);

  setConfigStr(config, sonnenHostname, json[F("sn_host")]);
  setConfigStr(config, sonnenApiToken, json[F("sn_token")]);
  config.gridMin_W = json[F("sn_grdmin")].as<uint16_t>();
  config.loadPower_W = json[F("sn_loadpower")].as<uint16_t>();
  config.cap_bat_min_Wh = json[F("sn_cap_min")].as<uint16_t>();

  config.lon = json[F("lc_lon")].as<float>();
  config.lat = json[F("lc_lat")].as<float>();
  config.kWp = json[F("lc_kWp")].as<float>();
  config.az = json[F("lc_az")].as<uint16_t>();
  config.dec = json[F("lc_dec")].as<uint16_t>();

  setConfigStr(config, location, json[F("loc")]);
  setConfigStr(config, tz, json[F("tz")]);

  config.version = json[F("version")].as<uint16_t>();
}

void configToJson(JsonDocument &json, bool confidential)
{
  json[F("mode")] = config.mode;
  json[F("release_tag")] = config.release_tag;

  json[F("update_startup")] = config.update_startup;

  json[F("hostname")] = config.hostname;

  json[F("sn_host")] = config.sonnenHostname;
  if (!confidential)
  {
    json[F("sn_token")] = config.sonnenApiToken;
  }
  json[F("sn_grdmin")] = config.gridMin_W;
  json[F("sn_loadpower")] = config.loadPower_W;
  json[F("sn_cap_min")] = config.cap_bat_min_Wh;

  json[F("lc_lon")] = config.lon;
  json[F("lc_lat")] = config.lat;
  json[F("lc_kWp")] = config.kWp;
  json[F("lc_az")] = config.az;
  json[F("lc_dec")] = config.dec;

  json[F("loc")] = config.location;
  json[F("tz")] = config.tz;

  json[F("version")] = config.version;
}

void sendJson(String from, JsonDocument &json)
{
  String jsonString;

  serializeJsonPretty(json, jsonString);
  DEBUGF("%s - json %s\n", from.c_str(), jsonString.c_str());

  cacheControlHeader(false);
  server.send(200, "application/json", jsonString);
}

void addLog(JsonArray &array, logEntry &log)
{
  if (strlen(log.msg))
  {
    JsonObject e = array.add<JsonObject>();
    e[F("ts")] = log.ts;
    e[F("msg")] = log.msg;
  }
}

void handleData()
{
  JsonDocument json;

  configToJson(json, true);
  json[F("start_ts")] = toLocalDate(&systemState, systemState.start_ts);
  sendJson("data", json);
}

// api/status
void handleStatus()
{
  JsonDocument json;

  json[F("cons_w")] = systemState.cons_W;
  json[F("cons_avg_w")] = systemState.cons_avg_W;
  json[F("prod")] = systemState.prod_W;
  json[F("grid")] = systemState.gridFeedIn_W;
  json[F("usoc")] = systemState.usoc;
  json[F("chrg")] = systemState.charge;
  json[F("pac_total_w")] = systemState.pac_total_W * -1; // invert - positive means discharge, we invert for display
  json[F("switch")] = systemState.switchEnabled;
  json[F("sn_cap_max")] = systemState.cap_bat_max_Wh;
  json[F("sn_cycles")] = systemState.bat_cycles;
  json[F("sn_cap_soh")] = systemState.cap_bat_soh;
  char buf[16];
  json[F("sn_fchrg_ts")] = format_duration(systemState.fullChargeRequestIn, buf, sizeof(buf));

  char dev_id[40] = "n.a.";
  char dev_nfo[40] = "";
  device_map *device = lpb->getDestDevice();
  if (device != NULL)
  {
    snprintf_P(dev_id, sizeof(dev_id), PSTR("%d - %s"), device->dev_id, device->name);
    snprintf_P(dev_nfo, sizeof(dev_nfo), PSTR("(Fam: %d, Var: %d, Ser: %08X)"), device->dev_fam, device->dev_var, device->dev_serial);
  }
  json[F("bs_dev_id")] = dev_id;
  json[F("bs_dev_nfo")] = dev_nfo;
  json[F("bs_t_cur")] = String(systemState.boiler_T_cur);
  json[F("bs_t_max")] = String(systemState.boiler_T_max);
  json[F("bs_t_min")] = String(systemState.boiler_T_min);
  json[F("bs_t_nom")] = String(systemState.boiler_T_nom);

  JsonArray errors = json[F("errors")].to<JsonArray>();
  addLog(errors, systemState.error_bt);
  addLog(errors, systemState.error_bs);
  addLog(errors, systemState.error_lc);

  JsonArray events = json[F("events")].to<JsonArray>();
  unsigned short i = SIZE_EVENT_BUFFER;
  while (i-- > 0)
  {
    logEntry log = systemState.events[(systemState.eventIx + i) % SIZE_EVENT_BUFFER];
    addLog(events, log);
  }

  json[F("version")] = config.version;

  sendJson("status", json);
}

void handleAPI()
{

  bool doRestart = false;

  if (server.hasArg(F("mode")))
  {
    config.mode = server.arg(F("mode")).toInt() & 3;
    DEBUGF("Mode: %d\n", config.mode);
    saveConfig();
  }
  else if (server.hasArg(F("calibrate")))
  {
    if (server.arg(F("calibrate")).equals(F("set")))
    {
      config.loadPower_W = MAX(0, server.arg(F("sn_loadpower")).toInt());
      saveConfig();
    }
    else
    {
      calibrateLoad = true;
    }
  }
  else if (server.hasArg(F("update_startup")))
  {
    config.update_startup = server.arg(F("update_startup")).toInt();
    DEBUGF("Enabled: %d\n", config.update_startup);
    saveConfig();
  }
  else if (server.hasArg(F("boiler")))
  {
    // ?
  }
  else if (server.hasArg(F("sonnen")))
  {
    setConfigStr(config, sonnenHostname, server.arg(F("sn_host")).c_str());
    String apiToken = server.arg(F("sn_token"));
    if (apiToken.length() > 0)
    {
      setConfigStr(config, sonnenApiToken, apiToken.c_str());
    }
    config.gridMin_W = MAX(50, MAX(0, server.arg(F("sn_grdmin")).toInt()));
    config.cap_bat_min_Wh = MIN(systemState.cap_bat_max_Wh, MAX(0, server.arg(F("sn_cap_min")).toInt()));
    saveConfig();
    systemState.skipUpdateCountSysten = 0;
  }
  else if (server.hasArg(F("location")))
  {
    config.lon = MAX(0, server.arg(F("lc_lon")).toDouble());
    config.lat = MAX(0, server.arg(F("lc_lat")).toDouble());
    config.kWp = MAX(0, server.arg(F("lc_kWp")).toDouble());
    config.az = MIN(360, MAX(0, server.arg(F("lc_az")).toInt()));
    config.dec = MIN(90, MAX(0, server.arg(F("lc_dec")).toInt()));
    systemState.pv_forecast_ts = 0; // force fetch new data
    saveConfig();
  }
  else if (server.hasArg(F("hostname")))
  {
    setConfigStr(config, hostname, server.arg(F("hostname")).c_str());
    doRestart = saveConfig();
  }
  else if (server.hasArg(F("restart")))
  {
    doRestart = saveConfig();
  }
  else if (server.hasArg(F("reset")))
  {
    if (server.arg(F("reset")).toInt())
    {
      configDefaults();
      wifiManager.resetSettings(); // reset wifi settings
      doRestart = saveConfig();
    }
  }
  else if (server.hasArg(F("update")))
  {
    handleGithubUpdate();
    return;
  }

  server.sendHeader(F("location"), F("/"));
  server.send(303);
  server.client().flush();

  if (doRestart)
  {
    restart();
  }
}

void handleNotFound()
{
  server.send_P(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}

void restart()
{
  server.close();
  LittleFS.end();
  ESP.restart();
}

String userAgent()
{
  return String(F("SmartSwitch v")) + config.release_tag;
}

const char STRING_HTML_UPDATE[] PROGMEM = "<html lang='en'><head><meta http-equiv='refresh' content='30;url=/'></head><body><h1>%s</h1><p>Redirect in 30s...</p></body></html>";

void sendUpdateStatus(int code, const char *status)
{
  char buffer[256];
  snprintf_P(buffer, sizeof(buffer), STRING_HTML_UPDATE, status);
  server.send(code, F("text/html;charset=utf-8"), buffer);
}

void handleGithubUpdate()
{
  GithubOTA gh_updater(UPDATE_HOST, UPDATE_URL, UPDATE_TYPE, UPDATE_FILENAME);

  if (!gh_updater.checkUpdate(config.release_tag))
  {
    if (server.client() && server.client().connected())
    {
      char msg[128];
      snprintf_P(msg, sizeof(msg), PSTR("Update failed: %s"), gh_updater.getUpdateStatus().c_str());
      sendUpdateStatus(404, msg);
    }
    return;
  }

  if (server.client() && server.client().connected())
  {
    char msg[128];
    snprintf_P(msg, sizeof(msg), PSTR("Update found: Going to install Release %s."), gh_updater.release_tag.c_str());
    sendUpdateStatus(200, msg);
  }
  if (gh_updater.doUpdate(userAgent(), &onOTABegin, &onOTAEnd))
  {
    setConfigStr(config, release_tag, gh_updater.release_tag.c_str());
    if (!saveConfig())
    {
      putEvent(String(F("error saving config with release tag ")) + gh_updater.release_tag);
    }
    else
    {
      DEBUGLN("config saved.");
      restart();
    }
  }
  else
  {
    putEvent(gh_updater.getUpdateStatus());
  }
}

void clearLocationError()
{
  systemState.error_lc.msg[0] = '\0';
}

void putLocationError(String event)
{
  putLog(systemState.error_lc, event.c_str());
}

void clearBatteryError()
{
  systemState.error_bt.msg[0] = '\0';
}

void putBatteryError(String event)
{
  putLog(systemState.error_bt, event.c_str());
}

void putBoilerError(String event)
{
  putLog(systemState.error_bs, event.c_str());
}

void clearBoilerError()
{
  systemState.error_bs.msg[0] = '\0';
}

void putLog(logEntry &log, const char *event)
{
  strncpy(log.msg, event, sizeof(log.msg));
  log.ts = systemState.ts;
  DEBUGF("log %u: %s\n", systemState.ts, event);
}

void putEvent(const char *event)
{
  putLog(systemState.events[systemState.eventIx++ % SIZE_EVENT_BUFFER], event);
}

void putEvent(String event)
{
  putEvent(event.c_str());
}

bool updateBoilerData()
{

  static long lastUpdate = 0;
  static long lastResult = false;

  long ms = millis();
  if (lastUpdate == 0 || ms > lastUpdate + BOILER_UPDATE_INTERVAL_MS)
  {
    lastUpdate = ms;

    boiler_t boilerData;
    if ((lastResult = lpb->update(&boilerData, isDevMode())))
    {
      systemState.boiler_T_cur = boilerData.t_cur;
      systemState.boiler_T_nom = boilerData.t_nom;
      systemState.boiler_T_min = boilerData.t_min;
      systemState.boiler_T_max = boilerData.t_max;

      clearBoilerError();
    }
    else
    {
      putBoilerError(F("Could not update boiler data."));
    }
  }
  return lastResult; // no new data, so still ok
}

void calibrate(bool validData)
{
  static uint8_t cnt = CALIBRATE_LOOP_CNT;
  static uint16_t load_on[CALIBRATE_MEASURE];
  static uint16_t load_off[CALIBRATE_MEASURE];

  if (!validData)
  {
    putEvent("invalid configuration, cannot calibrate load");
    calibrateLoad = false;
    return;
  }

  if (cnt == 0)
  {
    config.loadPower_W = MAX(0, (median_uint16(load_on, CALIBRATE_MEASURE) - median_uint16(load_off, CALIBRATE_MEASURE)) * 103 / 100); // +3%
    saveConfig();

    char event[64];
    snprintf_P(event, sizeof(event), PSTR("load calibrated, on %dW / off %dW => %dW"), median_uint16(load_on, CALIBRATE_MEASURE), median_uint16(load_off, CALIBRATE_MEASURE), config.loadPower_W);
    putEvent(event);

    memset(load_on, 0, sizeof(load_on));
    memset(load_off, 0, sizeof(load_off));
    cnt = CALIBRATE_LOOP_CNT;
    calibrateLoad = false;

    systemState.switchEnabled = false;
  }
  else
  {
    bool on = (--cnt & CALIBRATE_ONOFF_TOGGLE) == 0;
    if ((cnt & CALIBRATE_MEASURE) == 0) // wait with measure after toggle
    {
      if (on)
      {
        load_on[cnt & (CALIBRATE_MEASURE - 1)] = systemState.cons_W;
      }
      else
      {
        load_off[cnt & (CALIBRATE_MEASURE - 1)] = systemState.cons_W;
      }
    }
    systemState.switchEnabled = on;
  }
}

uint16_t updateSystemCount = 0;

bool isUpdateSystemData()
{
  return updateSystemCount == 0;
}

void updateSystemCounter()
{
  if (updateSystemCount-- == 0)
  {
    updateSystemCount = 4 * 60 * SYSTEM_UPDATE_INTERVAL_MS / 1000; // every 4 hours
  }
}

// main loop
void loop()
{
  if (doUpdateFlag)
  {
#ifdef ESP32
// esp_task_wdt_reset();
#elif ESP8266
    ESP.wdtFeed();
#endif
    if (isUpdateSystemData() && ensureConnected())
    {
      configTime(0, 0, "pool.ntp.org", "time.nist.gov");
      DEBUGLN("systime configured.");
    }

    bool validData = ensureConnected() && updateSystemData() && updateBoilerData();

    if (calibrateLoad)
    {
      calibrate(validData);
    }
    else
    {
      updateState(&config, &systemState, validData);

      updateSolarForecast();
    }
    updateSwitch(systemState.switchEnabled);

    DEBUGF("ESP Heap %uk CPU: %uMhz valid: %d\n", ESP.getFreeHeap() >> 10, ESP.getCpuFreqMHz(), validData);

    updateSystemCounter();

    doUpdateFlag = false;
  }
  server.handleClient();
}

void buildInLED(bool onOff)
{
#ifdef ESP32
  digitalWrite(LED_BUILTIN, onOff ? HIGH : LOW);
#else
  digitalWrite(LED_BUILTIN, onOff ? LOW : HIGH);
#endif
}

void statusLED(int status)
{
  int d = 1000 - (100 * (status % 10));
  for (int i = 0; i < status; i++)
  {
    buildInLED(true);
    delay(d);
    buildInLED(false);
    delay(d);
  }
}

bool ensureConnected()
{
  wl_status_t status = WiFi.status();
  if (status != WL_CONNECTED)
  {
    DEBUG("Connecting");
    for (int retry = 1; retry <= 8; retry++)
    {
      statusLED(0);
      DEBUG(".");
      delay(500);

      status = WiFi.status();
      if (status == WL_CONNECTED)
      {
        DEBUGF("Connected. IP address: %s status: %d\n", WiFi.localIP().toString().c_str(), status);
        break;
      }
      else
      {
        DEBUGF("Wifi not connected, status: %d\n", status);
        if (status == WL_IDLE_STATUS)
        {
          WiFi.disconnect();
          status = WiFi.begin();
        }
      }
    }
    if (status != WL_CONNECTED)
    {
      restart();
    }
  }
  return status == WL_CONNECTED;
}

bool fetchApiData(String uri, JsonDocument &json)
{
  return fetchApiData(uri, json, NULL);
}

bool fetchApiData(String uri, JsonDocument &json, JsonDocument *filter)
{

  bool r = false;

  RestClient restClient;

  if (strlen(config.sonnenHostname) && strlen(config.sonnenApiToken))
  {
    char url[128];
    snprintf_P(url, sizeof(url), PSTR("http://%.31s/%s/%s"), config.sonnenHostname, SONNEN_API_URI, uri.c_str());
    r = restClient.fetch(String(url), json, filter, "auth-token", config.sonnenApiToken);
    if (!r)
    {
      DEBUGF("ERROR: fetchApiData(%s)\n", uri.c_str());
      putBatteryError(restClient.lastError());
    }
  }
  else
  {
    putBatteryError(F("Sonnen Battery Hostname/Token not properly configured!"));
  }
  return r;
}

static bool updateSystemData()
{

  static uint8_t errorLoopBackoff = 1;

  JsonDocument json;

  if (systemState.skipUpdateCountSysten > 0)
  {
    systemState.skipUpdateCountSysten--;
    return false;
  }

  bool ok = true;

  if (systemState.inv_max_w == -1)
  {
    if ((ok &= fetchApiData(SONNEN_API_CONFIGURATIONS, json)))
    {
      systemState.inv_max_w = json[F("IC_InverterMaxPower_w")].as<int>();
      systemState.cap_bat_new_Wh = json[F("CM_MarketingModuleCapacity")].as<int>() * json[F("IC_BatteryModules")].as<int>() * 90 / 100;
      DEBUGF("IC_InverterMaxPower_w %d, max battery capacity %d\n", systemState.inv_max_w, systemState.cap_bat_new_Wh);
    }
  }
  if (systemState.cap_bat_max_Wh == 0 || isUpdateSystemData())
  {
    if (ok && (ok &= fetchApiData(SONNEN_API_BATTERY, json)))
    {
      systemState.cap_bat_max_Wh = json[F("fullchargecapacitywh")].as<uint16_t>();
      systemState.bat_cycles = json[F("cyclecount")].as<uint16_t>();
      systemState.cap_bat_soh = systemState.cap_bat_max_Wh * 100 / systemState.cap_bat_new_Wh;
      DEBUGF("FullChargeCapacity %d, cycles: %d, capacity: %2d%%\n", systemState.cap_bat_max_Wh, systemState.bat_cycles, systemState.cap_bat_soh);
    }
  }
  JsonDocument filter;
  filter[F("UTC_Offet")] = true;
  filter[F("ic_status")][F("Setpoint Priority")][F("Full Charge Request")] = true;
  filter[F("ic_status")][F("nextfullchargestarttime")] = true;
  filter[F("ic_status")][F("secondssincefullcharge")] = true;
  if (ok && (ok &= fetchApiData(SONNEN_API_LATEST_DATA, json, &filter)))
  {
    systemState.utc_offset = json[F("UTC_Offet")].as<uint8_t>() * 3600;
    systemState.fullChargeRequest = json[F("ic_status")][F("Setpoint Priority")][F("Full Charge Request")].as<bool>();
    systemState.fullChargeRequestIn =
        json[F("ic_status")][F("nextfullchargestarttime")].as<uint32_t>() -
        json[F("ic_status")][F("secondssincefullcharge")].as<uint32_t>();

    DEBUGF("UTC offset %d, discharge not allowed: %d %u\n", systemState.utc_offset, systemState.fullChargeRequest, systemState.fullChargeRequestIn);
  }
  if (ok && (ok &= fetchApiData(SONNEN_API_STATUS, json)))
  {
    systemState.usoc = json[F("USOC")].as<uint8_t>();
    systemState.cap_bat_Wh = systemState.cap_bat_max_Wh * systemState.usoc / 100;
    systemState.gridFeedIn_W = json[F("GridFeedIn_W")].as<int>();
    systemState.prod_W = json[F("Production_W")].as<uint16_t>();
    systemState.cons_W = json[F("Consumption_W")].as<uint16_t>();
    systemState.cons_avg_W = json[F("Consumption_Avg")].as<uint16_t>();
    systemState.pac_total_W = json[F("Pac_total_W")].as<int16_t>();
    systemState.charge = json[F("BatteryCharging")].as<short>() - json[F("BatteryDischarging")].as<short>();

    struct tm time;
    strptime(json[F("Timestamp")].as<const char *>(), "%Y-%m-%d %H:%M:%S", &time);

    systemState.ts = mktime(&time) - systemState.utc_offset; // to UTC
    if (systemState.start_ts == 0)
    {
      systemState.start_ts = systemState.ts;
    }
    systemState.tm_yday = time.tm_yday;

    updateSystemState(&config, &systemState);
  }

  if (ok && !(ok &= (config.loadPower_W > 0)))
  {
    // TODO wrong error category
    putBatteryError("Load not properly configured! Must be > 0!");
  }

  if (ok)
  {
    clearBatteryError();
    errorLoopBackoff = 1; // reset backoff
  }
  else
  {
    systemState.skipUpdateCountSysten = errorLoopBackoff; // error loop back off
    if (errorLoopBackoff != (1 << 7))
    {
      errorLoopBackoff <<= 1;
    }
  }

  return ok;
}

static void updateState(SystemConfig *systemConfig, SystemState *systemState, bool validData)
{
  bool desiredState = systemState->switchEnabled;

  if (systemConfig->mode == SMODE_AUTO)
  {
    char msg[80];
    bool desiredState = determineDesiredState(msg, sizeof(msg), systemConfig, systemState, validData);
    if (systemState->switchEnabled != desiredState)
    {
      putEvent(String(F("switch ")) + (desiredState ? F("on") : F("off")) + F(" - ") + msg);
    }
  }
  systemState->switchEnabled = (desiredState && systemConfig->mode == SMODE_AUTO) || (systemConfig->mode == SMODE_ON); // combine with mode

  DEBUGF("ts: %s (%u) usoc: %2d%% p/c: %d/%d/%d (W) avg: %d (Wh) grid: %d (W) mode %d: heater %d\n", toDate(systemState->ts), systemState->ts, systemState->usoc, systemState->prod_W, systemState->cons_W, systemState->cons_W, systemState->cons_avg_W, systemState->gridFeedIn_W, systemConfig->mode, systemState->switchEnabled);
}

static void updateSwitch(bool switchEnabled)
{
  digitalWrite(PIN_SSR, switchEnabled ? HIGH : LOW);
  buildInLED(switchEnabled);
}

static bool loadConfig()
{
  if (!LittleFS.exists(CONFIGFILE))
  {
    DEBUGLN("Config file not found");

    return false;
  }

  File f = LittleFS.open(CONFIGFILE, "r");
  if (!f)
  {
    DEBUGF("Could not open config file %s\n", CONFIGFILE);

    return false;
  }

  JsonDocument json;
  DeserializationError error = deserializeJson(json, f);
  f.close();

  if (error)
  {
    return false;
  }

  DEBUGLN("Config loaded:");
  serializeJsonPretty(json, Serial);

  jsonToConfig(json);

  return true;
}

static bool saveConfig()
{
  File f = LittleFS.open(CONFIGFILE, "w");
  if (!f)
  {
    DEBUGF("Could not open config file %s for writing\n", CONFIGFILE);
    return false;
  }

  config.version++; // update version

  JsonDocument json;
  configToJson(json, false);
  DEBUGLN("saveConfig() => ");
  serializeJsonPretty(json, Serial);
  serializeJson(json, f);
  f.close();
  saveConfigFile = false;
  return true;
}
