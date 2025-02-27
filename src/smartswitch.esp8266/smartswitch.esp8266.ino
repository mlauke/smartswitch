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

#include "GithubOTA.h"
#include "RestClient.h"


#define WIFI_PWD "0802052978480893"
#define WIFI_SSID "snusnu2"

#define SONNEN_API_TOKEN "22bfd7df-8b3b-4e74-ae24-edfd0bc3f230"

#define CONSUMER_POWER_MAX_W 3300
#define GRID_FEED_IN_MIN 100

#define SERIAL_BAUDRATE 115200
#define WEBSERVER_PORT 80

#define PIN_SSR 5  // GPIO 5 (D1)

#define SONNEN_API_CONFIGURATIONS "configurations"
String SONNEN_API_LATEST_DATA = "latestdata";
#define SONNEN_API_STATUS "status"

const char homepage[] PROGMEM =
  "<html><head><title>Smartswitch</title></head><body>"
  "  <body><h1>Smartswitch</h1>"
  "  <p><form action=\"/api\" method=\"post\">"
  "  <fieldset><legend>Nixies</legend>"
  "  <input type=\"radio\" name=\"enabled\" value=\"1\" checked /><label>On</label>"
  "  <input type=\"radio\" name=\"enabled\" value=\"0\" /><label>Off</label>"
  "  <input type=\"submit\" value=\"Send\" />"
  "  </fieldset>"
  "  </form></p>"

  "  <p><form action=\"/api\" method=\"post\">"
  "  <fieldset><legend>Update-Check on startup</legend>"
  "  <input type=\"radio\" name=\"update_startup\" value=\"0\" /><label>no</label>"
  "  <input type=\"radio\" name=\"update_startup\" value=\"1\" checked /><label>yes</label>"
  "  <input type=\"submit\" value=\"Send\" />"
  "  </fieldset>"
  "  </form></p>"

  "  <p><form action=\"api\" method=\"post\" onSubmit=\"return confirm('Sicher?');\"><fieldset><legend>Update</legend>"
  "  <input type=\"submit\" value=\"Update\"/>"
  "  <input type=\"hidden\" name=\"update\" value=\"1\" />"
  "  </fieldset>"
  "  </form></p>"

  "  <p><a href=\"/update\">update</a></p>"

  "  <p><form action=\"api\" method=\"post\" onSubmit=\"return confirm('Sicher? WLAN Zugangsdaten werden gel&ouml;scht!');\" ><fieldset><legend>Reset</legend>"
  "  <input type=\"submit\" value=\"Reset\"/>"
  "  <input type=\"hidden\" name=\"reset\" value=\"1\" />"
  "  </fieldset>"
  "  </form></p>"

  "  <p><form action=\"/api\" method=\"post\">"
  "  <fieldset><legend>Update Sonnen</legend>"
  "  <input name=\"sn_host\" value=\"%s\">"
  "  <input name=\"sn_token\" value=\"%s\">"
  "  <input type=\"submit\" value=\"Send\" />"
  "  </fieldset>"
  "  </form></p>"


  "</body></html>";

int IC_InverterMaxPower_w = -1;

bool ConsumerEnabled = false;

WiFiManager wifiManager;

ESP8266WebServer server(WEBSERVER_PORT);

RestClient restClient;

struct configStruct {
  char hostname[32];
  char release_tag[4];
  char sonnenHostname[32];
  char sonnenApiToken[32];
  long loadPower_W;
  long gridFeedInMin;
  double longitude;
  double latitude;
  bool enabled;
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

  // init config struct with default values
  strncpy(config.hostname, MY_HOSTNAME, 31);
  strncpy(config.release_tag, RELEASE_TAG, 4);
  strncpy(config.sonnenApiToken, SONNEN_API_TOKEN, 31);
  config.longitude = 0.0;
  config.latitude = 0.0;
  config.loadPower_W = CONSUMER_POWER_MAX_W;
  config.gridFeedInMin = GRID_FEED_IN_MIN;
  config.enabled = true;
  config.update_startup = false;

  Serial.begin(SERIAL_BAUDRATE);
  Serial.println("Mounting FS...");

  if (SPIFFS.begin()) {
    if (!loadConfig(CONFIGFILE)) {
      Serial.println("Error loading config");
    }
    Serial.printf("Config loaded:\nHostname: %s\nRelease: %s\nSonnen Host/Token: %s/%s",
                  config.hostname,
                  config.release_tag,
                  config.sonnenHostname,
                  config.sonnenApiToken);
  } else {
    Serial.println("failed to mount FS. Attempting to format.");
    SPIFFS.format();
    Serial.println("format done.");

    saveConfig(CONFIGFILE);
  }

  WiFiManagerParameter custom_hostname("hostname", "Hostname", config.hostname, 32);
  wifiManager.addParameter(&custom_hostname);
  WiFiManagerParameter custom_sonnenHostname("sonnen", "Sonnen Host/IP", config.sonnenHostname, 32);
  wifiManager.addParameter(&custom_sonnenHostname);
  WiFiManagerParameter custom_sonnenApiToken("sonnenApiToken", "Sonnen Api-Token", config.sonnenApiToken, 32);
  wifiManager.addParameter(&custom_sonnenApiToken);

  wifiManager.autoConnect("SmartSwitch AP");

  if (saveConfigFile)  //save the custom parameters to FS
  {
    strncpy(config.hostname, custom_hostname.getValue(), 31);
    if (!saveConfig(CONFIGFILE)) {
      Serial.println("Error saving config");
    }
  }

  if (config.update_startup) {
    handleGithubUpdate();
  }

  WiFi.hostname(config.hostname);
  MDNS.begin(config.hostname);

  server.on("/", handleRoot);
  server.on("/api", handleAPI);
  server.on("/api/status", handleStatus);
  server.onNotFound(handleNotFound);

  //  ESPhttpUpdate.onStart(beginOTAUpdate);
  //ElegantOTA.onStart(beginOTAUpdate);

  ElegantOTA.onProgress(onOTAProgress);
  ESPhttpUpdate.onProgress(onOTAProgress);

  ElegantOTA.onEnd(onOTAEnd);

  ElegantOTA.begin(&server);

  // httpUpdater.setup(&server);
  server.begin();  // Actually start the server
  Serial.println("HTTP server started");

  while (true) {
    if (ensureConnected() && fetchSystemData()) {
      break;
    }
    statusLED(9);
  }
}

void onOTAProgress(size_t current, size_t final) {
  static long ota_progress_millis = 0;
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
    return;
  }

  Serial.println("There was an error during OTA update!");
}
void handleRoot() {
  char buffer[1024];
  sprintf(buffer, homepage, 
    config.sonnenHostname,
    config.sonnenApiToken,
    config.loadPower_W,
    config.gridFeedInMin,
config.longitude,
    config.latitude
  );  // Send HTTP status 200 (Ok) and send some text to the browser/client
  server.send(200, "text/html", buffer);
}

void handleStatus() {
  String r = "Switch: " + ConsumerEnabled;
  server.send(200, "text/plain", r);
}

void handleAPI() {

  if (server.hasArg("enabled")) {
    config.enabled = server.arg("enabled").toInt() & 1;
    Serial.printf("Enabled: %d\n", config.enabled);
    saveConfig(CONFIGFILE);

  } else if (server.hasArg("update_startup")) {
    config.update_startup = server.arg("update_startup").toInt();
    Serial.printf("Enabled: %d\n", config.update_startup);
    saveConfig(CONFIGFILE);

  } else if (server.hasArg("sn_host")) {
    strncpy(config.sonnenHostname, server.arg("sn_host").c_str(), 31);
    strncpy(config.sonnenApiToken, server.arg("sn_token").c_str(), 31);
    saveConfig(CONFIGFILE);

  } else if (server.hasArg("reset")) {
    if (server.arg("reset").toInt() == 1) {
      wifiManager.resetSettings();  // reset wifi settings
      ESP.restart();
    }

  } else if (server.hasArg("update")) {
    handleGithubUpdate();
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not found");  // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}

void handleGithubUpdate() {
  GithubOTA gh_updater(&restClient, UPDATE_HOST, UPDATE_URL, UPDATE_TYPE, UPDATE_FILENAME);

  if (!gh_updater.checkUpdate(config.release_tag)) {
    server.send(404, "text/plain", "No Update found");
    return;
  }

  if (gh_updater.doUpdate()) {
    strncpy(config.release_tag, gh_updater.release_tag, 4);  //update to new tag
    if (!saveConfig(CONFIGFILE)) {
      Serial.println("Error saving config");
      return;
    }
    Serial.println("config saved.");

    server.send(200, "text/plain", "Update done. Restart");

    ESP.restart();
  } else {
    server.send(500, "text/plain", gh_updater.getUpdateError());
  }
}

// main loop
void loop() {
  if (ensureConnected()) {
    updateSwitch();
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
  String url = String("http://") + config.sonnenHostname + "/api/v2/" + uri;
  return restClient.fetch(url, doc, "auth-token", config.sonnenApiToken);
}

bool fetchSystemData() {

  JsonDocument json;
  if (fetchData(SONNEN_API_CONFIGURATIONS, json)) {
    IC_InverterMaxPower_w = atoi((const char*)json["IC_InverterMaxPower_w"]);
  } else {
    Serial.printf("ERROR: fetchSystemData(%s) json is undefined\n", SONNEN_API_CONFIGURATIONS);
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

    ConsumerEnabled = !ConsumerEnabled && GridFeedIn_W > CONSUMER_POWER_MAX_W || ConsumerEnabled && GridFeedIn_W > GRID_FEED_IN_MIN;
    //    ConsumerEnabled = (deltaP >= 0 ? 1 : 0);

    Serial.printf("Time %s: usoc: %2d%% p/c: %d/%d grid: %d switch: %d\n", (const char*)json["Timestamp"], usoc, prod_w, cons_w, GridFeedIn_W, ConsumerEnabled);

    digitalWrite(GPIO_ID_PIN(PIN_SSR), ConsumerEnabled ? HIGH : LOW);
    buildInLED(ConsumerEnabled);
  }
}


bool loadConfig(const char* filename) {
  if (!SPIFFS.exists(filename)) {
    Serial.println("Config file not found");

    return false;
  }

  File binaryConfigFile = SPIFFS.open(filename, "r");
  if (!binaryConfigFile) {
    Serial.println("Could not open config file");

    return false;
  }

  binaryConfigFile.read((byte*)&config, sizeof(config));
  binaryConfigFile.close();

  return true;
}

bool saveConfig(const char* filename) {
  File binaryConfigFile = SPIFFS.open(filename, "w");
  if (!binaryConfigFile) {
    Serial.println("Could not open config file for writing");
    return false;
  }
  size_t r = binaryConfigFile.write((byte*)&config, sizeof(config));
  binaryConfigFile.close();
  saveConfigFile = false;
  return true;
}
