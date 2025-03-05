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

#ifdef DEBUG_ENABLED
#define DEBUG(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

#endif