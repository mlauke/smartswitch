#ifndef _DEBUG_H
#define _DEBUG_H

#ifdef DEBUG_ENABLED

#if defined(ESP8266)
#define DEBUG(str) Serial.print(PSTR(str))
#define DEBUGLN(str) Serial.println(PSTR(str))
#define DEBUGP(str) Serial.print(str)
#define DEBUGF(fmt, ...) Serial.printf(PSTR(fmt), __VA_ARGS__)
#elif defined(ESP32)
#define DEBUG(str) Serial.print(str)
#define DEBUGLN(str) Serial.println(str)
#define DEBUGP(str) Serial.print(str)
#define DEBUGF(fmt, ...) Serial.printf(fmt, __VA_ARGS__)
#else
#include <stdio.h>
#define DEBUG(fmt, ...) fprintf(stdout, fmt, __VA_ARGS__)
#define DEBUGLN(str) fprintf(stdout, "%s\n", str)
#define DEBUGP(str) fprintf(stdout, str)
#define DEBUGF(fmt, ...) fprintf(stdout, fmt, __VA_ARGS__)
#endif
#else
#define DEBUG(fmt, ...)
#define DEBUGLN(str)
#define DEBUGP(str)
#define DEBUGF(fmt, ...)
#endif

#endif