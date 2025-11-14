#ifndef _DEBUG_H
#define _DEBUG_H

#ifdef DEBUG_ENABLED
#if defined(ESP8266) || defined(ESP32)
#define DEBUG(fmt, ...) Serial.printf(fmt, __VA_ARGS__)
#else
#include <stdio.h>
#define DEBUG(fmt, ...) fprintf(stdout, fmt, __VA_ARGS__)
#endif
#else
#define DEBUG(fmt, ...)
#endif

#endif