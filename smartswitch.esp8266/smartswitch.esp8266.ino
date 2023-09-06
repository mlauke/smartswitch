#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <Arduino_JSON.h>

#define WIFI_PWD "0802052978480870"
#define WIFI_SSID "snusnu2"

#define SONNEN_API_TOKEN "22bfd7df-8b3b-4e74-ae24-edfd0bc3f230"
#define HEATER_POWER_MAX_W 3300

String SONNEN_API_URL = String("http://192.168.188.36/api/v2/");
String SONNEN_API_CONFIGURATIONS = String("configurations");
String SONNEN_API_LATEST_DATA = String("latestdata");
String SONNEN_API_STATUS = String("status");

int IC_InverterMaxPower_w = -1;

short HeaterSwitchEnabled = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  while (!Serial)
    ;
  Serial.println();

  pinMode(LED_BUILTIN, OUTPUT);

  wl_status_t status = WiFi.begin(WIFI_SSID, WIFI_PWD);
  WiFi.setAutoReconnect(true);

  while (true) {
    if (ensureConnected() && fetchInitData()) {
      break;
    }
    statusLED(9);
  }
}

void buildInLED(short onOff) {
  short s = ((onOff ^ 0x1) & 0x01);
  Serial.printf("led: %d\n", s);
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
      Serial.printf("Connected, IP address: %s status: %d\n", WiFi.localIP(), status);
    }
  }
  return status == WL_CONNECTED;
}

bool isDefined(JSONVar v) {
  return (JSON.typeof(v) != "undefined");
}

JSONVar sendRequest(String method) {

  WiFiClient client;
  HTTPClient http;

  JSONVar res;

  // Your Domain name with URL path or IP address with path
  String url = String(SONNEN_API_URL + method);

  http.begin(client, url);
  http.addHeader("auth-token", SONNEN_API_TOKEN);
  int httpResponseCode = http.GET();
  Serial.printf("url %s %d\n", url.c_str(), httpResponseCode);
  if (httpResponseCode == 200) {
    String payload = http.getString();
    res = JSON.parse(payload);
    if (!isDefined(res)) {
      Serial.println("parsing input failed!");
    }
  } else {
    Serial.printf("no data - code: %d %s\n", httpResponseCode, http.getString());
  }
  // Free resources
  http.end();

  return res;
}

bool fetchInitData() {

  JSONVar json = sendRequest(SONNEN_API_CONFIGURATIONS);
  if (isDefined(json)) {
    IC_InverterMaxPower_w = atoi((const char*)json["IC_InverterMaxPower_w"]);
  } else {
    Serial.printf("fetchInitData() json is undefined\n");
  }
  Serial.printf("IC_InverterMaxPower_w %d\n", IC_InverterMaxPower_w);
  return IC_InverterMaxPower_w > 0;
}


void updateData() {

  JSONVar json = sendRequest(SONNEN_API_STATUS);
  if (isDefined(json)) {
    int cap = (int)json["FullChargeCapacity"];
    int usoc = (int)json["USOC"];
    int ucap = (int)(cap * usoc / (float)100);
    int gridFeedIn = (int)json["GridFeedIn_W"];
    int prod_w = (int)json["Production_W"];
    int cons_w = (int)json["Consumption_Avg"];

    int deltaP = IC_InverterMaxPower_w + prod_w - (cons_w + (HEATER_POWER_MAX_W * (1 - HeaterSwitchEnabled)));

    HeaterSwitchEnabled = (deltaP >= 0 ? 1 : 0);

    Serial.printf("Time %s: usoc: %d p/c: %d/%d dP: %d heater:%d\n", (const char*)json["Timestamp"], usoc, prod_w, cons_w, deltaP, HeaterSwitchEnabled);
    buildInLED(HeaterSwitchEnabled);
  }
}

void loop() {
  if (ensureConnected()) {
    updateData();
  }
  delay(3000);
}
