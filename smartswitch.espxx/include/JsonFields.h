// MIT License
//
// Copyright (c) 2024 Marko Lauke
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <ArduinoJson.h>

#if defined(ESP8266) || defined(ESP32)
#include <pgmspace.h>
#else // native test build has no separate flash segment
#define PROGMEM
#define PSTR(s) (s)
#define FPSTR(p) (p)
#define pgm_read_ptr(p) (*(p))
#define strncpy_P strncpy
#define snprintf_P snprintf
#endif

// deepest supported path: ic_status / Setpoint Priority / Full Charge Request
#define JSON_FIELD_MAX_DEPTH 3
#define JSON_FIELD_PATH_SEPARATOR '/'
#define JSON_FIELD_PATH_SIZE 128

#define JSON_FIELD_COUNT(fields) ((uint8_t)(sizeof(fields) / sizeof((fields)[0])))

// one expected response field, given as flash resident, NULL terminated key path
typedef struct JsonField
{
  const char *path[JSON_FIELD_MAX_DEPTH];
} JsonField;

// resolves a flash resident key path within the given node and writes the walked
// path to the given buffer - returns the addressed node, null if it does not exist
inline JsonVariantConst resolveJsonField(JsonVariantConst root, const JsonField *field, char *path, size_t pathSize)
{
  JsonVariantConst node = root;
  size_t len = 0;

  path[0] = '\0';
  for (uint8_t depth = 0; depth < JSON_FIELD_MAX_DEPTH; depth++)
  {
    const char *key = (const char *)pgm_read_ptr(&field->path[depth]);
    if (key == NULL)
      break;

    if (len > 0 && len + 1 < pathSize)
    {
      path[len++] = JSON_FIELD_PATH_SEPARATOR;
    }
    strncpy_P(path + len, key, pathSize - len - 1);
    path[pathSize - 1] = '\0';
    len += strlen(path + len);

    node = node[FPSTR(key)];
  }
  return node;
}

// checks that every expected field is present in the response document
inline bool validateJsonFields(JsonVariantConst root, const JsonField *fields, uint8_t count, char *error, size_t errorSize)
{
  char path[JSON_FIELD_PATH_SIZE];

  for (uint8_t i = 0; i < count; i++)
  {
    if (resolveJsonField(root, &fields[i], path, sizeof(path)).isNull())
    {
      snprintf_P(error, errorSize, PSTR("missing field: %s"), path);
      return false;
    }
  }
  return true;
}
