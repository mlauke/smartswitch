#ifndef LOGIC_H
#define LOGIC_H

#include <SmartSwitch.h>
#include <time.h>
#include <debug.h>

// TODO
int stdOffset = 3600; // 1h utc offset Europe/Berlin
int dstOffset = 3600; // 1h suummer time offset

char tsfmt[20];
char *toDate(uint32_t utc_ts, uint16_t offset)
{
  time_t time = (time_t)(utc_ts + offset);
  tm *timeinfo = gmtime(&time);
  strftime(tsfmt, sizeof(tsfmt), "%Y-%m-%d %H:%M:%S", timeinfo);
  return tsfmt;
}

char *toDate(uint32_t utc_ts)
{
  return toDate(utc_ts, 0);
}
char *toLocalDate(SystemData *systemData, uint32_t utc_ts)
{
  return toDate(utc_ts, (stdOffset + systemData->dstOffset));
}

void updateConsumption(SystemConfig *systemConfig, SystemData *systemData){
  systemData->cons_W_nom = (systemData->cons_W + 5) / 10 * 10 + systemConfig->gridMin_W;// round up multiple of 10W
  // consumption without load
  systemData->cons_W_norm = (systemData->switchEnabled && systemData->cons_W_nom > systemConfig->loadPower_W)
    ? systemData->cons_W_nom - systemConfig->loadPower_W
    : systemData->cons_W_nom;
}

bool batteryCapacityTargetFulfilled(SystemConfig *systemConfig, SystemData *systemData, uint32_t *ts)
{
  bool foundPvData = false;

  if (systemData->pv_forecast_wh_h[0][0] == 0)
  {
    return foundPvData; // no solar forecast data, assume battery will become empty
  }

  uint16_t hysteresis_Wh = systemConfig->loadPower_W / 6; // Wh if load is switched on for 10min
  uint32_t cap_bat_Wh = systemData->cap_bat_Wh;
  uint16_t cap_bat_min_Wh = systemConfig->cap_bat_min_Wh + (systemData->switchEnabled ? 0 : hysteresis_Wh);

  *ts = systemData->ts - (systemData->ts % 3600); // start timestamp of last full hour

  uint8_t i = 0;
  for (; i < sizeof(systemData->pv_forecast_wh_h) / sizeof(systemData->pv_forecast_wh_h[0]); i++)
  {
    if ((foundPvData = (systemData->pv_forecast_wh_h[i][0] == *ts))) // seek to pv forecast upon system ts
    {
      break;
    }
  }

  uint32_t wh = (*ts + 3600 - systemData->ts) * systemData->pv_forecast_wh_h[i][1] / 3600; // remaining pv production in this hour

  for (i++; i < sizeof(systemData->pv_forecast_wh_h) / sizeof(systemData->pv_forecast_wh_h[0]); i++)
  {
    if (cap_bat_Wh < cap_bat_min_Wh)
    { // capacity below expected min capacity
      break;
    }
    if (cap_bat_Wh == systemData->cap_bat_max_Wh)
    {
      break;
    }
    // cumulate battery capacity upon production forecast in range [0..<bat max capacity>]
    cap_bat_Wh = MIN((uint16_t)MAX(0, (int)cap_bat_Wh + MIN(systemData->inv_max_w, (int)wh - systemData->cons_W_norm)), systemData->cap_bat_max_Wh);
    DEBUG("%d => %u (s) %s %u (Wh) cap_bat %u (Wh) cons %uW usoc: %u%%\n", i, *ts, toDate(*ts), wh, cap_bat_Wh, systemData->cons_W_norm, cap_bat_Wh * 100 / systemData->cap_bat_max_Wh);

    *ts = systemData->pv_forecast_wh_h[i][0];
    wh = systemData->pv_forecast_wh_h[i][1];
  }
  DEBUG("capacity %u Wh (bat) %u Wh (min) %u Wh (hys) %u W at %s\n", cap_bat_Wh, cap_bat_min_Wh, hysteresis_Wh, systemData->cons_W_norm, toLocalDate(systemData, *ts));
  return foundPvData && cap_bat_Wh >= cap_bat_min_Wh;
}

void updateSwitch(SystemConfig *systemConfig, SystemData *systemData, bool validData){
  systemData->switchEnabled = validData;
}

#endif