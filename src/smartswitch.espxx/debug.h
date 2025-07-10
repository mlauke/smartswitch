#ifndef _DEBUG_H
#define _DEBUG_H

//#define DEBUG_ENABLED
#ifdef DEBUG_ENABLED
#define DEBUG(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

#endif