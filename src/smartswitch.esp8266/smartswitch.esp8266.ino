/**
 * TODOs:
 * - local time on event level
 * - 
 */
#include "SmartSwitch.h"
#include "GithubOTA.h"
#include "RestClient.h"

// echo -e "const char index_html[] PROGMEM = { $(gzip -9 -c nginx/index.html | hexdump -v -e '1/1 "0x%02X, "') };" > src/smartswitch.esp8266/index_html.h && \
   echo -e "const char app_js[] PROGMEM = { $(gzip -9 -c nginx/app.js | hexdump -v -e '1/1 "0x%02X, "') };" > src/smartswitch.esp8266/app_js.h && \
   echo "const char app_css[] PROGMEM = { $(gzip -9 -c nginx/app.css | hexdump -v -e '1/1 "0x%02X, "') };" > src/smartswitch.esp8266/app_css.h

#include "app_js.h"
#include "app_css.h"
#include "index_html.h"

static WiFiManager wifiManager;
static ESP8266WebServer server(WEBSERVER_PORT);
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
#define RELEASE_TAG "v000"

void setup() {
  Serial.begin(SERIAL_BAUDRATE);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(GPIO_ID_PIN(PIN_SSR), OUTPUT);

  toggleSwitch(false);

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  configDefaults();
  systemDefaults();

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

  WiFiManagerParameter custom_hostname("hostname", "Hostname", config.hostname, CFG_SZ_HOSTNAME);
  wifiManager.addParameter(&custom_hostname);
  wifiManager.autoConnect("SmartSwitchAP");
  wifiManager.setConfigPortalTimeout(8);

  if (saveConfigFile) {
    String lcHostname = String(custom_hostname.getValue());
    lcHostname.toLowerCase();
    setConfigStr(config, hostname, lcHostname.c_str());
    if (!saveConfig()) {
      Serial.println("Error saving config");
    }
  }

  WiFi.hostname(config.hostname);
  MDNS.begin(config.hostname);

  server.on("/", handleRoot);
  server.on("/app.js", handleAppJs);
  server.on("/app.css", handleAppCss);
  server.on("/api/data", handleData);
  server.on("/api/status", handleStatus);
  server.on("/api/update", handleAPI);
  server.onNotFound(handleNotFound);

  updateLocation();

  ESPhttpUpdate.onStart(onOTABegin);
  ESPhttpUpdate.onProgress(onOTAProgress);
  if (config.update_startup) {
    handleGithubUpdate();
  }

  server.begin();  // Actually start the server
  server.keepAlive(false);
  Serial.println("HTTP server started");

  timer.attach_ms(SYSTEM_UPDATE_INTERVAL, timerCallback);
}

void timerCallback() {
  doUpdateFlag = true;
}

void onOTAProgress(size_t current, size_t final) {
  static long ota_progress_millis = 0;
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTABegin() {
  timer.detach();
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
    return;
  }
  Serial.println("There was an error during OTA update!");
}

void systemDefaults() {
  systemData.pv_forecast_ts = 0;
  memset(systemData.pv_forecast_wh_h, 0, sizeof(systemData.pv_forecast_wh_h));
  memset(systemData.cons_avg_W_h, 0, sizeof(systemData.cons_avg_W_h));
}

void configDefaults() {  // init config struct with default values
  setConfigStr(config, hostname, WiFi.hostname().c_str());
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
  config.gridMin_W = 50;
  config.mode = 0;  //initial set to off
  config.update_startup = false;

  config.boiler_T_max = 65;
  config.boiler_T_nom = 50;
}

bool updateSolarForecast() {

  long ms = millis();
  if (config.lat != 0.0 && config.lon != 0.0 && (systemData.pv_forecast_ts == 0 || (systemData.pv_forecast_ts + SOLAR_FORECAST_INTERVAL) < ms)) {

    RestClient restClient;
    JsonDocument doc;

    char url[128];
    snprintf(url, sizeof(url), URL_SOLAR_FORECAST, config.lat, config.lon, config.az, config.dec, config.kWp);

    if (restClient.fetch(String(url), doc)) {

      serializeJsonPretty(doc, Serial);
      uint8_t i = 0;
      for (JsonPair entry : doc["result"].as<JsonObject>()) {
        uint32_t ts = strtoul(entry.key().c_str(), NULL, 10);
        uint32_t wh = entry.value().as<uint32_t>();
        Serial.printf("%u %u ", ts, wh);
        if (i < sizeof(systemData.pv_forecast_wh_h) / sizeof(systemData.pv_forecast_wh_h[0])) {
          systemData.pv_forecast_wh_h[i][0] = ts;
          systemData.pv_forecast_wh_h[i][1] = wh;
          i++;
        } else {
          putEvent("WARN: overflow " + i);
        }
      }
      clearLocationError();
    } else {
      putLocationError("WARN solar forcast " + restClient.lastError());
    }
    systemData.pv_forecast_ts = ms;  // update timestamp
  }
  return true;
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

  config.boiler_T_max = data["bs_t_max"].as<uint8_t>();
  config.boiler_T_nom = data["bs_t_nom"].as<uint8_t>();
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

  data["bs_t_max"] = config.boiler_T_max;
  data["bs_t_nom"] = config.boiler_T_nom;
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
  data["switch"] = systemData.switchEnabled;
  data["bs_t_cur"] = systemData.boiler_T_cur;

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
    config.boiler_T_max = server.arg("bs_t_max").toInt();
    config.boiler_T_nom = server.arg("bs_t_nom").toInt();
    saveConfig();

  } else if (server.hasArg("sonnen")) {
    setConfigStr(config, sonnenHostname, server.arg("sn_host").c_str());
    setConfigStr(config, sonnenApiToken, server.arg("sn_token").c_str());
    config.gridMin_W = server.arg("sn_grdmin").toInt();
    config.loadPower_W = server.arg("sn_loadpower").toInt();
    config.cap_bat_min_Wh = server.arg("sn_cap_min").toInt();
    saveConfig();

  } else if (server.hasArg("location")) {
    config.lon = server.arg("lc_lon").toDouble();
    config.lat = server.arg("lc_lat").toDouble();
    config.kWp = server.arg("lc_kWp").toDouble();
    config.az = server.arg("lc_az").toInt();
    config.dec = server.arg("lc_dec").toInt();
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
  server.client().flush();

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
    server.send(404, "text/plain", "No Update found");
    return;
  }

  char buffer[256];
  snprintf(buffer, sizeof(buffer), "<html lang='en'><head><meta http-equiv='refresh' content='30;url=/'></head><body><p>Update found, Going to install Release %s</p></body></html>", gh_updater.release_tag.c_str());
  server.send(200, "text/html;charset=utf-8", buffer);
  DEBUG(buffer);

  if (gh_updater.doUpdate()) {
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

void putLog(logEntry& log, const char* event) {
  log.msg.clear();
  log.msg.concat(event);
  log.ts = systemData.ts;
  Serial.printf("%u %s\n", systemData.ts, event);
}

void putLog(logEntry* log, uint8_t* ix, const char* event) {
  logEntry entry = log[(*ix)++ % 8];
  putLog(entry, event);
}

void putEvent(const char* event) {
  putLog(systemData.events, &systemData.eventIx, event);
}

void putEvent(String event) {
  putEvent(event.c_str());
}

// main loop
void loop() {
  if (doUpdateFlag) {

    uint32_t heap = ESP.getFreeHeap();

    bool validData =
      ensureConnected() && updateSystemData() && updateSolarForecast();

    updateSwitch(validData);

    Serial.printf("ESP Heap %uk/%uk valid: %d\n", heap >> 10, ESP.getFreeHeap() >> 10, validData);

    doUpdateFlag = false;
  }
  server.handleClient();
  //ElegantOTA.loop();
}

void buildInLED(bool onOff) {
  short s = ((onOff ^ 0x1) & 0x01);
  digitalWrite(LED_BUILTIN, s);
}

void statusLED(int status) {
  int d = 1000 - (100 * (status % 10));
  for (int i = 0; i < status; i++) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(d);
    digitalWrite(LED_BUILTIN, HIGH);
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

    status = WiFi.status();  // refresh
    statusLED(status);
    if (status == WL_WRONG_PASSWORD) {
      Serial.printf("wrong password: status: %d\n", status);
    } else if (status == WL_CONNECTED) {
      Serial.printf("Connected. IP address: %s status: %d\n", WiFi.localIP().toString().c_str(), status);
    }
  }
  return status == WL_CONNECTED;
}

bool fetchData(String uri, JsonDocument& doc) {

  bool r = false;

  RestClient restClient;

  if (config.sonnenHostname && strlen(config.sonnenHostname) && config.sonnenApiToken && strlen(config.sonnenApiToken)) {
    char url[128];
    snprintf(url, sizeof(url), "http://%.31s/%s/%s", config.sonnenHostname, SONNEN_API_URI, uri.c_str());
    r = restClient.fetch(String(url), doc, "auth-token", config.sonnenApiToken);
    if (!r) {
      putBatteryError(restClient.lastError());
    }
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
    systemData.gridFeedIn_W = json["GridFeedIn_W"].as<int>();
    systemData.prod_W = json["Production_W"].as<uint16_t>();
    systemData.cons_W = json["Consumption_W"].as<uint16_t>();
    systemData.cons_avg_W = median(systemData.cons_W);
    systemData.dischargeNotAllowed = json["dischargeNotAllowed"].as<bool>();

    struct tm time;
    strptime(json["Timestamp"].as<const char*>(), "%Y-%m-%d %H:%M:%S", &time);

    systemData.ts = mktime(&time) - stdOffset - (isDST(&time) ? dstOffset : 0);
    systemData.tm_yday = time.tm_yday;

    clearBatteryError();
    return true;
  }
  return false;
}

uint16_t median(uint16_t cons_W) {
  static uint16_t ix = 0;

  uint32_t cons_avg_W = 0;
  uint16_t cnt = 0;

  systemData.cons_avg_W_h[ix++ % (sizeof(systemData.cons_avg_W_h) / sizeof(uint16_t))] = cons_W;
  // Serial.println("avg w:");
  for (uint16_t i = 0; i < sizeof(systemData.cons_avg_W_h) / sizeof(uint16_t); i++) {
    //    Serial.printf("%u ", systemData.cons_avg_W_h[i]);
    if (systemData.cons_avg_W_h[i]) {  //only > 0 are considered, cause after restart/reset it will be empty
      cons_avg_W += systemData.cons_avg_W_h[i];
      cnt++;
    }
  }
  //  Serial.printf("avg w last h: %u %u => %uW\n", cnt, cons_avg_W, cons_avg_W / cnt);
  return cons_avg_W / cnt;
}

bool updateSwitch(bool validData) {

  static uint8_t inverterLatencyCnt = 0;

  bool desiredState = validData
                      && ((!systemData.switchEnabled && systemData.gridFeedIn_W > config.loadPower_W)
                          || (systemData.switchEnabled && systemData.gridFeedIn_W > 0)
                          || (systemData.dischargeNotAllowed == false && batteryCapacityTargetReachable()));

  if (desiredState) {                                              // on?
    if (systemData.switchEnabled) {                                // already on?
      if (systemData.gridFeedIn_W < -GRID_PURCHASE_THRESHOLD_W) {  // grid purchase active? (negative grid feed in denotes purchase)
        inverterLatencyCnt++;
      }
      desiredState = inverterLatencyCnt <= MAX(1, SONNEN_INVERTER_LATENCY / SYSTEM_UPDATE_INTERVAL);
      if (!desiredState) {
        putEvent(String("off - latency count ") + inverterLatencyCnt);
      }
    } else {  // off, but on desired, reset latency counter
      inverterLatencyCnt = 0;
    }
  }

  systemData.switchEnabled = (desiredState && config.mode == 2) || (config.mode == 1);  // combine with mode

  Serial.printf("ts: %s %u usoc: %2d%% p/c: %d/%d (W) avg: %d (Wh) grid: %d (W) mode %d: heater %d\n", toDate(systemData.ts), systemData.ts, systemData.usoc, systemData.prod_W, systemData.cons_W, systemData.cons_avg_W, systemData.gridFeedIn_W, config.mode, systemData.switchEnabled);

  toggleSwitch(systemData.switchEnabled);

  return true;
}

void toggleSwitch(bool switchEnabled) {
  digitalWrite(GPIO_ID_PIN(PIN_SSR), switchEnabled ? HIGH : LOW);
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

static char* toDate(uint32 utc_ts) {
  time_t time = (time_t)utc_ts;
  // TODO localtime
  char* str = asctime(gmtime(&time));
  char* p = strrchr(str, '\n');
  if (p != NULL) {
    *p = '\0';
  }
  return str;
}

bool batteryCapacityTargetReachable() {

  if (systemData.pv_forecast_wh_h[0][0] == 0) {
    putEvent("no solar forecast");
    return false;  // no solar forecast data, assume battery will become empty
  }

  uint32_t cap_bat_Wh = systemData.cap_bat_max_Wh * systemData.usoc / 100;

  uint32_t ts = systemData.ts - (systemData.ts % 3600);  // ts of last full hour

  for (uint8_t i = 0; i < sizeof(systemData.pv_forecast_wh_h) / sizeof(systemData.pv_forecast_wh_h[0]); i++) {

    if (systemData.pv_forecast_wh_h[i][0] == ts) {  //select pv forecast upon system ts

      uint32_t wh = (ts + 3600 - systemData.ts) * systemData.pv_forecast_wh_h[i][1] / 3600;  // remaining pv production in this hour

      for (i++; i < sizeof(systemData.pv_forecast_wh_h) / sizeof(systemData.pv_forecast_wh_h[0]); i++) {

        cap_bat_Wh = MIN(systemData.cap_bat_max_Wh, MAX(0, (int32_t)(cap_bat_Wh + wh) - (int16_t)systemData.cons_avg_W));

        Serial.printf("%d => %u (s) %s %u (Wh) cap_bat %u (Wh) usoc: %u%%\n", i, ts, toDate(ts), wh, cap_bat_Wh, cap_bat_Wh * 100 / systemData.cap_bat_max_Wh);

        if (cap_bat_Wh < config.cap_bat_min_Wh) {  // capacity below expected min capacity
          putEvent("min capacity reached at " + String(toDate(ts)));
          return false;
        }
        if (cap_bat_Wh == systemData.cap_bat_max_Wh) {
          putEvent("max capacity reached at " + String(toDate(ts)));
          return true;
        }
        ts = systemData.pv_forecast_wh_h[i][0];
        wh = systemData.pv_forecast_wh_h[i][1];
      }
    }
  }
  return cap_bat_Wh >= config.cap_bat_min_Wh;
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
  deserializeJson(json, f);
  f.close();

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
