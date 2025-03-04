/**
 * TODOs:
 * - detect accu maintenance mode - full charged, but not used if no production => http://192.168.188.36/api/v2/latestdata "Setpoint Priority": Full Charge Request": true|false
 * - weather forecast and tracking
 *
 */
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <ElegantOTA.h>
#include <FS.h>
#include <Ticker.h>

#include "GithubOTA.h"
#include "RestClient.h"

// (cd nginx;xxd -i index.html | sed "s/=/PROGMEM =/g" | sed "s/unsigned/const/g") > src/smartswitch.esp8266/index_html.h && (cd nginx;xxd -i app.js | sed "s/=/PROGMEM =/g" | sed "s/unsigned/const/g") > src/smartswitch.esp8266/app_js.h

#include "app_js.h"
#include "index_html.h"

#define GRID_FEED_IN_MIN 50

#define SERIAL_BAUDRATE 115200
#define WEBSERVER_PORT 80

#define PIN_SSR 5  // GPIO 5 (D1)

#define SONNEN_API_URI "api/v2"
#define SONNEN_API_CONFIGURATIONS "configurations"
#define SONNEN_API_LATEST_DATA "latestdata";
#define SONNEN_API_STATUS "status"


#define STRING(x) #x
#define _cs(a) sizeof(((struct configStruct*)0)->a) - 1  // '\0' trailing zero
#define setConfigStr(a, b) \
  if (b) strncpy(config.a, b, _cs(a))

#define CFG_SZ_HOSTNAME 32
#define CFG_SZ_REL_TAG 4
#define CFG_SZ_SONNENHOST 32
#define CFG_SZ_SONNENTOKEN 36
#define CFG_SZ_LOCATION 64
#define CFG_SZ_TZ 32

int IC_InverterMaxPower_w = -1;

bool switchEnabled = false;

WiFiManager wifiManager;

ESP8266WebServer server(WEBSERVER_PORT);

Ticker timer;
volatile bool doUpdateFlag = false;

struct configStruct {
  char hostname[CFG_SZ_HOSTNAME + 1];
  char release_tag[CFG_SZ_REL_TAG + 1];
  char sonnenHostname[CFG_SZ_SONNENHOST + 1];
  char sonnenApiToken[CFG_SZ_SONNENTOKEN + 1];
  uint16_t loadPower_W;
  uint16_t gridMin_W;
  double lat;
  double lon;
  double kWp;   // installed PV power
  uint16_t az;  // Azimuth
  char location[CFG_SZ_LOCATION + 1];
  char tz[CFG_SZ_TZ + 1];
  uint8_t mode;  // 0 - off, 1 - on, 2 - automatic
  bool update_startup;
};

struct configStruct config;
bool saveConfigFile = false;

void saveConfigCallback() {
  saveConfigFile = true;
}

#define MY_HOSTNAME "SmartSwitch"
#define RELEASE_TAG "v000"

void setup() {

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(GPIO_ID_PIN(PIN_SSR), OUTPUT);

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  configDefaults();

  Serial.begin(SERIAL_BAUDRATE);

  Serial.println("Mounting FS...");
  if (SPIFFS.begin()) {
    if (!loadConfig()) {
      Serial.println("Error loading config");
    }
    Serial.printf("Config:\nHostname: %s\nRelease: %s\nSonnen Host/Token: %s/%s\n",
                  config.hostname,
                  config.release_tag,
                  config.sonnenHostname,
                  config.sonnenApiToken);
  } else {
    Serial.println("failed to mount FS. Attempting to format.");
    SPIFFS.format();
    Serial.println("format done.");

    saveConfig();
  }

  WiFiManagerParameter custom_hostname("hostname", "Hostname", config.hostname, CFG_SZ_HOSTNAME);
  wifiManager.addParameter(&custom_hostname);
  WiFiManagerParameter custom_sonnenHostname("sonnen", "Sonnen Host/IP", config.sonnenHostname, CFG_SZ_SONNENHOST);
  wifiManager.addParameter(&custom_sonnenHostname);
  WiFiManagerParameter custom_sonnenApiToken("sonnenApiToken", "Sonnen Api-Token", config.sonnenApiToken, CFG_SZ_SONNENTOKEN);
  wifiManager.addParameter(&custom_sonnenApiToken);

  wifiManager.autoConnect("SmartSwitchAP");

  updateLocation();

  if (saveConfigFile) {
    String lcHostname = String(custom_hostname.getValue());
    lcHostname.toLowerCase();
    setConfigStr(hostname, lcHostname.c_str());
    setConfigStr(sonnenHostname, custom_sonnenHostname.getValue());
    setConfigStr(sonnenApiToken, custom_sonnenApiToken.getValue());
    if (!saveConfig()) {
      Serial.println("Error saving config");
    }
  }

  if (config.update_startup) {
    handleGithubUpdate();
  }

  WiFi.hostname(config.hostname);
  MDNS.begin(config.hostname);

  server.on("/", handleRoot);
  server.on("/app.js", handleAppJs);
  server.on("/api/data", handleData);
  server.on("/api/update", handleAPI);
  server.onNotFound(handleNotFound);

  ESPhttpUpdate.onStart(onOTABegin);
  ElegantOTA.onStart(onOTABegin);
  ElegantOTA.onProgress(onOTAProgress);
  ESPhttpUpdate.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  ElegantOTA.begin(&server);

  // httpUpdater.setup(&server);
  server.begin();  // Actually start the server
  Serial.println("HTTP server started");

  timer.attach(3, timerCallback);
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

void configDefaults() {  // init config struct with default values
  setConfigStr(hostname, MY_HOSTNAME);
  setConfigStr(release_tag, RELEASE_TAG);
  setConfigStr(sonnenHostname, "");
  setConfigStr(sonnenApiToken, "");
  config.lon = 0.0;
  config.lat = 0.0;
  setConfigStr(tz, "Europe/Berlin");
  setConfigStr(location, "");
  config.loadPower_W = 1000;
  config.gridMin_W = GRID_FEED_IN_MIN;
  config.mode = 2;
  config.update_startup = false;
}

void updateLocation() {

  if (config.lat == 0.0 && config.lon == 0.0) {

    RestClient restClient;
    DynamicJsonDocument doc(512);

    if ((saveConfigFile = restClient.fetch("http://ip-api.com/json/", doc))) {
      config.lon = doc["lon"].as<double>();
      config.lat = doc["lat"].as<double>();
      snprintf(config.location, CFG_SZ_LOCATION, "%s %s", doc["zip"].as<const char*>(), doc["city"].as<const char*>());
      setConfigStr(tz, doc["timezone"]);
      Serial.printf("Location: %f/%f - tz: %s loc: %s\n", config.lon, config.lat, config.tz, config.location);
    }
  }
}

void handleAppJs() {
  server.sendHeader("cache-control", "max-age=31536000");  // "ttl max, cache per release"
  server.send_P(200, "text/javascript", app_js, app_js_len);
}

void handleRoot() {
  server.sendHeader("cache-control", "max-age=180");
  server.send_P(200, "text/html", index_html, index_html_len);
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
  setConfigStr(release_tag, data["release_tag"]);

  config.update_startup = data["update_startup"];

  setConfigStr(hostname, data["hostname"]);

  setConfigStr(sonnenHostname, data["sn_host"]);
  setConfigStr(sonnenApiToken, data["sn_token"]);
  config.gridMin_W = data["sn_grdmin"].as<uint16_t>();
  config.loadPower_W = data["sn_loadpower"].as<uint16_t>();

  config.lon = data["lc_lon"].as<double>();
  config.lat = data["lc_lat"].as<double>();
  config.kWp = data["lc_kWp"].as<double>();
  config.az = data["lc_az"].as<uint16_t>();

  setConfigStr(location, data["loc"]);
  setConfigStr(tz, data["tz"]);
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

  data["lc_lon"] = config.lon;
  data["lc_lat"] = config.lat;
  data["lc_kWp"] = config.kWp;
  data["lc_az"] = config.az;
  
  data["loc"] = config.location;
  data["tz"] = config.tz;
}

void handleData() {

  JsonDocument data;

  configToJson(data);

  data["cons"] = "";
  data["prod"] = "";
  data["grid"] = "";
  data["usoc"] = "";
  data["switch"] = switchEnabled;

  String jsonString;
  size_t r = serializeJsonPretty(data, jsonString);
  Serial.printf("handleData() json (%d) %s\n", r, jsonString.c_str());

  server.sendHeader("cache-control", "no-cache");
  server.send(200, "application/json", jsonString);
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

  } else if (server.hasArg("sonnen")) {
    setConfigStr(sonnenHostname, server.arg("sn_host").c_str());
    setConfigStr(sonnenApiToken, server.arg("sn_token").c_str());
    config.gridMin_W = server.arg("sn_grdmin").toInt();
    config.loadPower_W = server.arg("sn_loadpower").toInt();
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
  }

  server.sendHeader("location", "/");
  server.send(303);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not found");  // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}

void restart() {
  SPIFFS.end();
  ESP.restart();
}

void handleGithubUpdate() {

  GithubOTA gh_updater(UPDATE_HOST, UPDATE_URL, UPDATE_TYPE, UPDATE_FILENAME);

  if (!gh_updater.checkUpdate(config.release_tag)) {
    server.send(404, "text/plain", "No Update found");
    return;
  }

  if (gh_updater.doUpdate()) {
    setConfigStr(release_tag, gh_updater.release_tag);
    if (!saveConfig()) {
      Serial.println("Error saving config");
      return;
    }
    Serial.println("config saved.");

    server.send(200, "text/plain", "Update done. Restart");

    restart();
  } else {
    server.send(500, "text/plain", gh_updater.getUpdateError());
  }
}

// main loop
void loop() {

  if (doUpdateFlag) {

    uint32_t heap = ESP.getFreeHeap();
    Serial.printf(">ESP Heap %uk\n", heap >> 10);

    if (ensureConnected()) {
      if (fetchSystemData()) {
        updateSwitch();
      } else {
        //statusLED(9);
      }
    }

    Serial.printf("<ESP Heap %uk/%uk\n", heap >> 10, ESP.getFreeHeap() >> 10);

    doUpdateFlag = false;
  }
  server.handleClient();
  ElegantOTA.loop();
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

bool fetchSystemData() {

  JsonDocument json;

  if (fetchData(SONNEN_API_CONFIGURATIONS, json)) {
    IC_InverterMaxPower_w = atoi((const char*)json["IC_InverterMaxPower_w"]);
  } else {
    Serial.printf("ERROR: fetchSystemData(%s)\n", SONNEN_API_CONFIGURATIONS);
  }
  Serial.printf("IC_InverterMaxPower_w %d\n", IC_InverterMaxPower_w);

  /*
  if (sendRequest(SONNEN_API_LATEST_DATA, json)) {
    FullChargeCapacity_Wh = atoi((const char*)json["FullChargeCapacity"]);
  } else {
    Serial.printf("ERROR: fetchSystemData(%s) json is undefined\n", SONNEN_API_CONFIGURATIONS);
  }
  Serial.printf("IC_InverterMaxPower_w %d\n", IC_InverterMaxPower_w);

*/
  return IC_InverterMaxPower_w > 0;
}

void updateSwitch() {

  /*
    heater_on = (not(heater_on) && GridFeedIn_W > HEATER_POWER_MAX_W) ||
                 heater_on && GridFeedIn_W > Gin_thr(50) ||

                 battery usoc
                 time

  */
  // Pac_total_W

  JsonDocument json;
  if (fetchData(SONNEN_API_STATUS, json)) {

    bool BatteryCharging = (bool)json["BatteryCharging"];
    bool BatteryDischarging = (bool)json["BatteryDischarging"];
    //int rsoc = (int)json["RSOC"];
    //    int cap = (int)json["FullChargeCapacity"];// battery full charge
    int usoc = (int)json["USOC"];  //
                                   //  int ucap = (int)(cap * usoc / (float)100);
    int GridFeedIn_W = (int)json["GridFeedIn_W"];
    int prod_w = (int)json["Production_W"];
    int cons_w = ((int)json["Consumption_W"] + 50) / 100 * 100;  // Consumption_Avg or Consumption_W % 100 more "real time"

    switchEnabled = (!switchEnabled && GridFeedIn_W > config.loadPower_W) || (switchEnabled && GridFeedIn_W > config.gridMin_W);
    //    switchEnabled = (deltaP >= 0 ? 1 : 0);

    switchEnabled = switchEnabled && (config.mode == 2) || (config.mode == 1);

    Serial.printf("Time %s: usoc: %2d%% p/c: %d/%d grid: %d switch (mode) %d: %d\n", (const char*)json["Timestamp"], usoc, prod_w, cons_w, GridFeedIn_W, config.mode, switchEnabled);

    digitalWrite(GPIO_ID_PIN(PIN_SSR), switchEnabled ? HIGH : LOW);
    buildInLED(switchEnabled);
  }
}


bool loadConfig() {
  if (!SPIFFS.exists(CONFIGFILE)) {
    Serial.println("Config file not found");

    return false;
  }

  File f = SPIFFS.open(CONFIGFILE, "r");
  if (!f) {
    Serial.printf("Could not open config file %s\n", CONFIGFILE);

    return false;
  }

  JsonDocument json;
  deserializeJson(json, f);
  f.close();

  jsonToConfig(json);

  return true;
}

bool saveConfig() {
  File f = SPIFFS.open(CONFIGFILE, "w");
  if (!f) {
    Serial.printf("Could not open config file %s for writing\n", CONFIGFILE);
    return false;
  }
  JsonDocument json;
  configToJson(json);
  Serial.print("saveConfig()");
  serializeJsonPretty(json, Serial);
  serializeJson(json, f);
  f.close();
  saveConfigFile = false;
  return true;
}
