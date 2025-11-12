#ifndef _DEBUG_H
#define _DEBUG_H

#ifdef DEBUG_ENABLED
#if defined(ESP32) || defined(ESP8266)
#define DEBUG(...) Serial.printf(__VA_ARGS__)
#else
#include <stdio.h>
#define DEBUG(...) fprintf(stdout, __VA_ARGS__)
#endif
#else
#define DEBUG(...)
#endif

#endif