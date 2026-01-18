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
#include "debug.h"

static char tsfmt[30];
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

void updateConsumption(SystemConfig *systemConfig, SystemState *systemState)
{
  systemState->cons_W_nom = (systemState->cons_W + 5) / 10 * 10; // round up multiple of 10W
  // consumption without load
  systemState->cons_W_norm = (systemState->switchEnabled && systemState->cons_W_nom > systemConfig->loadPower_W)
                                 ? systemState->cons_W_nom - systemConfig->loadPower_W
                                 : systemState->cons_W_nom;
}

static bool batteryCapacityTargetFulfilled(SystemConfig *systemConfig, SystemState *systemState, uint32_t *ts)
{
  bool foundPvData = false;

  if (systemState->pv_forecast_wh_h[0][0] == 0)
  {
    return foundPvData; // no solar forecast data, assume battery will become empty
  }

  uint16_t hysteresis_Wh = systemConfig->loadPower_W / 6; // Wh if load is switched on for 10min
  uint32_t cap_bat_Wh = systemState->cap_bat_Wh;
  uint16_t cap_bat_min_Wh = systemConfig->cap_bat_min_Wh + (systemState->switchEnabled ? 0 : hysteresis_Wh);

  *ts = systemState->ts - (systemState->ts % 3600); // start with timestamp of last full hour

  uint8_t i = 0;
  for (; i < sizeof(systemState->pv_forecast_wh_h) / sizeof(systemState->pv_forecast_wh_h[0]); i++)
  {
    if ((foundPvData = (systemState->pv_forecast_wh_h[i][0] == *ts))) // seek to pv forecast upon system ts
    {
      break;
    }
  }

  uint32_t wh = (*ts + 3600 - systemState->ts) * systemState->pv_forecast_wh_h[i][1] / 3600; // remaining pv production in this hour
  DEBUGF("%d => %u %u (s) %s %u/%u (Wh)\n", i, *ts, systemState->ts, toDate(*ts), wh, systemState->pv_forecast_wh_h[i][1]);
  for (i++; i < sizeof(systemState->pv_forecast_wh_h) / sizeof(systemState->pv_forecast_wh_h[0]); i++)
  {
    if (cap_bat_Wh < cap_bat_min_Wh)
    { // capacity below expected min capacity
      break;
    }
    if (cap_bat_Wh == systemState->cap_bat_max_Wh)
    {
      break;
    }
    // cumulate battery capacity upon production forecast in range [0..<bat max capacity>]
    cap_bat_Wh = MIN((uint16_t)MAX(0, (int)cap_bat_Wh + MIN(systemState->inv_max_w, (int)wh - systemState->cons_W_norm)), systemState->cap_bat_max_Wh);
    DEBUGF("%d => %u (s) %s %u (Wh) cap_bat %u (Wh) cons %uW usoc: %u%%\n", i, *ts, toDate(*ts), wh, cap_bat_Wh, systemState->cons_W_norm, cap_bat_Wh * 100 / systemState->cap_bat_max_Wh);

    *ts = systemState->pv_forecast_wh_h[i][0];
    wh = systemState->pv_forecast_wh_h[i][1];
  }
  DEBUGF("capacity %u Wh (bat) %u Wh (min) %u Wh (hys) %u W at %s\n", cap_bat_Wh, cap_bat_min_Wh, hysteresis_Wh, systemState->cons_W_norm, toDate(*ts));
  return foundPvData && cap_bat_Wh >= cap_bat_min_Wh;
}

#endif