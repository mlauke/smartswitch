/**
 * TODOs:
 * - detect accu maintenance mode - full charged, but not used if no production => http://192.168.188.36/api/v2/latestdata "Setpoint Priority": Full Charge Request": true|false
 * - weather forecast and tracking
 */
#include "SmartSwitch.h"
#include "GithubOTA.h"
#include "RestClient.h"

// echo "const char index_html[] PROGMEM = { $(gzip -9 -c nginx/index.html | hexdump -v -e '1/1 "0x%02X, "') };" > src/smartswitch.esp8266/index_html.h &&
// echo "const char app_js[] PROGMEM = { $(gzip -9 -c nginx/app.js | hexdump -v -e '1/1 "0x%02X, "') };" > src/smartswitch.esp8266/app_js.h

#include "app_js.h"
#include "index_html.h"

#define SERIAL_BAUDRATE 115200
#define WEBSERVER_PORT 80

#define PIN_SSR 5  // GPIO 5 (D1)

#define SONNEN_API_URI "api/v2"
#define SONNEN_API_CONFIGURATIONS "configurations"
#define SONNEN_API_LATEST_DATA "latestdata"
#define SONNEN_API_STATUS "status"

#define URL_LOCATION "http://ip-api.com/json/"

#define SOLAR_FORECAST_INTERVAL 10 * 60 * 1000  //every 10min
#define URL_SOLAR_FORECAST "http://api.forecast.solar/estimate/watthours/period/%.4f/%.4f/%d/%d/%.2f?time=seconds&no_sun=0&full=1"

static WiFiManager wifiManager;
static ESP8266WebServer server(WEBSERVER_PORT);
static Ticker timer;
static volatile bool doUpdateFlag = false;
static systemDataStruct systemData;
static configStruct config;
static bool saveConfigFile = false;

int stdOffset = 3600;  // 1h utc offset
int dstOffset = 3600;  // 1h suummer time"offset

void saveConfigCallback() {
  saveConfigFile = true;
}

#define HOSTNAME "smartswitch"
#define RELEASE_TAG "v000"

void setup() {

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(GPIO_ID_PIN(PIN_SSR), OUTPUT);

  toggleSwitch(false);

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  configDefaults();
  systemDefaults();

  Serial.begin(SERIAL_BAUDRATE);

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

  updateLocation();

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
  server.on("/api/data", handleData);
  server.on("/api/status", handleStatus);
  server.on("/api/update", handleAPI);
  server.onNotFound(handleNotFound);

  ESPhttpUpdate.onStart(onOTABegin);
  ESPhttpUpdate.onProgress(onOTAProgress);
  if (config.update_startup) {
    handleGithubUpdate();
  }

  server.begin();  // Actually start the server
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
  setConfigStr(config, hostname, HOSTNAME);
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
          Serial.printf("ERROR: overflow %d => %u %u ", i, ts, wh);
        }
      }
      Serial.println();
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

void commonHeader(size_t contentLength, const char* ctype) {
  server.sendHeader("cache-control", "max-age=31536000, must-revalidate");
  server.setContentLength(contentLength);
  server.sendHeader("content-encoding", "gzip");
  server.send(200, ctype, "");
}

void handleAppJs() {
  commonHeader(app_js_length, "text/javascript");
  server.client().write_P(app_js, sizeof(app_js));
}

void handleRoot() {
  commonHeader(index_html_length, "text/html;charset=utf-8");
  server.client().write_P(index_html, sizeof(index_html));
}

void changeHostname(const char* newHostname) {
  WiFi.disconnect();
  while (WiFi.status() == WL_CONNECTED) {
    delay(100);
  }
  WiFi.hostname(newHostname);
  WiFi.reconnect();
  while (!ensureConnected()) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("Reconnected! New Hostname: " + WiFi.hostname());
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

  data["loc"] = config.location;
  data["tz"] = config.tz;

  data["bs_t_max"] = config.boiler_T_max;
  data["bs_t_nom"] = config.boiler_T_nom;
}

void sendJson(JsonDocument& json) {

  String jsonString;

  size_t r = serializeJsonPretty(json, jsonString);
  Serial.printf("handleData() json (%d) %s\n", r, jsonString.c_str());

  server.sendHeader("cache-control", "no-cache");
  server.send(200, "application/json", jsonString);
}

void handleData() {

  JsonDocument data;

  configToJson(data);
  sendJson(data);
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

  data["events"] = systemData.events;

  sendJson(data);
}

void handleAPI() {
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
    saveConfig();

  } else if (server.hasArg("reset")) {
    if (server.arg("reset").toInt()) {
      configDefaults();
      saveConfig();
      wifiManager.resetSettings();  // reset wifi settings
      restart();
    }
  } else if (server.hasArg("update")) {
    handleGithubUpdate();
    return;
  }

  server.sendHeader("location", "/");
  server.send(303);
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
  snprintf(buffer, sizeof(buffer), "<html lang='en'><head><meta http-equiv='refresh' content='10;url=/'></head><body><p>Update found, Going to install Release %s</p></body></html>", gh_updater.release_tag);
  server.send(200, "text/plain", buffer);
  DEBUG(buffer);

  if (gh_updater.doUpdate()) {
    setConfigStr(config, release_tag, gh_updater.release_tag);
    if (!saveConfig()) {
      Serial.println("Error saving config");
      systemData.events.concat("Error saving config\n");
      return;
    }
    Serial.println("config saved.");
    restart();
  }
  systemData.events.concat(gh_updater.getUpdateError() + "\n");
}

// main loop
void loop() {
  if (doUpdateFlag) {

    uint32_t heap = ESP.getFreeHeap();

    bool r =
      ensureConnected() && updateSystemData() && updateSolarForecast() && updateSwitch();

    Serial.printf("ESP Heap %uk/%uk r: %d\n", heap >> 10, ESP.getFreeHeap() >> 10, r);

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
  RestClient restClient;

  if (config.sonnenHostname && strlen(config.sonnenHostname) && config.sonnenApiToken && strlen(config.sonnenApiToken)) {
    char url[128];
    snprintf(url, sizeof(url), "http://%.31s/%s/%s", config.sonnenHostname, SONNEN_API_URI, uri.c_str());
    return restClient.fetch(String(url), doc, "auth-token", config.sonnenApiToken);
  }
  return false;
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
      Serial.printf("ERROR: fetchSystemData(%s) json is undefined\n", SONNEN_API_LATEST_DATA);
      return false;
    }
    Serial.printf("FullChargeCapacity %d\n", systemData.cap_bat_max_Wh);
  }

  if (fetchData(SONNEN_API_STATUS, json)) {

    //int rsoc = (int)json["RSOC"];
    //    int cap = (int)json["FullChargeCapacity"];// battery full charge
    systemData.usoc = json["USOC"].as<uint8_t>();  //
                                                   //  int ucap = (int)(cap * usoc / (float)100);
    systemData.gridFeedIn_W = json["GridFeedIn_W"].as<int>();
    systemData.prod_W = json["Production_W"].as<uint16_t>();
    systemData.cons_W = json["Consumption_W"].as<uint16_t>();  // Consumption_Avg or Consumption_W % 100 more "real time"
    systemData.cons_avg_W = median(json["Consumption_Avg"].as<uint16_t>());

    struct tm time;
    strptime(json["Timestamp"].as<const char*>(), "%Y-%m-%d %H:%M:%S", &time);

    systemData.ts = mktime(&time) - stdOffset - (isDST(&time) ? dstOffset : 0);
    systemData.tm_yday = time.tm_yday;
  }

  return true;
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

bool updateSwitch() {
  /*
    heater_on = (not(heater_on) && GridFeedIn_W > HEATER_POWER_MAX_W) ||
                 heater_on && GridFeedIn_W > Gin_thr(50) ||
  */
  systemData.switchEnabled = (systemData.gridFeedIn_W > -GRID_PURCHASE_W)  // grid purchase must be greater then threshold (negative grid feed in denotes purchase)
                             && ((!systemData.switchEnabled && systemData.gridFeedIn_W > config.loadPower_W)
                                 || (systemData.switchEnabled && systemData.gridFeedIn_W > 0)
                                 || (forecastBatteryCapacityWh() >= config.cap_bat_min_Wh));

  systemData.switchEnabled = (systemData.switchEnabled && (config.mode == 2)) || (config.mode == 1);  // with mode

  time_t ts = (time_t)systemData.ts;

  Serial.printf("ts: %s%u usoc: %2d%% p/c: %d/%d (W) avg: %d (Wh) grid: %d (W) mode %d: heater %d\n", asctime(gmtime(&ts)), systemData.ts, systemData.usoc, systemData.prod_W, systemData.cons_W, systemData.cons_avg_W, systemData.gridFeedIn_W, config.mode, systemData.switchEnabled);

  toggleSwitch(systemData.switchEnabled);

  return true;
}

void toggleSwitch(bool switchEnabled) {
  digitalWrite(GPIO_ID_PIN(PIN_SSR), switchEnabled ? HIGH : LOW);
  buildInLED(switchEnabled);
}

bool isDST(struct tm* timeinfo) {
  int year = timeinfo->tm_year + 1900;

  struct tm lastMarchSunday = { 0 };
  lastMarchSunday.tm_year = year - 1900;
  lastMarchSunday.tm_mon = 2;  // März
  lastMarchSunday.tm_mday = 31;
  lastMarchSunday.tm_hour = 2;
  mktime(&lastMarchSunday);
  lastMarchSunday.tm_mday -= lastMarchSunday.tm_wday;

  struct tm lastOctoberSunday = { 0 };
  lastOctoberSunday.tm_year = year - 1900;
  lastOctoberSunday.tm_mon = 9;
  lastOctoberSunday.tm_mday = 31;
  lastOctoberSunday.tm_hour = 3;
  mktime(&lastOctoberSunday);
  lastOctoberSunday.tm_mday -= lastOctoberSunday.tm_wday;

  time_t now = mktime(timeinfo);
  return (now >= mktime(&lastMarchSunday) && now < mktime(&lastOctoberSunday));
}

uint32_t forecastBatteryCapacityWh() {

  if (systemData.pv_forecast_wh_h[0][0] == 0) {  // no forecast data, cannot calculate forecast, assume battery will become empty
    Serial.println("<= no solar forcast");
    return 0;
  }

  uint32_t cap_bat_Wh = systemData.cap_bat_max_Wh * systemData.usoc / 100;

  uint32_t ts = systemData.ts - (systemData.ts % 3600);  // ts of last full hour

  for (uint8_t i = 0; i < sizeof(systemData.pv_forecast_wh_h) / sizeof(systemData.pv_forecast_wh_h[0]); i++) {

    if (systemData.pv_forecast_wh_h[i][0] == ts) {  //select pv forecast upon system ts

      uint32_t wh = (ts + 3600 - systemData.ts) * systemData.pv_forecast_wh_h[i][1] / 3600;  // pv production in this hour
      Serial.printf("%d => %u (s) %u (Wh) cap %u (Wh) %u%%\n", i, ts, wh, cap_bat_Wh, cap_bat_Wh * 100 / systemData.cap_bat_max_Wh);

      for (i++; i < sizeof(systemData.pv_forecast_wh_h) / sizeof(systemData.pv_forecast_wh_h[0]); i++) {

        cap_bat_Wh = MIN(systemData.cap_bat_max_Wh, MAX(0, (int32_t)(cap_bat_Wh + wh) - (int16_t)systemData.cons_avg_W));
        Serial.printf("%d => %u (s) %u (Wh) cap %u (Wh) %u%%\n", i, systemData.pv_forecast_wh_h[i][0], wh, cap_bat_Wh, cap_bat_Wh * 100 / systemData.cap_bat_max_Wh);

        if (cap_bat_Wh < config.cap_bat_min_Wh) {  // capacity below expected min capacity
          Serial.println("<= below min capacity");
          return cap_bat_Wh;
        }
        if (cap_bat_Wh == systemData.cap_bat_max_Wh) {
          Serial.println("<= max capacity reachable");
          return cap_bat_Wh;
        }
        wh = systemData.pv_forecast_wh_h[i][1];
      }
    }
  }
  return cap_bat_Wh;
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
