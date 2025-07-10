#ifndef LPB_DEFS_H
#define LPB_DEFS_H

#define DEV_FAM_(X, Y) ((X))
#define DEV_VAR_(X, Y) ((Y))
#define DEV_FAM(...) DEV_FAM_(__VA_ARGS__)
#define DEV_VAR(...) DEV_VAR_(__VA_ARGS__)

/* telegram types */
#define TYPE_00 0x00    // undecoded type sent after date/time change
#define TYPE_QINF 0x01  // request info telegram
#define TYPE_INF 0x02   // send info telegram
#define TYPE_SET 0x03   // set parameter
#define TYPE_ACK 0x04   // acknowledge set parameter
#define TYPE_NACK 0x05  // do not acknowledge set parameter
#define TYPE_QUR 0x06   // query parameter
#define TYPE_ANS 0x07   // answer query
#define TYPE_ERR 0x08   // error
#define TYPE_QRV 0x0F   // query  reset value
#define TYPE_ARV 0x10   // answer reset value
#define TYPE_QRE 0x11   // query reset value failed (1 byte payload of unknown meaning)
#define TYPE_IQ1 0x12   // internal query type 1 (still undecoded)
#define TYPE_IA1 0x13   // internal answer type 1 (still undecoded)
#define TYPE_IQ2 0x14   // internal query type 2 (still undecoded)
#define TYPE_IA2 0x15   // internal answer type 2 (still undecoded)

/* special command ids */
#define CMD_UNKNOWN 0x00000000u
#define CMD_END 0xffffffffu
#define FL_WRITEABLE 0
#define FL_RONLY 1
#define FL_WONLY 2
#define FL_NO_CMD 4
#define FL_OEM 8             // Flag for OEM parameters (read-only by default)
#define FL_SPECIAL_INF 16    // Flag to distinguish between INF telegrams that directly start with the payload (like room temperature) and those who don't (like outside temperature)
#define FL_EEPROM 32         // Flag to determine whether value should be written to EEPROM
#define FL_QINF_ONLY 64      // Flag to determine whether parameter needs to be queried via TYP_QUR or TYP_QINF
#define FL_SW_CTL_RONLY 128  // Software controlled read-only flag. if readOnlyMode = 1 then program values won't save. If readOnlyMode = 0 - new values can be set.
#define FL_NOSWAP_QUR 256    // Do not swap first two bytes for QUR telegram
#define FL_FORCE_INF 512     // Command ID is always used with INF telegrams, so force INF even if SET is requested.
#define FL_ENUM_0_1 (0 << 16) + (1 << 20)
#define FL_ENUM_0_2 (0 << 16) + (2 << 20)
#define FL_ENUM_1_1 (1 << 16) + (1 << 20)
#define FL_ENUM_1_2 (1 << 16) + (2 << 20)
#define FL_ENUM_2_1 (2 << 16) + (1 << 20)
#define FL_ENUM_2_2 (2 << 16) + (2 << 20)
#define FL_ENUM_3_1 (3 << 16) + (1 << 20)
#define FL_ENUM_4_1 (4 << 16) + (1 << 20)
#define FL_ENUM_5_1 (5 << 16) + (1 << 20)
#define FL_ENUM_6_1 (6 << 16) + (1 << 20)
#define FL_ENUM_6_2 (6 << 16) + (2 << 20)
#define FL_ENUM_7_1 (7 << 16) + (1 << 20)
#define FL_ENUM_8_1 (8 << 16) + (1 << 20)
#define FL_ENUM_9_1 (9 << 16) + (1 << 20)
#define FL_ENUM_10_1 (10 << 16) + (1 << 20)
#define FL_ENUM_11_1 (11 << 16) + (1 << 20)


#define DEV_ALL 255, 255  // All devices

/* Parameter types */
/* order of types must according to optbl table */
typedef enum {
  VT_TEMP,      //  3 Byte - 1 enable / value/64
  VT_STRING,    //* x Byte - 1 enable / string
  VT_DATETIME,  //* 9 Byte - 1 enable 0x01 / year+1900 month day weekday hour min sec
  VT_UINT,      //  3 Byte - 1 enable 0x01 / value
  VT_UNKNOWN
} vt_type_t;

typedef enum {
  DT_VALS,  // plain value
  DT_ENUM,  // value (8/16 Bit) followed by space followed by text
  DT_BITS,  // bit value followed by bitmask followed by text
  DT_WDAY,  // weekday. Not used but must leaved here. Or replaced with new data type in future
  DT_HHMM,  // hour:minute
  DT_DTTM,  // date and time
  DT_DDMM,  // day and month
  DT_STRN,  // string
  DT_DWHM,  // PPS time (day of week, hour:minute)
  DT_TMPR,  // time program
  DT_THMS,  // time (hours:minute:seconds)
} dt_types_t;


typedef struct {
  uint8_t type;            // message type (e.g. VT_TEMP)
  float operand;           // both for divisors as well as factors (1/divisor)
  uint8_t enable_byte;     // for regular commands either 1 or 6. 8 indicates data type does not use enable. 0 indicates no set telegram has been logged to determine correct enable byte.
  uint8_t payload_length;  // length of payload in byte; +32 if special treatment is needed, +64 if payload length can vary; 0 for read-only type or unknown length
  uint8_t data_type;       // Value, String, Date...
  uint8_t precision;       // decimal places
  const char *unit;
  uint8_t unit_len;
  const char *type_text;
} units;

#endif