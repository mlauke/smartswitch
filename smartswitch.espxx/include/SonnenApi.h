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

#include "JsonFields.h"

// property names of the sonnen battery REST api - kept in flash, not in RAM

// api/v2/configurations
static const char KEY_SN_INVERTER_MAX_POWER_W[] PROGMEM = "IC_InverterMaxPower_w";
static const char KEY_SN_MODULE_CAPACITY[] PROGMEM = "CM_MarketingModuleCapacity";
static const char KEY_SN_BATTERY_MODULES[] PROGMEM = "IC_BatteryModules";

// api/v2/battery
static const char KEY_SN_FULL_CHARGE_CAPACITY_WH[] PROGMEM = "fullchargecapacitywh";
static const char KEY_SN_CYCLE_COUNT[] PROGMEM = "cyclecount";

// api/v2/latestdata
static const char KEY_SN_UTC_OFFSET[] PROGMEM = "UTC_Offset";
static const char KEY_SN_IC_STATUS[] PROGMEM = "ic_status";
static const char KEY_SN_SETPOINT_PRIORITY[] PROGMEM = "Setpoint Priority";
static const char KEY_SN_FULL_CHARGE_REQUEST[] PROGMEM = "Full Charge Request";
static const char KEY_SN_NEXT_FULL_CHARGE_START[] PROGMEM = "nextfullchargestarttime";
static const char KEY_SN_SECONDS_SINCE_FULL_CHARGE[] PROGMEM = "secondssincefullcharge";

// api/v2/status
static const char KEY_SN_USOC[] PROGMEM = "USOC";
static const char KEY_SN_GRID_FEED_IN_W[] PROGMEM = "GridFeedIn_W";
static const char KEY_SN_PRODUCTION_W[] PROGMEM = "Production_W";
static const char KEY_SN_CONSUMPTION_W[] PROGMEM = "Consumption_W";
static const char KEY_SN_PAC_TOTAL_W[] PROGMEM = "Pac_total_W";
static const char KEY_SN_BATTERY_CHARGING[] PROGMEM = "BatteryCharging";
static const char KEY_SN_BATTERY_DISCHARGING[] PROGMEM = "BatteryDischarging";
static const char KEY_SN_TIMESTAMP[] PROGMEM = "Timestamp";

// fields required by the firmware - a response missing one of them is treated as error

static const JsonField SONNEN_FIELDS_CONFIGURATIONS[] PROGMEM = {
    {{KEY_SN_INVERTER_MAX_POWER_W}},
    {{KEY_SN_MODULE_CAPACITY}},
    {{KEY_SN_BATTERY_MODULES}},
};

static const JsonField SONNEN_FIELDS_BATTERY[] PROGMEM = {
    {{KEY_SN_FULL_CHARGE_CAPACITY_WH}},
    {{KEY_SN_CYCLE_COUNT}},
};

static const JsonField SONNEN_FIELDS_LATEST_DATA[] PROGMEM = {
    {{KEY_SN_UTC_OFFSET}},
    {{KEY_SN_IC_STATUS, KEY_SN_SETPOINT_PRIORITY, KEY_SN_FULL_CHARGE_REQUEST}},
    {{KEY_SN_IC_STATUS, KEY_SN_NEXT_FULL_CHARGE_START}},
    {{KEY_SN_IC_STATUS, KEY_SN_SECONDS_SINCE_FULL_CHARGE}},
};

static const JsonField SONNEN_FIELDS_STATUS[] PROGMEM = {
    {{KEY_SN_USOC}},
    {{KEY_SN_GRID_FEED_IN_W}},
    {{KEY_SN_PRODUCTION_W}},
    {{KEY_SN_CONSUMPTION_W}},
    {{KEY_SN_PAC_TOTAL_W}},
    {{KEY_SN_BATTERY_CHARGING}},
    {{KEY_SN_BATTERY_DISCHARGING}},
    {{KEY_SN_TIMESTAMP}},
};
