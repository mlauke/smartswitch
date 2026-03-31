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
#ifndef LOGIC_H
#define LOGIC_H

#include <time.h>
#include "SmartSwitch.h"
#include "Util.h"
#include "debug.h"

#define SECONDS_PER_HOUR 3600

char tsfmt[30];
char *toDate(uint32_t utc_ts, int16_t offset)
{
  time_t time = (time_t)(utc_ts + offset);
  tm timeinfo;
  gmtime_r(&time, &timeinfo);
  strftime(tsfmt, sizeof(tsfmt), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return tsfmt;
}

char *toDate(uint32_t utc_ts)
{
  return toDate(utc_ts, 0);
}

static char *toLocalDate(SystemState *systemState, uint32_t utc_ts)
{
  return toDate(utc_ts, (systemState->utc_offset));
}

void updateSystemState(SystemConfig *systemConfig, SystemState *systemState)
{
  systemState->cons_W_nom = (systemState->cons_W + 5) / 10 * 10; // round up multiple of 10W
  // consumption without load
  systemState->cons_W_norm = systemState->cons_W_nom - (systemState->switchEnabled && systemState->cons_W_nom > systemConfig->loadPower_W ? systemConfig->loadPower_W : 0);
  systemState->system_Power_W = systemState->prod_W + (systemState->fullChargeRequest || systemState->usoc == 0 ? 0 : systemState->inv_max_w);

  // Update per-hour-per-weekday consumption statistics (true hourly mean, cross-week EMA α=0.1)
  if (systemState->ts > 0)
  {
    time_t local_ts = (time_t)(systemState->ts + systemState->utc_offset);
    struct tm lt;
    gmtime_r(&local_ts, &lt);
    int16_t key = (int16_t)(lt.tm_wday * 24 + lt.tm_hour);

    // On hour rollover: compute true mean and blend into slot via EMA, then reset accumulators
    if (key != systemState->stat_hour_key && systemState->stat_hour_key >= 0 && systemState->cons_count > 0)
    {
      uint8_t slot_day = (uint8_t)(systemState->stat_hour_key / 24); // day of the hour being closed out (differs from current day at midnight)
      uint8_t slot_hour = (uint8_t)(systemState->stat_hour_key % 24);
      uint16_t *slot = &systemConfig->cons_stats_Wh[slot_day][slot_hour];
      *slot = (*slot == 0) ? systemState->cons_avg_W
                           : (uint16_t)(((uint32_t)*slot * 9 + systemState->cons_avg_W) / 10);
      systemState->cons_sum_W = 0;
      systemState->cons_count = 0;
    }

    // Accumulate current sample into the (possibly just-reset) hour bucket
    systemState->cons_sum_W += systemState->cons_W_norm;
    systemState->cons_count++;
    systemState->cons_avg_W = (uint16_t)(systemState->cons_sum_W / systemState->cons_count);
    systemState->stat_hour_key = key;
  }
}

enum BatteryLevel
{
  Min,     // predicted final state will be below min capacity
  Max,     // predicted state will reach battery full capacity
  Balanced // neither min nor max will be reached
};

typedef struct
{
  uint8_t hours;
  BatteryLevel level;
} BatteryState;

static int findPvForecastData(SystemState *systemState)
{
  short i = 0;

  if (systemState->pv_forecast_ts_wh[0][0] == 0)
  {
    return -1; // no solar forecast data
  }

  bool foundPvData = false;
  uint32_t ts = systemState->ts - (systemState->ts % SECONDS_PER_HOUR); // start with timestamp of last full hour

  for (; i < SOLAR_FORECAST_HOURS; i++)
  {
    if ((foundPvData = (systemState->pv_forecast_ts_wh[i][0] == ts))) // seek to pv forecast upon system ts
    {
      break;
    }
  }
  return foundPvData ? i : -1;
}

static uint16_t getConsumptionWh(SystemConfig *systemConfig, SystemState *systemState, uint32_t utc_ts, uint16_t seconds)
{
  time_t local_ts = (time_t)(utc_ts + systemState->utc_offset);
  struct tm lt;
  gmtime_r(&local_ts, &lt);
  uint16_t stat = systemConfig->cons_stats_Wh[lt.tm_wday][lt.tm_hour];
  uint16_t cons = (stat > 0) ? stat : systemState->cons_W_norm;
  return (uint16_t)((uint32_t)seconds * cons / SECONDS_PER_HOUR);
}

static BatteryState predictBatteryCapacityState(SystemConfig *systemConfig, SystemState *systemState)
{
  short index = findPvForecastData(systemState);
  if (index == -1)
  {
    return BatteryState{0, BatteryLevel::Min}; // no solar forecast data, assume battery will become empty
  }

  uint16_t hysteresis_Wh = systemConfig->loadPower_W >> 2; // required capacity (Wh) if load is switched on for 15min
  uint32_t cap_bat_sim_wh = systemState->cap_bat_Wh;
  uint16_t cap_bat_min_Wh = (systemConfig->bat_soc_min * systemState->cap_bat_max_Wh / 100) + (systemState->switchEnabled ? 0 : hysteresis_Wh);
  uint32_t ts = systemState->ts - (systemState->ts % SECONDS_PER_HOUR); // full hour

  uint16_t seconds = ts + SECONDS_PER_HOUR - systemState->ts;                          // remaining seconds in this hour
  uint32_t wh = seconds * systemState->pv_forecast_ts_wh[index][1] / SECONDS_PER_HOUR; // remaining pv production in this hour
  uint16_t cons_wh = getConsumptionWh(systemConfig, systemState, ts, seconds);         // remaining consumption in this hour

  DEBUGF("%d => %u %u (s) %s %u/%u (Wh)\n", index, ts, systemState->ts, toDate(ts), wh, systemState->pv_forecast_ts_wh[index][1]);

  uint8_t hour = 0;
  for (index++; index < SOLAR_FORECAST_HOURS; index++, hour++)
  {
    // adaptive weight
    float weight = 1.0f - ((float)(hour) / (float)SOLAR_FORECAST_HOURS);

    wh = wh * SOLAR_FORECAST_SAFETY_FACTOR * weight; // apply safety factor + adaptive weight to production forecast

    uint32_t cap_bat_sim_previos_wh = cap_bat_sim_wh; // safe previous to check battery capacity trend

    // cumulate battery capacity upon production forecast in range [0..<bat max capacity>]
    cap_bat_sim_wh = MIN((uint16_t)MAX(0, (int)cap_bat_sim_wh + MIN(systemState->inv_max_w, (int)wh - cons_wh)), systemState->cap_bat_max_Wh);
    DEBUGF("%d => %u (s) %s %u Wh (pv) %u Wh (bat) %u Wh (min) %u Wh (hys) %u Wh (cons) usoc: %u%%\n", index, ts, toDate(ts), wh, cap_bat_sim_wh, cap_bat_min_Wh, hysteresis_Wh, cons_wh, cap_bat_sim_wh * 100 / systemState->cap_bat_max_Wh);

    // capacity below expected min capacity and no positive battery capacity trend
    if (cap_bat_sim_wh < cap_bat_min_Wh && cap_bat_sim_wh < cap_bat_sim_previos_wh)
    {
      return BatteryState{hour, BatteryLevel::Min};
    }
    if (cap_bat_sim_wh == systemState->cap_bat_max_Wh)
    {
      return BatteryState{hour, BatteryLevel::Max};
    }

    ts = systemState->pv_forecast_ts_wh[index][0];
    wh = systemState->pv_forecast_ts_wh[index][1];
    cons_wh = getConsumptionWh(systemConfig, systemState, ts, SECONDS_PER_HOUR); // next turn is full hour, so consumption is Wh
  }
  DEBUGF("capacity %u Wh (bat) %u Wh (min) %u Wh (hys) %u Wh at %s\n", cap_bat_sim_wh, cap_bat_min_Wh, hysteresis_Wh, cons_wh, toDate(ts));
  return BatteryState{
      hour,
      (cap_bat_sim_wh < cap_bat_min_Wh ? BatteryLevel::Min : BatteryLevel::Balanced),
  };
}

const char *const EVENTS[] = {
    NULL,
    "SoC %0% - boiler temperature %1°C < %6°C (min) reached",
    "SoC %0% - invalid data, error was %8",
    "SoC %0% - consumption %2W too high, to much grid purchase %3W",
    "SoC %0% - consumption %2W, battery min SoC %4% reached%7",
    "SoC %0% - boiler temperature %1°C >= %5°C (max) reached",
    "SoC %0% - battery will full charge, but SoC too low",
};

static bool isSurplusAvailable(SystemConfig *systemConfig, SystemState *systemState)
{
  return (systemState->prod_W - systemState->cons_W_nom) > 0;
}

// is surplus "wasted" to grid? - if battery is not loaded - e.g. summer mode
static bool isWasteExceedsLoad(SystemConfig *systemConfig, SystemState *systemState)
{
  return systemState->gridFeedIn_W >= (systemConfig->loadPower_W + systemConfig->gridMin_W);
}

// aware of max system power (production + max inverter power) or if surplus ("waste") exceeds load
static bool isEnoughPowerAvailable(SystemConfig *systemConfig, SystemState *systemState)
{
  uint16_t consumption = systemState->cons_W_nom + systemConfig->loadPower_W;
  return (systemState->system_Power_W - consumption) > systemConfig->gridMin_W;
}

static bool isBoilerOnThreshold(SystemState *systemState, float temperature_on)
{
  return systemState->boiler_T_cur < temperature_on;
}

static bool isBoilerOffThreshold(SystemState *systemState, float temperature_off)
{
  return systemState->boiler_T_cur >= temperature_off;
}

static uint16_t inverterLatencyCnt = 0;

static bool determineDesiredState(char *msg, int len, SystemConfig *systemConfig, SystemState *systemState, SystemStatus status)
{

  bool desiredState = systemState->switchEnabled;

  uint8_t event = 0;

  float temp_on = (systemState->boiler_T_max + systemState->boiler_T_nom) / 2 - BOILER_TEMPERATURE_HYSTERESIS;
  float temp_off = (systemState->boiler_T_max + systemState->boiler_T_nom) / 2 - 1.2f; // ~0.8 °C heater "afterglow"

  BatteryState state = predictBatteryCapacityState(systemConfig, systemState);

  if (systemState->switchEnabled)
  {
    // inverters take some time until load is compensated, so we check whether grid purchase is active? (negative grid feed in denotes purchase)
    // if so, we count until the system specific threshold is reached. if so, we switch off, because the load could not be driven
    inverterLatencyCnt = (systemState->gridFeedIn_W < -systemConfig->gridMin_W) ? inverterLatencyCnt + 1 : 0;

    if (((event = 2) && status != SystemStatus::Ok) ||
        ((event = 3) && (inverterLatencyCnt > SONNEN_INVERTER_LATENCY_COUNT)) || // latency count reached?
        ((event = 5) && isBoilerOffThreshold(systemState, temp_off)) ||
        ((event = 4) && state.level == BatteryLevel::Min && !isSurplusAvailable(systemConfig, systemState)) ||
        ((event = 6) && state.level == BatteryLevel::Max && systemState->usoc <= BATTERY_MIN_USOC))
    {
      desiredState = false;
      inverterLatencyCnt = 0;
    }
  }
  else
  {
    if (((event = 1) && isBoilerOnThreshold(systemState, temp_on)) &&
        (isWasteExceedsLoad(systemConfig, systemState) ||
         (isEnoughPowerAvailable(systemConfig, systemState) &&
          systemState->usoc > BATTERY_MAX_USOC &&
          state.level != BatteryLevel::Min)) &&
        (inverterLatencyCnt++ > SONNEN_INVERTER_LATENCY_COUNT)) // stable
    {
      desiredState = true;
      inverterLatencyCnt = 0;
    }
  }

  if (event != 0)
  {
    char label[16];
    snprintf(label, sizeof(label), state.hours > 0 ? " in ~%dh" : "", state.hours);

    Arg args[] = {
        ARG_UINT8, &systemState->usoc,
        ARG_FLT, &systemState->boiler_T_cur,
        ARG_UINT, &systemState->cons_W_norm,
        ARG_INT, &systemState->gridFeedIn_W,
        ARG_UINT8, &systemConfig->bat_soc_min,
        ARG_FLT, &temp_off,
        ARG_FLT, &temp_on,
        ARG_STR, &label,
        ARG_STR, (void *)SystemStatusLabel[status]};

    format_indexed(msg, len, EVENTS[event], args);
  }
  return desiredState;
}

#endif