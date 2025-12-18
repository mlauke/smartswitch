#ifndef LPB_H
#define LPB_H

#define LPB_BAUDRATE 4800

#define QUERY_RETRIES 3

#define BUS_OK 1
#define BUS_NOTFREE -1
#define BUS_NOMATCH -2

#define cmdtbl_size sizeof(cmdtbl) / sizeof(cmdtbl[0])
#define uint_farptr_t const char*

#include <stdint.h>
#include <stdio.h>
#include <Stream.h>
#include <Arduino.h>
#include "LPB_defs.h"
#include "LPB_LANG_de.h"

#ifdef ESP32
#include <HardwareSerial.h>
#elif defined(ESP8266)
#include <SoftwareSerial.h>
#endif

#include <cmath>

typedef uint8_t byte;
// typedef uint16_t word;

typedef struct {
  uint32_t cmd;          // the command or fieldID
  uint8_t type;          // the message type
  float line;            // parameter number
  const char* desc;      // description test
  uint16_t enumstr_len;  // sizeof enum
  const char* enumstr;   // enum string
  uint32_t flags;        // e.g. FL_RONLY
  uint8_t dev_fam;       // device family
  uint8_t dev_var;       // device variant
} cmd_t;

typedef struct {
  uint16_t dev_fam;
  uint16_t dev_var;
  uint8_t dev_id;
  uint16_t dev_oc;
  uint32_t dev_serial;
  char name[18];
} device_map;


typedef struct {
  float t_cur;
  float t_nom;
  float t_min;
  float t_max;
} boiler_t;

class LPB {
public:

  LPB(uint8_t rx, uint8_t tx, uint8_t addr = 0x42, uint8_t d_addr = 0x00);

  bool enableInterface(uint8_t retries);
  bool GetDevId();

  device_map* getDestDevice();

  void disableInterface();
  bool update(boiler_t* data, bool simulate);

  int8_t Send(uint8_t type, uint32_t cmd, byte* rx_msg, byte* tx_msg, const byte* param = NULL, byte param_len = 0, bool wait_for_reply = true);
  bool GetMessage(byte* msg);
  void print(const byte* msg);

private:

  uint8_t rx_pin;
  uint8_t tx_pin;
  uint8_t myAddr;
  uint8_t destAddr;
  uint8_t len_idx;
  uint8_t offset;
  uint8_t pl_start;

  boiler_t boilerData = {0, 0, 0 ,0};

  uint8_t getBusAddr();
  uint8_t getBusDest();
  uint8_t getPl_start();
  uint8_t getLen_idx();

  uint8_t my_dev_fam = 0;
  uint8_t my_dev_var = 0;
  uint16_t my_dev_oc = 0;
  uint32_t my_dev_serial = 0;

  device_map dev_lookup[10];

  inline int8_t _send(byte* msg);
  uint16_t CRC(byte* buffer, uint8_t length);
  uint16_t CRC_LPB(const byte* buffer, uint8_t length);
  uint16_t _crc_xmodem_update(uint16_t crc, uint8_t data);
  uint8_t readByte();
  bool rx_pin_read();
  int findLine(float line);
  uint16_t query(float line, byte* rx_msg);  // line (ProgNr)

  float getTemperature(float line, float* r);

  void printTelegram(byte* msg, float query_line);
  float toFIXPOINT(byte* msg, cmd_t cmd);

  Stream* serial = NULL;  // Bus interface. Point to Software or HarwareSerial
};

#define DEFAULT_FLAG FL_SW_CTL_RONLY


const units optbl[] = {
  { VT_TEMP, 64.0, 1, 2 + 32, DT_VALS, 1, U_DEG, sizeof(U_DEG), STR_TEMP },
  { VT_STRING, 1.0, 8, 22 + 64, DT_STRN, 0, U_NONE, sizeof(U_NONE), STR_STRING },
  { VT_DATETIME, 1.0, 1, 8 + 32, DT_DTTM, 0, U_NONE, sizeof(U_NONE), STR_DATETIME },
  { VT_UINT, 1.0, 1, 2, DT_VALS, 0, U_NONE, sizeof(U_NONE), STR_UINT },

  { VT_UNKNOWN, 1.0, 0, 0, DT_STRN, 1, U_NONE, sizeof(U_NONE), STR_UNKNOWN },
};

const cmd_t cmdtbl[] = {
  // Uhrzeit und Datum
  { 0x053D000B, VT_DATETIME, 0, STR0, 0, NULL, DEFAULT_FLAG, DEV_ALL },      // [ ] - Uhrzeit und Datum
  { 0x0505000B, VT_DATETIME, 0, STR0, 0, NULL, DEFAULT_FLAG, DEV_ALL },      // [ ] - Uhrzeit und Datum   // gleiche Funktion mit anderer CommandID
  { 0x0500006C, VT_DATETIME, 0, STR0, 0, NULL, DEFAULT_FLAG, DEV_ALL },      // [ ] - Uhrzeit und Datum   // gleiche Funktion mit anderer CommandID
  { 0x053D000B, VT_DATETIME, 0, STR0, 0, NULL, DEFAULT_FLAG, DEV_ALL },      // [ ] - Uhrzeit und Datum   // gleiche Funktion mit anderer CommandID
                                                                             //  { 0x2D3D058E, VT_TEMP, 710, STR710, 0, NULL, DEFAULT_FLAG, DEV_ALL },      // [°C ] - Heizkreis 1 - Komfortsollwert
  { 0x053D0001, VT_STRING, 6224, STR6224, 0, NULL, DEFAULT_FLAG, DEV_ALL },  // Geräte-Identifikation
  { 0x053D0002, VT_UINT, 6225, STR6225, 0, NULL, DEFAULT_FLAG, DEV_ALL },    // Gerätefamilie
  { 0x053D0003, VT_UINT, 6226, STR6226, 0, NULL, DEFAULT_FLAG, DEV_ALL },    // Gerätevariante
                                                                             //  { 0x053D0521, VT_TEMP, 8700, STR8700, 0, NULL, DEFAULT_FLAG, DEV_ALL },    // [°C ] - Diagnose Verbraucher - Aussentemperatur
  /*
  { 0x05000000, VT_TEMP, 8310, STR8310, 0, NULL, DEFAULT_FLAG, DEV_ALL },  // [°C ] - Diagnose Erzeuger - Kesseltemperatur
  { 0x05000461, VT_TEMP, 8310, STR8310, 0, NULL, DEFAULT_FLAG, DEV_ALL },  // [°C ] - Diagnose Erzeuger - Kesseltemperatur
  { 0x05000656, VT_TEMP, 8310, STR8310, 0, NULL, DEFAULT_FLAG, DEV_ALL },  // [°C ] - Diagnose Erzeuger - Kesseltemperatur
  { 0x0500021E, VT_TEMP, 8314, STR8314, 0, NULL, DEFAULT_FLAG, DEV_ALL },  // [°C ] - Diagnose Erzeuger - Kesselrücklauftemperatur
  { 0x050006b9, VT_TEMP, 7973, STR7973, 0, NULL, DEFAULT_FLAG, DEV_ALL },  // [°C ] - Diagnose Erzeuger - Trinkwasser - Speicherfühler B31
  { 0x050006b8, VT_TEMP, 8310, STR8310, 0, NULL, DEFAULT_FLAG, DEV_ALL },  // [°C ] - Diagnose Erzeuger - Kesseltemperatur
*/
  { 0x05000516, VT_TEMP, 8310, STR8310, 0, NULL, DEFAULT_FLAG, DEV_ALL },  // [°C ] - Diagnose Erzeuger - Kesseltemperatur
  { 0x05000532, VT_TEMP, 1610, STR1610, 0, NULL, DEFAULT_FLAG, DEV_ALL },  // [°C ] - Trinkwasser - Nennsollwert
  { 0x050006ba, VT_TEMP, 1612, STR1612, 0, NULL, DEFAULT_FLAG, DEV_ALL },  // [°C ] - Trinkwasser - Reduziertsollwert
  { 0x050006bc, VT_TEMP, 1645, STR1645, 0, NULL, DEFAULT_FLAG, DEV_ALL },  // [°C ] - Trinkwasser - Legionellenfkt. Sollwert

};

#endif