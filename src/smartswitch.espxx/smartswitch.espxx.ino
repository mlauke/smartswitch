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
#include "SmartSwitch.h"
#include "GithubOTA.h"
#include "RestClient.h"
#include "LPB.h"

// echo -e "const char index_html[] PROGMEM = { $(gzip -9 -c nginx/index.html | hexdump -v -e '1/1 "0x%02X, "') };" > src/smartswitch.esp8266/index_html.h && \
   echo -e "const char app_js[] PROGMEM = { $(gzip -9 -c nginx/app.js | hexdump -v -e '1/1 "0x%02X, "') };" > src/smartswitch.esp8266/app_js.h && \
   echo "const char app_css[] PROGMEM = { $(gzip -9 -c nginx/app.css | hexdump -v -e '1/1 "0x%02X, "') };" > src/smartswitch.esp8266/app_css.h

#include "app_js.h"
#include "app_css.h"
#include "app_icon.h"
#include "index_html.h"

#ifdef ESP32
static WebServer server(WEBSERVER_PORT);
#elif defined(ESP8266)
static ESP8266WebServer server(WEBSERVER_PORT);
#endif

static LPB* lpb;
static WiFiManager wifiManager;
static Ticker timer;
static volatile bool doUpdateFlag = false;
static systemDataStruct systemData;
static configStruct config;
static bool saveConfigFile = false;

int stdOffset = 3600;  // 1h utc offset Europe/Berlin
int dstOffset = 3600;  // 1h suummer time offset


void saveConfigCallback() {
  saveConfigFile = true;
}

#define HOSTNAME "smartswitch"
#define RELEASE_TAG "-"

void setup() {
  configDefaults();
  systemDefaults();

  Serial.begin(SERIAL_BAUDRATE);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_SSR, OUTPUT);

  lpb = new LPB(PIN_LPB_RX, PIN_LPB_TX, 2, 0);

  toggleSwitch(false);

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  WiFiManagerParameter custom_hostname("hostname", "Hostname", config.hostname, CFG_SZ_HOSTNAME);
  wifiManager.addParameter(&custom_hostname);
  wifiManager.autoConnect("SmartSwitchAP");
  wifiManager.setConfigPortalTimeout(10);

  Serial.println("Mounting FS...");
  if (LittleFS.begin()) {
    if (!loadConfig()) {
      Serial.println("Error loading config");
    }
  } else {
    Serial.println("Failed to mount FS. Attempting to format.");
    LittleFS.format();
    Serial.println("format done.");

    saveConfig();
  }

  if (saveConfigFile) {
    String lcHostname = String(custom_hostname.getValue());
    lcHostname.toLowerCase();
    setConfigStr(config, hostname, lcHostname.c_str());
    if (!saveConfig()) {
      Serial.println("Error saving config");
    }
  }

  if (config.update_startup) {
    handleGithubUpdate();
  }

  WiFi.setHostname(config.hostname);
  MDNS.begin(config.hostname);

  server.on("/", handleRoot);
  server.on("/favicon.ico", handleFavicon);
  server.on("/app.js", handleAppJs);
  server.on("/app.css", handleAppCss);
  server.on("/api/data", handleData);
  server.on("/api/status", handleStatus);
  server.on("/api/update", handleAPI);
  server.onNotFound(handleNotFound);

  updateLocation();

  server.begin();  // Actually start the server
  //server.keepAlive(false);
  Serial.println("HTTP server started");

  lpb->enableInterface();

  uint8_t retry = 0;
  while (!lpb->GetDevId() && retry++ < 3) {
    putBoilerError(String("No device found, retry ") + retry);
  }

  timer.attach_ms(SYSTEM_UPDATE_INTERVAL_MS, timerCallback);

#ifdef ESP32
  //esp_task_wdt_init({ 20000, 0, true });
#elif defined(ESP8266)
  ESP.wdtEnable(20000);
#endif
}

void timerCallback() {
  doUpdateFlag = true;
}

void onOTABegin() {
  timer.detach();
  lpb->disableInterface();
#if defined(ESP8266)
  ESP.wdtDisable();
#endif
}

void onOTAEnd(bool success) {
  if (success) {
    Serial.println("OTA update finished successfully!");  // Log when OTA has finished
    return;
  }
  Serial.println("There was an error during OTA update!");
}

void systemDefaults() {
  systemData.pv_forecast_ts = 0;
  memset(systemData.pv_forecast_wh_h, 0, sizeof(systemData.pv_forecast_wh_h));
}

void configDefaults() {  // init config struct with default values

#ifdef ESP32
  setConfigStr(config, hostname, WiFi.getHostname());
#elif defined(ESP8266)
  setConfigStr(config, hostname, WiFi.hostname().c_str());
#endif
  setConfigStr(config, release_tag, RELEASE_TAG);
  config.sonnenHostname[0] = '\0';
  config.sonnenApiToken[0] = '\0';
  config.lon = 0.0;
  config.lat = 0.0;
  config.az = 0;    // default azimuth to 0 (south)
  config.dec = 30;  //default panel declination
  setConfigStr(config, tz, "Europe/Berlin");
  config.location[0] = '\0';
  config.loadPower_W = 1000;  //initial assume 1kW
  config.cap_bat_min_Wh = 500;
  config.gridMin_W = GRID_PURCHASE_THRESHOLD_W;
  config.mode = 0;  //initial set to off
  config.update_startup = false;

  //  config.boiler_T_max = 65;
  //config.boiler_T_nom = 50;
}

bool updateSolarForecast() {

  long ms = millis();
  if (config.lat != 0.0 && config.lon != 0.0 && (systemData.pv_forecast_ts == 0 || (systemData.pv_forecast_ts + SOLAR_FORECAST_INTERVAL_MS) < ms)) {

    RestClient restClient;
    JsonDocument doc;

    String solarUrl = strstr(config.hostname, "-dev") == NULL ? URL_SOLAR_FORECAST : URL_SOLAR_FORECAST_DEV;
    char url[128];
    snprintf(url, sizeof(url), solarUrl.c_str(), config.lat, config.lon, config.az, config.dec, config.kWp);

    if (restClient.fetch(String(url), doc)) {

      serializeJsonPretty(doc, Serial);
      uint8_t i = 0;
      for (JsonPair entry : doc["result"].as<JsonObject>()) {
        uint32_t ts = strtoul(entry.key().c_str(), NULL, 10);
        uint32_t wh = entry.value().as<uint32_t>();
        if (i < sizeof(systemData.pv_forecast_wh_h) / sizeof(systemData.pv_forecast_wh_h[0])) {
          systemData.pv_forecast_wh_h[i][0] = ts;
          systemData.pv_forecast_wh_h[i][1] = wh;
          i++;
        } else {
          putEvent("WARN: overflow " + i);
        }
      }
      setConfigStr(config, location, doc["message"]["info"]["place"].as<const char*>());

      clearLocationError();
    } else {
      putLocationError("WARN solar forecast " + restClient.lastError());
    }
    systemData.pv_forecast_ts = restClient.lastResponseCode() > 0 ? ms : 0;
  }
  return true;  //always true, if there are no forecast the calculation may detect that there is enough surplus to be able to switch the load on
}

void updateLocation() {

  if (config.lat == 0.0 && config.lon == 0.0) {

    RestClient restClient;
    JsonDocument doc;

    if ((saveConfigFile = restClient.fetch(URL_LOCATION, doc))) {
      config.lon = doc["lon"].as<double>();
      config.lat = doc["lat"].as<double>();
      snprintf(config.location, CFG_SZ_LOCATION, "%s %s", doc["zip"].as<const char*>(), doc["city"].as<const char*>());
      setConfigStr(config, tz, doc["timezone"]);
      Serial.printf("Location: %f/%f - tz: %s loc: %s\n", config.lon, config.lat, config.tz, config.location);
    }
  }
}

void cacheControlHeader(bool cache) {
  if (cache)
    server.sendHeader("cache-control", "max-age=31536000, must-revalidate");
  else
    server.sendHeader("cache-control", "no-cache");
}

void commonHeader() {
  //server.sendHeader("last-modified", "");
  server.sendHeader("connection", "close");
  server.sendHeader("content-encoding", "gzip");
}

void handleFavicon() {
  cacheControlHeader(true);
  server.send_P(200, "image/x-icon", app_icon, sizeof(app_icon));
}

void handleAppJs() {
  commonHeader();
  cacheControlHeader(true);
  server.send_P(200, "text/javascript", app_js, sizeof(app_js));
}

void handleAppCss() {
  commonHeader();
  cacheControlHeader(true);
  server.send_P(200, "text/css; charset=utf-8", app_css, sizeof(app_css));
}

void handleRoot() {
  commonHeader();
  cacheControlHeader(false);
  server.send_P(200, "text/html; charset=utf-8", index_html, sizeof(index_html));
}

void jsonToConfig(JsonDocument& data) {
  config.mode = data["mode"].as<uint8_t>();
  setConfigStr(config, release_tag, data["release_tag"]);

  config.update_startup = data["update_startup"];

  setConfigStr(config, hostname, data["hostname"]);

  setConfigStr(config, sonnenHostname, data["sn_host"]);
  setConfigStr(config, sonnenApiToken, data["sn_token"]);
  config.gridMin_W = data["sn_grdmin"].as<uint16_t>();
  config.loadPower_W = data["sn_loadpower"].as<uint16_t>();
  config.cap_bat_min_Wh = data["sn_cap_min"].as<uint16_t>();

  config.lon = data["lc_lon"].as<float>();
  config.lat = data["lc_lat"].as<float>();
  config.kWp = data["lc_kWp"].as<float>();
  config.az = data["lc_az"].as<uint16_t>();
  config.dec = data["lc_dec"].as<uint16_t>();

  setConfigStr(config, location, data["loc"]);
  setConfigStr(config, tz, data["tz"]);

  //  config.boiler_T_max = data["bs_t_max"].as<uint8_t>();
  //config.boiler_T_nom = data["bs_t_nom"].as<uint8_t>();
}

void configToJson(JsonDocument& data) {
  data["mode"] = config.mode;
  data["release_tag"] = config.release_tag;

  data["update_startup"] = config.update_startup;

  data["hostname"] = config.hostname;

  data["sn_host"] = config.sonnenHostname;
  data["sn_token"] = config.sonnenApiToken;
  data["sn_grdmin"] = config.gridMin_W;
  data["sn_loadpower"] = config.loadPower_W;
  data["sn_cap_min"] = config.cap_bat_min_Wh;

  data["lc_lon"] = config.lon;
  data["lc_lat"] = config.lat;
  data["lc_kWp"] = config.kWp;
  data["lc_az"] = config.az;
  data["lc_dec"] = config.dec;

  data["loc"] = config.location;
  data["tz"] = config.tz;

  //  data["bs_t_max"] = config.boiler_T_max;
  //data["bs_t_nom"] = config.boiler_T_nom;
}

void sendJson(String from, JsonDocument& json) {

  String jsonString;

  size_t r = serializeJsonPretty(json, jsonString);
  Serial.printf("%s - json (%d) %s\n", from.c_str(), r, jsonString.c_str());

  cacheControlHeader(false);
  server.send(200, "application/json", jsonString);
}

void handleData() {

  JsonDocument data;

  configToJson(data);
  data["sn_cap_max"] = systemData.cap_bat_max_Wh;

  sendJson("data", data);
}

void addLog(JsonArray& array, logEntry& log) {
  if (!log.msg.isEmpty()) {
    JsonObject e = array.add<JsonObject>();
    e["ts"] = log.ts;
    e["msg"] = log.msg;
  }
}

///api/status
void handleStatus() {

  JsonDocument data;

  data["cons_w"] = systemData.cons_W;
  data["cons_avg_w"] = systemData.cons_avg_W;
  data["prod"] = systemData.prod_W;
  data["grid"] = systemData.gridFeedIn_W;
  data["usoc"] = systemData.usoc;
  data["chrg"] = systemData.charge;
  data["switch"] = systemData.switchEnabled;

  char devid[40] = "unknown";
  device_map* device = lpb->getDestDevice();
  if (device != NULL) {
    snprintf(devid, sizeof(devid), "%d - %s (%d/%d)", device->dev_id, device->name, device->dev_fam, device->dev_var);
  }
  data["bs_devid"] = devid;

  data["bs_t_cur"] = systemData.boiler_T_cur;
  data["bs_t_max"] = systemData.boiler_T_max;
  data["bs_t_min"] = systemData.boiler_T_min;
  data["bs_t_nom"] = systemData.boiler_T_nom;

  JsonArray errors = data["errors"].to<JsonArray>();
  addLog(errors, systemData.error_bt);
  addLog(errors, systemData.error_bs);
  addLog(errors, systemData.error_lc);

  JsonArray events = data["events"].to<JsonArray>();
  int i = sizeof(systemData.events) / sizeof(systemData.events[0]);
  while (i-- > 0) {
    logEntry log = systemData.events[(systemData.eventIx + i) % 8];
    addLog(events, log);
  }

  sendJson("status", data);
}

void handleAPI() {

  bool doRestart = false;

  if (server.hasArg("mode")) {
    config.mode = server.arg("mode").toInt() & 3;
    Serial.printf("Mode: %d\n", config.mode);
    saveConfig();

  } else if (server.hasArg("update_startup")) {
    config.update_startup = server.arg("update_startup").toInt();
    Serial.printf("Enabled: %d\n", config.update_startup);
    saveConfig();

  } else if (server.hasArg("boiler")) {
    //    config.boiler_T_max = MIN(85, MAX(0, server.arg("bs_t_max").toInt()));
    //  config.boiler_T_nom = MIN(85, MAX(0, server.arg("bs_t_nom").toInt()));
    //saveConfig();

  } else if (server.hasArg("sonnen")) {
    setConfigStr(config, sonnenHostname, server.arg("sn_host").c_str());
    setConfigStr(config, sonnenApiToken, server.arg("sn_token").c_str());
    config.gridMin_W = MAX(50, MAX(0, server.arg("sn_grdmin").toInt()));
    config.loadPower_W = MAX(0, server.arg("sn_loadpower").toInt());
    config.cap_bat_min_Wh = MIN(systemData.cap_bat_max_Wh, MAX(0, server.arg("sn_cap_min").toInt()));
    saveConfig();

  } else if (server.hasArg("location")) {
    config.lon = MAX(0, server.arg("lc_lon").toDouble());
    config.lat = MAX(0, server.arg("lc_lat").toDouble());
    config.kWp = MAX(0, server.arg("lc_kWp").toDouble());
    config.az = MIN(360, MAX(0, server.arg("lc_az").toInt()));
    config.dec = MIN(90, MAX(0, server.arg("lc_dec").toInt()));
    systemData.pv_forecast_ts = 0;  // force fetch new data
    saveConfig();

  } else if (server.hasArg("hostname")) {
    setConfigStr(config, hostname, server.arg("hostname").c_str());
    doRestart = saveConfig();

  } else if (server.hasArg("restart")) {
    doRestart = saveConfig();

  } else if (server.hasArg("reset")) {
    if (server.arg("reset").toInt()) {
      configDefaults();
      wifiManager.resetSettings();  // reset wifi settings
      doRestart = saveConfig();
    }
  } else if (server.hasArg("update")) {
    handleGithubUpdate();
    return;
  }

  server.sendHeader("location", "/");
  server.send(303);

  if (doRestart) {
    restart();
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not found");  // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}

void restart() {
  server.close();
  LittleFS.end();
  ESP.restart();
}

void handleGithubUpdate() {

  GithubOTA gh_updater(UPDATE_HOST, UPDATE_URL, UPDATE_TYPE, UPDATE_FILENAME);

  if (!gh_updater.checkUpdate(config.release_tag)) {
    if (server.client() && server.client().connected()) {
      server.send(404, "text/plain", "No Update found");
    }
    return;
  }

  char buffer[256];
  snprintf(buffer, sizeof(buffer), "<html lang='en'><head><meta http-equiv='refresh' content='30;url=/'></head><body><p>Update found, Going to install Release %s</p></body></html>", gh_updater.release_tag.c_str());
  if (server.client() && server.client().connected()) {
    server.send(200, "text/html;charset=utf-8", buffer);
  }
  DEBUG(buffer);

  if (gh_updater.doUpdate(onOTABegin)) {
    setConfigStr(config, release_tag, gh_updater.release_tag.c_str());
    if (!saveConfig()) {
      Serial.println("Error saving config");
      putEvent("Error saving config");
      return;
    }
    Serial.println("config saved.");
    restart();
  }
  putEvent(gh_updater.getUpdateError());
}

void clearLocationError() {
  systemData.error_lc.msg.clear();
}

void putLocationError(String event) {
  putLog(systemData.error_lc, event.c_str());
}

void clearBatteryError() {
  systemData.error_bt.msg.clear();
}

void putBatteryError(String event) {
  putLog(systemData.error_bt, event.c_str());
}

void putBoilerError(String event) {
  putLog(systemData.error_bs, event.c_str());
}

void clearBoilerError() {
  systemData.error_bs.msg.clear();
}

void putLog(logEntry& log, const char* event) {
  log.msg.clear();
  log.msg.concat(event);
  log.ts = systemData.ts;
  Serial.printf("log %u: %s\n", systemData.ts, event);
}

void putEvent(const char* event) {
  putLog(systemData.events[systemData.eventIx++ % 8], event);
}

void putEvent(String event) {
  putEvent(event.c_str());
}

bool updateBoilerData() {
  static long lastUpdate = 0;

  long ms = millis();
  if (lastUpdate == 0 || ms > lastUpdate + BOILER_UPDATE_INTERVAL_MS) {
    lastUpdate = ms;
    if (lpb->update()) {

      boilder_t boilerData;

      lpb->getBoilerData(&boilerData);

      systemData.boiler_T_cur = boilerData.t_cur;
      systemData.boiler_T_nom = boilerData.t_nom;
      systemData.boiler_T_min = boilerData.t_min;
      systemData.boiler_T_max = boilerData.t_max;

      clearBoilerError();
    } else {
      putBoilerError("Could not update boiler data.");
      return false;
    }
  }
  return true;  // no new data, so still ok
}

// main loop
void loop() {
  if (doUpdateFlag) {

    uint32_t heap = ESP.getFreeHeap();

    bool validData =
      ensureConnected() && updateSystemData() && updateSolarForecast() && updateBoilerData();

    updateSwitch(validData);

    Serial.printf("ESP Heap %uk/%uk valid: %d\n", heap >> 10, ESP.getFreeHeap() >> 10, validData);

    doUpdateFlag = false;
  }
  server.handleClient();

#ifdef ESP32
  //esp_task_wdt_reset();
#elif defined(ESP8266)
  ESP.wdtFeed();
#endif
}

void buildInLED(bool onOff) {
  short s = onOff ? 0 : 1;
  digitalWrite(LED_BUILTIN, s);
}

void statusLED(int status) {
  int d = 1000 - (100 * (status % 10));
  for (int i = 0; i < status; i++) {
    buildInLED(true);
    delay(d);
    buildInLED(false);
    delay(d);
  }
}

bool ensureConnected() {
  wl_status_t status = WiFi.status();
  if (status != WL_CONNECTED) {
    Serial.print("Connecting");
    for (int i = 0; i < 8; i++) {
      statusLED(0);
      Serial.print(".");
      delay(500);
    }
    Serial.println();

    status = WiFi.status();
    statusLED(status);
    if (status == WL_CONNECTED) {
      Serial.printf("Connected. IP address: %s status: %d\n", WiFi.localIP().toString().c_str(), status);
    } else {
      Serial.printf("Wifi Error: %d\n", status);
    }
  }
  return status == WL_CONNECTED;
}

bool fetchData(String uri, JsonDocument& doc) {

  bool r = false;

  RestClient restClient;

  if (strlen(config.sonnenHostname) && strlen(config.sonnenApiToken)) {
    char url[128];
    snprintf(url, sizeof(url), "http://%.31s/%s/%s", config.sonnenHostname, SONNEN_API_URI, uri.c_str());
    r = restClient.fetch(String(url), doc, "auth-token", config.sonnenApiToken);
    if (!r) {
      putBatteryError(restClient.lastError());
    }
  } else {
    putBatteryError("Sonnen Battery not properly configured!");
  }
  return r;
}

bool updateSystemData() {

  JsonDocument json;

  if (systemData.inv_max_w == -1) {
    if (fetchData(SONNEN_API_CONFIGURATIONS, json)) {
      systemData.inv_max_w = json["IC_InverterMaxPower_w"].as<int>();
    } else {
      Serial.printf("ERROR: fetchSystemData(%s)\n", SONNEN_API_CONFIGURATIONS);
      return false;
    }
    Serial.printf("IC_InverterMaxPower_w %d\n", systemData.inv_max_w);
  }

  if (systemData.cap_bat_max_Wh == 0) {
    if (fetchData(SONNEN_API_LATEST_DATA, json)) {
      systemData.cap_bat_max_Wh = json["FullChargeCapacity"].as<uint16_t>();
    } else {
      Serial.printf("ERROR: fetchSystemData(%s)\n", SONNEN_API_LATEST_DATA);
      return false;
    }
    Serial.printf("FullChargeCapacity %d\n", systemData.cap_bat_max_Wh);
  }

  if (fetchData(SONNEN_API_STATUS, json)) {

    systemData.usoc = json["USOC"].as<uint8_t>();
    systemData.cap_bat_Wh = systemData.cap_bat_max_Wh * systemData.usoc / 100;
    systemData.gridFeedIn_W = json["GridFeedIn_W"].as<int>();
    systemData.prod_W = json["Production_W"].as<uint16_t>();
    systemData.cons_W = json["Consumption_W"].as<uint16_t>();
    systemData.cons_avg_W = json["Consumption_Avg"].as<uint16_t>();
    systemData.dischargeNotAllowed = json["dischargeNotAllowed"].as<bool>();
    systemData.charge = json["BatteryCharging"].as<short>() - json["BatteryDischarging"].as<short>();

    systemData.cons_W_rnd = (systemData.cons_W + 50) / 100 * 100;
    systemData.cons_W_norm = (systemData.switchEnabled && systemData.cons_W_rnd > config.loadPower_W) ? systemData.cons_W_rnd - config.loadPower_W : systemData.cons_W_rnd;  // consumption without load

    struct tm time;
    strptime(json["Timestamp"].as<const char*>(), "%Y-%m-%d %H:%M:%S", &time);

    systemData.dstOffset = isDST(&time) ? dstOffset : 0;

    systemData.ts = mktime(&time) - stdOffset - systemData.dstOffset;
    systemData.tm_yday = time.tm_yday;

    clearBatteryError();
    return true;
  }
  return false;
}

bool updateSwitch(bool validData) {

  static uint8_t inverterLatencyCnt = 0;

  uint16_t hysteresis_Wh = config.loadPower_W / 12;  // Wh if load is switched on for 5min
  float temp_off = (systemData.boiler_T_max + systemData.boiler_T_nom) / 2 - 0.5;
  float temp_on = (systemData.boiler_T_max + systemData.boiler_T_nom) / 2 - BOILER_TEMPERATURE_DELTA;

  bool desiredState = validData
                      && ((systemData.switchEnabled && systemData.boiler_T_cur < temp_off)
                          || (!systemData.switchEnabled && systemData.boiler_T_cur < temp_on))
                      && (systemData.prod_W + (systemData.dischargeNotAllowed ? 0 : systemData.inv_max_w) - systemData.cons_W_rnd - (systemData.switchEnabled ? 0 : config.loadPower_W) > 0)  // aware of max system power (production + max inverter power)
                      && ((!systemData.switchEnabled && systemData.gridFeedIn_W > config.loadPower_W)                                                                                         // if surplus (waste) exceeds load
                          //|| (systemData.switchEnabled && systemData.gridFeedIn_W >= 0)                                                                                                             // if load enabled and still grid feed in
                          || (systemData.cap_bat_Wh > (config.cap_bat_min_Wh + hysteresis_Wh) && MAX(0, systemData.prod_W - systemData.cons_W_norm) > ((config.loadPower_W + (int)(config.loadPower_W * 0.1)) >> 1))  // if min cap is reached, but there is production already
                          || (systemData.dischargeNotAllowed == false && batteryCapacityTargetFulfilled(hysteresis_Wh)));                                                                                             // forecast battery capacity and be aware of discharge allowed


  if (systemData.switchEnabled != desiredState) {
    putEvent(String("switch ") + (desiredState ? "on" : "off") + ": ");
  }

  if (desiredState) {                                                                                   // on?
    if (systemData.switchEnabled) {                                                                     // already on?
      inverterLatencyCnt = (systemData.gridFeedIn_W < -config.gridMin_W) ? inverterLatencyCnt + 1 : 0;  // grid purchase active? (negative grid feed in denotes purchase)
      desiredState = inverterLatencyCnt <= SONNEN_INVERTER_LATENCY_COUNT;
      if (!desiredState) {
        putEvent(String("off - latency count ") + inverterLatencyCnt + "/" + SONNEN_INVERTER_LATENCY_COUNT);
      }
    } else {  // off, but on desired, reset latency counter
      inverterLatencyCnt = 0;
    }
  }

  systemData.switchEnabled = (desiredState && config.mode == 2) || (config.mode == 1);  // combine with mode

  Serial.printf("ts: %s (%u) usoc: %2d%% p/c: %d/%d/%d (W) avg: %d (Wh) grid: %d (W) mode %d: heater %d\n", toDate(systemData.ts), systemData.ts, systemData.usoc, systemData.prod_W, systemData.cons_W, systemData.cons_W, systemData.cons_avg_W, systemData.gridFeedIn_W, config.mode, systemData.switchEnabled);

  toggleSwitch(systemData.switchEnabled);

  return true;
}

void toggleSwitch(bool switchEnabled) {
  digitalWrite(PIN_SSR, switchEnabled ? HIGH : LOW);
  buildInLED(switchEnabled);
}

bool isDST(struct tm* timeinfo) {
  int year = timeinfo->tm_year + 1900;

  struct tm lastMarchSunday;
  lastMarchSunday.tm_min = 0;
  lastMarchSunday.tm_sec = 0;
  lastMarchSunday.tm_year = year - 1900;
  lastMarchSunday.tm_mon = 2;  // März
  lastMarchSunday.tm_mday = 31;
  lastMarchSunday.tm_hour = 2;
  mktime(&lastMarchSunday);
  lastMarchSunday.tm_mday -= lastMarchSunday.tm_wday;

  struct tm lastOctoberSunday;
  lastOctoberSunday.tm_min = 0;
  lastOctoberSunday.tm_sec = 0;
  lastOctoberSunday.tm_year = year - 1900;
  lastOctoberSunday.tm_mon = 9;
  lastOctoberSunday.tm_mday = 31;
  lastOctoberSunday.tm_hour = 3;
  mktime(&lastOctoberSunday);
  lastOctoberSunday.tm_mday -= lastOctoberSunday.tm_wday;

  time_t now = mktime(timeinfo);
  return (now >= mktime(&lastMarchSunday) && now < mktime(&lastOctoberSunday));
}

static char* toLocalDate(uint32_t utc_ts) {
  return toDate(utc_ts, (stdOffset + systemData.dstOffset));
}

static char* toDate(uint32_t utc_ts) {
  return toDate(utc_ts, 0);
}

static char tsfmt[20];

static char* toDate(uint32_t utc_ts, uint16_t offset) {
  time_t time = (time_t)(utc_ts + offset);
  tm* timeinfo = gmtime(&time);
  strftime(tsfmt, sizeof(tsfmt), "%Y-%m-%d %H:%M:%S", timeinfo);
  return tsfmt;
}

bool batteryCapacityTargetFulfilled(uint16_t hysteresis_Wh) {

  if (systemData.pv_forecast_wh_h[0][0] == 0) {
    return false;  // no solar forecast data, assume battery will become empty
  }

  uint32_t ts = systemData.ts - (systemData.ts % 3600);  // start timestamp of last full hour

  bool foundPvData = false;
  uint32_t cap_bat_Wh = systemData.cap_bat_Wh;

  for (uint8_t i = 0; i < sizeof(systemData.pv_forecast_wh_h) / sizeof(systemData.pv_forecast_wh_h[0]); i++) {

    if ((foundPvData = systemData.pv_forecast_wh_h[i][0] == ts)) {  // seek to pv forecast upon system ts

      uint32_t wh = (ts + 3600 - systemData.ts) * systemData.pv_forecast_wh_h[i][1] / 3600;  // remaining pv production in this hour

      for (i++; i < sizeof(systemData.pv_forecast_wh_h) / sizeof(systemData.pv_forecast_wh_h[0]); i++) {

        if (cap_bat_Wh < config.cap_bat_min_Wh) {  // capacity below expected min capacity
          putEvent("min capacity at " + String(toLocalDate(ts)));
          return false;
        }
        if (cap_bat_Wh == systemData.cap_bat_max_Wh) {
          putEvent("max capacity at " + String(toLocalDate(ts)));
          return true;
        }
        // cumulate battery capacity upon production forecast
        cap_bat_Wh = MIN(systemData.cap_bat_max_Wh, (uint16_t)MAX(0, (int)cap_bat_Wh + MIN(systemData.inv_max_w, (int)wh - systemData.cons_W_norm)));
        Serial.printf("%d => %u (s) %s %u (Wh) cap_bat %u (Wh) usoc: %u%%\n", i, ts, toDate(ts), wh, cap_bat_Wh, cap_bat_Wh * 100 / systemData.cap_bat_max_Wh);

        ts = systemData.pv_forecast_wh_h[i][0];
        wh = systemData.pv_forecast_wh_h[i][1];
      }
    }
  }

  putEvent(String("capacity ") + cap_bat_Wh + "Wh " + hysteresis_Wh + "Wh (hys) at " + toLocalDate(ts));
  return foundPvData && cap_bat_Wh >= (uint32_t)(config.cap_bat_min_Wh + (systemData.switchEnabled ? 0 : hysteresis_Wh));
}

bool loadConfig() {
  if (!LittleFS.exists(CONFIGFILE)) {
    Serial.println("Config file not found");

    return false;
  }

  File f = LittleFS.open(CONFIGFILE, "r");
  if (!f) {
    Serial.printf("Could not open config file %s\n", CONFIGFILE);

    return false;
  }

  JsonDocument json;
  DeserializationError error = deserializeJson(json, f);
  f.close();

  if (error) {
    return false;
  }

  Serial.println("Config loaded:");
  serializeJsonPretty(json, Serial);

  jsonToConfig(json);

  return true;
}

bool saveConfig() {
  File f = LittleFS.open(CONFIGFILE, "w");
  if (!f) {
    Serial.printf("Could not open config file %s for writing\n", CONFIGFILE);
    return false;
  }
  JsonDocument json;
  configToJson(json);
  Serial.print("saveConfig() ");
  serializeJsonPretty(json, Serial);
  serializeJson(json, f);
  f.close();
  saveConfigFile = false;
  return true;
}
