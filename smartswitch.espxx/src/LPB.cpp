#include <LPB.h>
#include "debug.h"

#define printlnToDebug(format) \
  {                            \
    printToDebug(format);      \
    writelnToDebug();          \
  }

LPB::LPB(uint8_t rx, uint8_t tx, uint8_t addr, uint8_t d_addr) : rx_pin(rx), tx_pin(tx), myAddr(addr), destAddr(d_addr), len_idx(1), offset(4), pl_start(13)
{
  for (uint8_t i = 0; i < sizeof(dev_lookup) / sizeof(dev_lookup[0]); i++)
  {
    dev_lookup[i].dev_fam = 0xFF;
    dev_lookup[i].dev_var = 0xFF;
    dev_lookup[i].dev_id = 0xFF;
    dev_lookup[i].dev_oc = 0xFF;
    dev_lookup[i].name[0] = '\0';
    dev_lookup[i].name[17] = '\0';
  }
}

void printToDebug(const char *format)
{
  DEBUGP(format);
}

void writelnToDebug()
{
  DEBUGLN("");
}

static void SerialPrintRAW(byte *msg, byte len);
static int bin2hex(char *toBuffer, byte *fromAddr, int len, char delimiter);

void LPB::printTelegram(byte *msg, float line)
{

  DEBUGF("%.2x -> %.2x: ", msg[3], msg[2]); // source => destination address
  int data_len = 0;
  if (msg[4 + offset] < 0x12 || msg[4 + offset] > 0x20)
  {
    data_len = msg[getLen_idx()] - 14; // get packet length, then subtract
  }
  else
  {
    data_len = msg[getLen_idx()] - 7; // for yet unknow telegram types 0x12 to 0x15
  }
  for (int x = 0; x < data_len; x++)
  {
    DEBUGF("%.2x ", msg[getPl_start() + x]);
  }
  DEBUGF("%4.1f - \n", line);
  /*
  int i = findLine(line);
  printToDebug(cmdtbl[i].desc);
  */
  SerialPrintRAW(msg, msg[getLen_idx()] + 1);
}

static void SerialPrintRAW(byte *msg, byte len)
{
  char outBuf[256];

  int outBufLen = strlen(outBuf);

  bin2hex(outBuf + outBufLen, msg, len, ' ');
  printToDebug(outBuf + outBufLen);
  outBuf[outBufLen] = 0;
}

int bin2hex(char *toBuffer, byte *fromAddr, int len, char delimiter)
{
  int resultLen = 0;
  bool isNotFirst = false;
  for (int i = 0; i < len; i++)
  {
    if (isNotFirst)
    {
      if (delimiter != 0)
      {
        toBuffer[resultLen] = delimiter;
        resultLen++;
      }
    }
    else
    {
      isNotFirst = true;
    }
    resultLen += sprintf_P(toBuffer + resultLen, "%02X", fromAddr[i]);
  }
  return resultLen;
}

const char *printError(uint16_t error)
{
  const char *errormsgptr;
  switch (error)
  {
  case 0:
    errormsgptr = "";
    break;
  case 7:
    errormsgptr = " (parameter not supported)";
    break;
  case 256:
    errormsgptr = " - decoding error";
    break;
  case 257:
    errormsgptr = " unknown command";
    break;
  case 258:
    errormsgptr = " - not found";
    break;
  case 259:
    errormsgptr = " no enum str";
    break;
  case 260:
    errormsgptr = " - unknown type";
    break;
  case 261:
    errormsgptr = " query failed";
    break;
  default:
    if (error < 256)
      errormsgptr = " (bus error)";
    else
      errormsgptr = " (??? error)";
    break;
  }
  return errormsgptr;
}

bool LPB::enableInterface(uint8_t retries)
{
#ifdef ESP32
  serial = new HardwareSerial(2); // UART2
  static_cast<HardwareSerial *>(serial)->begin(LPB_BAUDRATE, SERIAL_8N1, rx_pin, tx_pin, false);
#elif defined(ESP8266)
  serial = new SoftwareSerial();
  ((SoftwareSerial *)serial)->begin(LPB_BAUDRATE, SWSERIAL_8O1, rx_pin, tx_pin, false);
#endif
  DEBUGLN("LPB interface enabled.");

  bool success = false;
  while (--retries > 0 && !(success = GetDevId())){
    DEBUGF("LPP get device retries %d\n", retries);
  }
    ; // retry get device id

  return success;
}

void LPB::disableInterface()
{
  if (serial)
  {
#if defined(ESP32)
    static_cast<HardwareSerial *>(serial)->flush();
    static_cast<HardwareSerial *>(serial)->end();
#elif defined(ESP8266)
    ((SoftwareSerial *)serial)->flush();
    ((SoftwareSerial *)serial)->end();
#endif
  }
  serial = NULL;
}

float _toFIXPOINT(float dval, uint8_t precision)
{

  if (precision == 0)
  {
    return dval;
  }

  float rval = 10.0;
  for (int i = 0; i < precision; i++)
    rval *= 10.0;
  return (int)(dval * rval + 0.5) / rval; // rounding
}

float LPB::toFIXPOINT(byte *msg, cmd_t cmd)
{

  int data_len = 0;
  if (msg[4 + offset] < 0x12 || msg[4 + offset] > 0x20)
  {
    data_len = msg[getLen_idx()] - 14; // get packet length, then subtract
  }
  else
  {
    data_len = msg[getLen_idx()] - 7; // for yet unknow telegram types 0x12 to 0x15
  }

  DEBUGF("dval len %d %d 0x%08x\n", data_len, cmd.type, cmd.flags);
  if (data_len == 3 || data_len == 5)
  {
    if (msg[getPl_start()] == 0 || (cmd.flags & FL_SPECIAL_INF))
    {

      float divider = optbl[cmd.type].operand;
      uint8_t precision = optbl[cmd.type].precision;

      float dval;

      if (data_len == 3)
      {
        if (cmd.flags & FL_SPECIAL_INF)
        {
          dval = float((int16_t)(msg[getPl_start()] << 8) + (int16_t)msg[getPl_start() + 1]) / divider;
          DEBUGF("dval %d %f %f %d\n", (msg[getPl_start()] << 8) + (int16_t)msg[getPl_start() + 1], dval, divider, precision);
        }
        else
        {
          dval = float((int16_t)(msg[getPl_start() + 1] << 8) + (int16_t)msg[getPl_start() + 2]) / divider;
          DEBUGF("dval %d %f %f %d\n", (msg[getPl_start() + 1] << 8) + (int16_t)msg[getPl_start() + 2], dval, divider, precision);
        }
      }
      else
      {
        dval = float((int16_t)(msg[getPl_start() + 3] << 8) + (int16_t)msg[getPl_start() + 4]) / divider;
        DEBUGF("dval %d %f %f %d\n", (msg[getPl_start() + 3] << 8) + msg[getPl_start() + 4], dval, divider, precision);
      }
      return _toFIXPOINT(dval, precision);
    }
    else
    {
      // undefinedValueToBuffer(decodedTelegram.value);
    }
    // printDebugValueAndUnit(decodedTelegram.value, decodedTelegram.unit);
  }
  else
  {
    DEBUGLN("FIXPOINT len !=3: ");
    // prepareToPrintHumanReadableTelegram(msg, data_len, getPl_start());
    // decodedTelegram.error = 256;
  }
  return NAN;
}

float LPB::getTemperature(float line, float *r)
{

  byte rx_msg[33];
  if (query(line, rx_msg) == 0)
  {
    cmd_t cmd = cmdtbl[findLine(line)];
    *r = toFIXPOINT(rx_msg, cmd);
    DEBUGF("getTemp() %f %f\n", line, *r);
    return *r;
  }
  DEBUGF("getTemp() %f => NAN\n", line);
  return NAN;
}

bool LPB::update(boiler_t *p, bool simulate)
{

  if (simulate)
  {
    p->t_cur = 52.0f;
    p->t_max = 65.0f;
    p->t_min = 45.0f;
    p->t_nom = 55.0f;
    return true;
  }

  if (dev_lookup[0].dev_id == 0xFF)
  {
    return false;
  }

  if (getTemperature(8310, &boilerData.t_cur) == NAN || getTemperature(1610, &boilerData.t_nom) == NAN || getTemperature(1612, &boilerData.t_min) == NAN || getTemperature(1645, &boilerData.t_max) == NAN)
  {
    return false;
  }
  p->t_cur = boilerData.t_cur;
  p->t_max = boilerData.t_max;
  p->t_min = boilerData.t_min;
  p->t_nom = boilerData.t_nom;

  return true;
}

uint8_t LPB::getBusAddr()
{
  return myAddr;
}

uint8_t LPB::getBusDest()
{
  return destAddr;
}

uint8_t LPB::getLen_idx()
{
  return len_idx;
}

uint8_t LPB::getPl_start()
{
  return pl_start;
}

device_map *LPB::getDestDevice()
{
  for (uint8_t i = 0; i < sizeof(dev_lookup) / sizeof(dev_lookup[0]); i++)
  {
    if (dev_lookup[i].dev_id == getBusDest())
    {
      return &dev_lookup[i];
    }
  }
  return NULL;
}

bool LPB::GetDevId()
{

  if (dev_lookup[0].dev_id == 0xFF)
  {

    byte msg[33] = {0};
    byte tx_msg[33] = {0};
    uint8_t save_destAddr = getBusDest();

    int8_t anz_dev = 0;
    DEBUGLN("Scanning devices on the bus...");

    if (Send(TYPE_QINF, 0x053D0064, msg, tx_msg, NULL, 0, false) == BUS_OK)
    {
      printTelegram(tx_msg, -1);
      unsigned long startquery = millis();
      while (millis() - startquery < 10000)
      {
        if (GetMessage(msg))
        {
          printTelegram(msg, -1);
          if (msg[4 + offset] != TYPE_INF || msg[2] != getBusAddr())
          {
            break;
          }
          for (uint8_t i = 0; i < sizeof(dev_lookup) / sizeof(dev_lookup[0]); i++)
          {
            if (dev_lookup[i].dev_id == msg[3])
            { // dest
              //                found = true;
              break;
            }
            if (dev_lookup[i].dev_id == 0xFF)
            {
              dev_lookup[i].dev_id = msg[3];
              dev_lookup[i].dev_fam = msg[10 + offset];
              dev_lookup[i].dev_var = msg[12 + offset];
              dev_lookup[i].dev_oc = (msg[13 + offset] << 8) + msg[14 + offset];
              dev_lookup[i].dev_serial = (msg[15 + offset] << 24) + (msg[16 + offset] << 16) + (msg[17 + offset] << 8) + (msg[18 + offset]);
              dev_lookup[i].name[0] = '\0';
              dev_lookup[i].name[17] = '\0';
              break;
            }
          }
        }
        delay(1);
      }
      for (uint8_t i = 0; i < sizeof(dev_lookup) / sizeof(dev_lookup[0]); i++)
      {
        if (dev_lookup[i].dev_id == 0xFF)
          break;

        destAddr = dev_lookup[i].dev_id;
        if (Send(TYPE_QUR, 0x053D0001, msg, tx_msg, NULL, 0, true) == BUS_OK)
        {
          printTelegram(tx_msg, -1);
          printTelegram(msg, -1);
          memcpy(dev_lookup[i].name, &msg[getPl_start()], 17);
        }
      }
      DEBUGLN("Bus devices found:");
      for (int i = 0; i < (int)sizeof(dev_lookup) / (int)sizeof(dev_lookup[0]); i++)
      {
        if (dev_lookup[i].dev_id == 0xFF)
        {
          if (i < anz_dev - 1)
          {
            DEBUGF("Only %d out of %d devices have responded, will run device detection again next time.\n", i + 1, anz_dev);
            dev_lookup[0].dev_id = 0xFF;
            return false;
          }
          break;
        }
        DEBUGF("%d/%d/%d/%s\n", dev_lookup[i].dev_id, dev_lookup[i].dev_fam, dev_lookup[i].dev_var, dev_lookup[i].name);
      }
    }
    destAddr = save_destAddr;
  }
  // get first device
  for (uint8_t i = 0; i < sizeof(dev_lookup) / sizeof(dev_lookup[0]); i++)
  {
    if (dev_lookup[i].dev_id == getBusDest())
    {
      my_dev_fam = dev_lookup[i].dev_fam;
      my_dev_var = dev_lookup[i].dev_var;
      my_dev_oc = dev_lookup[i].dev_oc;
      my_dev_serial = dev_lookup[i].dev_serial;
      DEBUGF("device family: 0x%.2x, device variant: 0x%.2x\n", my_dev_fam, my_dev_var);
      return true;
    }
  }
  DEBUGF("unknown destination ID, sticking to device family: %d, device variant: %d\n", my_dev_fam, my_dev_var);
  return false;
}

int LPB::findLine(float line)
{
  uint8_t found = 0;
  int i = -1;
  int save_i = 0;
  uint32_t c;
  float l;

  for (uint16_t j = 0; j < cmdtbl_size; j++)
  {
    if (cmdtbl[j].line == line)
    {
      i = j;
      break;
      return i;
    }
  }
  if (i == -1)
    return i;

  l = cmdtbl[i].line;
  while (l == line)
  {
    c = cmdtbl[i].cmd;
    uint8_t dev_fam = cmdtbl[i].dev_fam;
    uint8_t dev_var = cmdtbl[i].dev_var;
    uint32_t dev_flags = cmdtbl[i].flags;

    if ((dev_fam == my_dev_fam || dev_fam == DEV_FAM(DEV_ALL)) && (dev_var == my_dev_var || dev_var == DEV_VAR(DEV_ALL)))
    {
      if (dev_fam == my_dev_fam && dev_var == my_dev_var)
      {
        if ((dev_flags & FL_NO_CMD) == FL_NO_CMD)
        {
          while (c == cmdtbl[i].cmd)
          {
            i++;
          }
          found = 0;
          i--;
        }
        else
        {
          found = 1;
          save_i = i;
          break;
        }
      }
      else if ((!found && dev_fam != my_dev_fam) || (dev_fam == my_dev_fam))
      { // wider match has hit -> store in case of best match
        if ((dev_flags & FL_NO_CMD) == FL_NO_CMD)
        {
          while (c == cmdtbl[i].cmd)
          {
            i++;
          }
          found = 0;
          i--;
        }
        else
        {
          found = 1;
          save_i = i;
        }
      }
    }
    i++;
    l = cmdtbl[i].line;
  }

  if (!found)
  {
    return -1;
  }
  return save_i;
}

uint16_t LPB::query(float line, byte *msg)
{ // line (ProgNr)

  byte tx_msg[33] = {0}; // xmit buffer

  int i = 0;

  i = findLine(line);
  if (i == -1)
  {
    return i;
  }

  uint32_t c = cmdtbl[i].cmd;
  uint8_t query_type = TYPE_QUR;
  uint32_t dev_flags = cmdtbl[i].flags;
  if (dev_flags & FL_QINF_ONLY)
  {
    query_type = TYPE_QINF;
  }
  if (dev_flags & FL_NOSWAP_QUR)
  {
    c = ((c & 0xFF000000) >> 8) | ((c & 0x00FF0000) << 8) | (c & 0x0000FFFF); // Bytes 1+2 of CoID will be swapped for QUR command, but need to remain as-is for FL_NOSWAP_QUR parameters, so swap here again.
  }
  if (i >= 0)
  {
    if (c != CMD_UNKNOWN && (dev_flags & FL_NO_CMD) != FL_NO_CMD)
    { // send only valid command codes
      short retry = QUERY_RETRIES;
      while (retry)
      {
        if (Send(query_type, c, msg, tx_msg) == BUS_OK)
        {
          // Decode the xmit telegram and send it
          printTelegram(tx_msg, line);
          // Decode the rcv telegram and send it
          printTelegram(msg, line);
          DEBUGF("#%g: ", line);
          // printlnToDebug(build_pvalstr(0));
          break; // success, break out of while loop
        }
        else
        {
          printlnToDebug(printError(261)); // query failed
          retry--;                         // decrement number of attempts
        }
      } // endwhile, maximum number of retries reached
      if (retry == 0)
      {
        DEBUGF("%g\n", line);
        return 261;
      }
    }
    return 0; // ok
  }
  return -1;
}

// Generates checksum from LPB message
// (255 - (Telegrammlänge ohne PS - 1)) * 256 + Telegrammlänge ohne PS - 1 + Summe aller Telegrammbytes
uint16_t LPB::CRC_LPB(const byte *buffer, uint8_t length)
{
  uint16_t crc = (257 - length) * 256 + length - 2;

  for (uint8_t i = 0; i < length - 1; i++)
  {
    crc = crc + buffer[i];
  }

  return crc;
}

// Dumps a message to Serial
void LPB::print(const byte *msg)
{
  byte len = msg[len_idx];
  if (len > 32)
    return;
  byte data = 0;

  for (int i = 0; i < len + 1; i++)
  { // msg length counts from zero with LPB (bus_type 1) and from 1 with BSB (bus_type 0)
    data = msg[i];
    if (data < 16)
      DEBUG("0");
    DEBUGF("$%.2x ", data);
  }
  DEBUGLN("");
}

bool LPB::rx_pin_read()
{
  return bool(*portInputRegister(digitalPinToPort(rx_pin)) & digitalPinToBitMask(rx_pin)) ^ 1;
}

uint8_t LPB::readByte()
{
  byte read = serial->read() ^ 0xFF;
  return read;
}

// Low-Level sending of message to bus
inline int8_t LPB::_send(byte *msg)
{
  // Nun - Ein Teilnehmer will senden :
  byte data, len;
  len = msg[len_idx];
  msg[0] = 0x78;
  msg[2] = destAddr;
  msg[3] = myAddr;
  uint16_t crc = CRC_LPB(msg, len);
  msg[len - 1] = (crc >> 8);
  msg[len] = (crc & 0xFF);

#if DEBUG_LL
  print(msg);
#endif
  static const unsigned long timeoutabort = 1000; // one second timeout
  unsigned long start_timer = millis();
  unsigned long waitfree;
retry:
  waitfree = random(1, 20) + 3 + 59; // range 63 .. 82 ms, BSB mimimum delay between telegrams is 59 ms (25 for LPB -> miwi), plus duration of one full (32 bytes) telegram (3 ms), plus random amount of 1-20 ms.
  {                                  // block begins
    if (millis() - start_timer > timeoutabort)
    { // one second has elapsed
      return BUS_NOTFREE;
    }
    unsigned long timeout = millis();
    // Probe the bus until the delay calculated above has passed. We want to wait this long even on the first try because we want to make sure the minimum delay between telegrams has passed.
    while (millis() - timeout < waitfree)
    {
      bool rx_pin = rx_pin_read();
      if (rx_pin)
      { // If there is activity on the bus / the bus has been pulled low, we have to try again and wait for 'waitfree' ms.
#if DEBUG_LL
        DEBUGLN("Activity on the bus while waiting, retrying...");
#endif
        delay(146); // Wait the duration of 11 bits at 4800 bps times 32 (maximum telegram size) in ms (*1000) times 2 (because we can just keep waiting for an answer telegram)
        while (serial->available())
        {
          char c = readByte();
#if DEBUG_LL
          if (c < 16)
            DEBUG("0");
          DEBUG(c, HEX);
          DEBUG(" ");
#endif
          c = c; // prevent compiler warning about unused variable if DEBUG_LL is not active
        }
#if DEBUG_LL
        DEBUGLN();
#endif
        goto retry;
      } // endif
    } // endwhile
  } // block ends

  byte loop_len = len; // same msg length difference as above
  for (byte i = 0; i <= loop_len; i++)
  {
    data = msg[i];
    data = data ^ 0xFF;
    serial->write(data);
#if !defined(ESP32)
    serial->flush();
#endif
  }
  // #if !defined(ESP32)
  serial->flush();
  unsigned long timeout = millis();
  while ((millis() - timeout < 50) && serial->available() == 0)
  {
    delay(1);
  }
  // #endif
  //   delay(loop_len*2+10);      // Wait up to 32 characters for the maximum number of bytes in a telegram to show up again on RX after sending it via TX.
  if (false)
  {
    if (serial->available())
    {
      for (uint8_t i = 0; i <= loop_len; i++)
      {
        char readdata = readByte();
        if (msg[i] != readdata)
        {
#if DEBUG_LL
          DEBUGLN(readdata, HEX);
#endif
          DEBUGLN("Collision on the bus, retrying...");
          delay(146); // Wait the duration of 11 bits at 4800 bps times 32 (maximum telegram size) in ms (*1000) times 2 (because we can just keep waiting for an answer telegram)
          while (serial->available())
          {
            char c = readByte();
#if DEBUG_LL
            if (c < 16)
              DEBUG("0");
            DEBUG(c, HEX);
#endif
            c = c; // prevent compiler warning about unused variable if DEBUG_LL is not active
          }
#if DEBUG_LL
          DEBUGLN();
#endif
          goto retry;
        }
      }
    }
  }
  return BUS_OK;
}

bool LPB::GetMessage(byte *msg)
{

  byte i = 0;
  uint8_t read;

  while (serial->available() > 0 && i < 33)
  {
    // Read serial data...
    read = readByte();
    // ... until SOF detected (= 0xDC, 0xDE bei BSB bzw. 0x78 bei LPB)
    if (read == 0x78)
    {
      // Restore otherwise dropped SOF indicator
      msg[i++] = read;

      //      	uint8_t PPS_write_enabled = myAddr;
      //      	if (PPS_write_enabled == 1) {
      //          return true; // PPS-Bus request byte 0x17 just contains one byte, so return
      //      	} else {
      //      	  len_idx = 9;
      //	      }

      // Delay for more data
      if (serial->available() == 0)
      {
        delay(4); // I wonder why HardwareSerial needs longer than SoftwareSerial until a character is ready to be processed. Also, why 3ms are fine for the Mega, but at least 4ms are necessary on the Due
      }
      // read the rest of the message
      while (serial->available() > 0 && i < 33)
      {
        read = readByte();
        msg[i++] = read;
        /*
#if DEBUG_LL
        if(read<16){
          DEBUG("0");
        }
        DEBUG(read, HEX);
        DEBUG(" ");
#endif
*/
        // Break if message seems to be completely received (i==msg.length)
        if (i > len_idx)
        {
          if (msg[len_idx] > 32) // check for maximum message length
            break;
          if (i >= msg[len_idx] + 1)
            break;
        }
        // Delay until we got next byte
        if (serial->available() == 0)
        {
          delay(4); // see question/reason above
        }
      }

      // We should have read the message completely. Now check and return

      if (i == msg[len_idx] + 1)
      { // LPB msg length is one less than BSB
        // Seems to have received all data
        if (CRC_LPB(msg, i - 1) - msg[i - 2] * 256 - msg[i - 1] == 0)
        {
          return true;
        }
        else
        {
          DEBUGLN("CRC error:");
          print(msg);
          return false;
        }
      }
      else
      {
        // Length error
        DEBUGF("Length error: 0x%.2x / 0x%.2x\n", i, msg[len_idx] + 1);
        print(msg);
        return false;
      }
    }
  }
  // We got no data so:
  return false;
}

int8_t LPB::Send(uint8_t type, uint32_t cmd, byte *rx_msg, byte *tx_msg, const byte *param, byte param_len, bool wait_for_reply)
{
  byte i;
  byte length_offset = 0;

  while (serial->available() > 0)
  {
    readByte(); // flush
  }

  // first two bytes are swapped
  byte A2 = (cmd & 0xff000000) >> 24;
  byte A1 = (cmd & 0x00ff0000) >> 16;
  byte A3 = (cmd & 0x0000ff00) >> 8;
  byte A4 = (cmd & 0x000000ff);

  // special treatment of internal query types
  if (type == 0x12)
  { // TYPE_IQ1
    A1 = A3;
    A2 = A4;
    length_offset = 2;
  }
  if (type == 0x14)
  { // TYPE_IQ2
    A1 = A4;
    length_offset = 3;
  }

  tx_msg[1] = param_len + 14 - length_offset;
  tx_msg[4] = 0xC0; // some kind of sending/receiving flag?
  tx_msg[5] = 0x02; // yet unknown
  tx_msg[6] = 0x00; // yet unknown
  tx_msg[7] = 0x14; // yet unknown
  tx_msg[8] = type;
  // Adress
  tx_msg[9] = A1;
  tx_msg[10] = A2;
  tx_msg[11] = A3;
  tx_msg[12] = A4;

  // Value
  for (i = 0; i < param_len; i++)
  {
    tx_msg[13 + i] = param[i];
  }
  int8_t return_value = _send(tx_msg);
  if (return_value != BUS_OK)
    return return_value;
  if (!wait_for_reply)
    return return_value;

  i = 15;

  unsigned long timeout = millis() + 3000;
  while ((i > 0) && (millis() < timeout))
  {
    if (GetMessage(rx_msg))
    {
#if DEBUG_LL
      DEBUG(F("\r\nDuration until answer received: "));
      DEBUGLN(3000 - (timeout - millis()));
      print(rx_msg);
#endif
      i--;
      byte msg_type = rx_msg[4 + offset];
      if (rx_msg[2] == myAddr && ((type == 0x12 && msg_type == 0x13) || (type = 0x14 && msg_type == 0x15)))
      {
        return BUS_OK;
      }
      /* Activate for LPB systems with truncated error messages (no commandID in return telegram)
       */
      if (rx_msg[2] == myAddr && rx_msg[8] == 0x08)
      { // TYPE_ERR
        return false;
      }
      if ((rx_msg[2] == myAddr) && (rx_msg[5 + offset] == A2) && (rx_msg[6 + offset] == A1) && (rx_msg[7 + offset] == A3) && (rx_msg[8 + offset] == A4))
      {
        return BUS_OK;
      }
      else
      {
#if DEBUG_LL
        DEBUGLN(F("Message received, but not for us:"));
        print(rx_msg);
#endif
      }
    }
    else
    {
      delayMicroseconds(205);
    }
  }
#if DEBUG_LL
  DEBUGLN(F("No answer for this send telegram:"));
#endif
  print(tx_msg);

  return BUS_NOMATCH;
}
