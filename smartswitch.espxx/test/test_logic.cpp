#include <unity.h>
#include <stdio.h>
#include <stdarg.h>
#include <Logic.h>

uint32_t ts = 0;
SystemState systemState;
SystemConfig systemConfig;

void prepareForecast(int count, int *data);

void tearDown(void)
{
  // clean stuff up here
}

static void assertStaysStable(bool expectedState)
{
  char msg[80];
  for (int i = 0; i <= SONNEN_INVERTER_LATENCY_COUNT; i++)
  {
    systemState.ts += 5;
    systemState.cons_W += 16;
    updateSystemState(&systemConfig, &systemState);
    bool state = determineDesiredState(msg, sizeof(msg), &systemConfig, &systemState, SystemStatus::Ok);
    TEST_ASSERT_EQUAL(expectedState, state);
  }
}

static bool determineState(char *msg, size_t size)
{
  bool state = systemState.switchEnabled;
  for (int i = 0; i <= SONNEN_INVERTER_LATENCY_COUNT + 1; i++) // +1 will switch
  {
    updateSystemState(&systemConfig, &systemState);
    bool newState = determineDesiredState(msg, size, &systemConfig, &systemState, SystemStatus::Ok);
    if (state != newState)
    {
      return newState;
    }
  }
  return state;
}

void test_upateConsumption()
{
  systemState.cons_W = 33;
  updateSystemState(&systemConfig, &systemState);

  TEST_ASSERT_EQUAL(30, systemState.cons_W_nom);
  TEST_ASSERT_EQUAL(30, systemState.cons_W_norm);

  systemState.cons_W = 103;
  updateSystemState(&systemConfig, &systemState);
  TEST_ASSERT_EQUAL(100, systemState.cons_W_nom);
  TEST_ASSERT_EQUAL(100, systemState.cons_W_norm);

  systemState.cons_W = 199;
  updateSystemState(&systemConfig, &systemState);
  TEST_ASSERT_EQUAL(200, systemState.cons_W_nom);
  TEST_ASSERT_EQUAL(200, systemState.cons_W_norm);

  systemState.cons_W = 207;
  updateSystemState(&systemConfig, &systemState);
  TEST_ASSERT_EQUAL(210, systemState.cons_W_nom);
  TEST_ASSERT_EQUAL(210, systemState.cons_W_norm);
}

void test_predictBatteryCapacityStateNoData()
{
  systemState.pv_forecast_ts_wh[0][0] = 0;

  updateSystemState(&systemConfig, &systemState);
  TEST_ASSERT_EQUAL(BatteryLevel::Min, predictBatteryCapacityState(&systemConfig, &systemState).level);
}

void test_predictBatteryCapacityStateSwitchOff()
{
  systemState.ts = 1762598185;
  systemState.cap_bat_Wh = 2500;
  systemState.cons_W = 397;
  systemState.switchEnabled = true;

  updateSystemState(&systemConfig, &systemState);
  TEST_ASSERT_EQUAL(BatteryLevel::Min, predictBatteryCapacityState(&systemConfig, &systemState).level);
}

void test_predictBatteryCapacityStateSwitchOn()
{
  systemState.ts = 1762598185;
  systemState.cap_bat_Wh = 4100;
  systemState.cons_W = 298;
  systemState.switchEnabled = false;
  systemState.prod_W = 2150;

  updateSystemState(&systemConfig, &systemState);
  TEST_ASSERT_NOT_EQUAL(BatteryLevel::Min, predictBatteryCapacityState(&systemConfig, &systemState).level);
}

void test_predictBatteryCapacityStateSwitchOffOn()
{
  systemState.ts = 1762598185;
  systemState.cap_bat_Wh = 4100;
  systemState.cons_W = 246;
  systemState.switchEnabled = false;
  updateSystemState(&systemConfig, &systemState);
  TEST_ASSERT_NOT_EQUAL(BatteryLevel::Min, predictBatteryCapacityState(&systemConfig, &systemState).level);

  systemState.switchEnabled = true;
  systemState.cons_W = systemConfig.loadPower_W + 253;
  updateSystemState(&systemConfig, &systemState);
  TEST_ASSERT_NOT_EQUAL(BatteryLevel::Min, predictBatteryCapacityState(&systemConfig, &systemState).level);
}

void test_predictBatteryCapacityStateSwitchHysteresis()
{
  systemState.cap_bat_Wh = 4051;

  systemState.ts = 1762598185 + 26 * 3600 - 44; // reach min capacity + hysteresis
  systemState.cons_W = 245;
  systemState.switchEnabled = false;
  updateSystemState(&systemConfig, &systemState);
  TEST_ASSERT_NOT_EQUAL(BatteryLevel::Min, predictBatteryCapacityState(&systemConfig, &systemState).level);

  systemState.ts = 1762598185 + 26 * 3600 + 10 * 60; // 10 min
  systemState.cons_W = systemConfig.loadPower_W + 261;
  systemState.switchEnabled = true;
  updateSystemState(&systemConfig, &systemState);
  TEST_ASSERT_NOT_EQUAL(BatteryLevel::Min, predictBatteryCapacityState(&systemConfig, &systemState).level);
}

void test_predictBatteryCapacityStateSwitchHysteresisAvoidFlicker()
{
  systemConfig.loadPower_W = 3249;
  systemState.cap_bat_Wh = 4124;

  systemState.ts = 1762598185 + 26 * 3600 - 44; // reach min capacity + hysteresis
  systemState.cons_W = 342;
  systemState.switchEnabled = false;
  updateSystemState(&systemConfig, &systemState);
  TEST_ASSERT_NOT_EQUAL(BatteryLevel::Min, predictBatteryCapacityState(&systemConfig, &systemState).level);

  systemState.ts = 1762598185 + 26 * 3600 - 44 + 10; // 10s later
  systemState.cons_W = systemConfig.loadPower_W + 352;
  systemState.switchEnabled = true;
  updateSystemState(&systemConfig, &systemState);
  TEST_ASSERT_NOT_EQUAL(BatteryLevel::Min, predictBatteryCapacityState(&systemConfig, &systemState).level);
}

void test_determineDesiredStateSwitchOffSystemStatusError_Network()
{
  char msg[80];

  systemState.switchEnabled = true;
  updateSystemState(&systemConfig, &systemState);
  bool state = determineDesiredState(msg, sizeof(msg), &systemConfig, &systemState, SystemStatus::Error_Network);
  TEST_ASSERT_FALSE(state);
  TEST_ASSERT_EQUAL_STRING("SoC 41% - invalid data, error was Network", msg);
}

void test_determineDesiredStateSwitchOffSystemStatusError_Battery()
{
  char msg[80];

  systemState.switchEnabled = true;
  updateSystemState(&systemConfig, &systemState);
  bool state = determineDesiredState(msg, sizeof(msg), &systemConfig, &systemState, SystemStatus::Error_Battery);
  TEST_ASSERT_FALSE(state);
  TEST_ASSERT_EQUAL_STRING("SoC 41% - invalid data, error was Battery", msg);
}

void test_determineDesiredStateSwitchOffSystemStatusError_Boiler()
{
  char msg[80];

  systemState.switchEnabled = true;
  updateSystemState(&systemConfig, &systemState);
  bool state = determineDesiredState(msg, sizeof(msg), &systemConfig, &systemState, SystemStatus::Error_Boiler);
  TEST_ASSERT_FALSE(state);
  TEST_ASSERT_EQUAL_STRING("SoC 41% - invalid data, error was Boilder", msg);
}

void test_determineDesiredStateBatteryTargetFulfilled()
{
  systemState.ts = 1762598185 + 26 * 3600 - 44; // reach min capacity + hysteresis
  systemConfig.loadPower_W = 3249;
  systemState.cap_bat_Wh = 4124;
  systemState.usoc = (systemState.cap_bat_Wh * 100 / systemState.cap_bat_max_Wh);
  systemState.cons_W = 247;
  systemState.prod_W = 1850;
  systemState.gridFeedIn_W = systemState.prod_W - systemState.cons_W;
  systemState.switchEnabled = false;

  char msg[80];

  bool state = determineState(msg, sizeof(msg));
  TEST_ASSERT_TRUE(state);
  TEST_ASSERT_EQUAL_STRING("SoC 41% - boiler temperature 54.00°C < 58.00°C (min) reached", msg);

  systemState.switchEnabled = state;
  updateSystemState(&systemConfig, &systemState);
  state = determineDesiredState(msg, sizeof(msg), &systemConfig, &systemState, SystemStatus::Ok);
  TEST_ASSERT_TRUE(state); // assert stable

  systemState.switchEnabled = state;
  updateSystemState(&systemConfig, &systemState);
  state = determineDesiredState(msg, sizeof(msg), &systemConfig, &systemState, SystemStatus::Ok);
  TEST_ASSERT_TRUE(state); // assert stable

  systemState.cons_W = 247 + systemConfig.loadPower_W + 30;
  systemState.gridFeedIn_W = systemState.prod_W - systemState.cons_W;

  systemState.switchEnabled = state;
  state = determineState(msg, sizeof(msg));
  TEST_ASSERT_FALSE(state);
}

void test_determineDesiredStateFullchargeRequestedWithLatencyCount()
{
  systemState.ts = 1762556400 + 47 * 3600; // reach min capacity + hysteresis
  systemConfig.loadPower_W = 3249;
  systemState.cap_bat_Wh = 5575;
  systemState.usoc = 41;
  systemState.cons_W = 1447;
  systemState.prod_W = 2150;
  systemState.gridFeedIn_W = systemState.prod_W - systemState.cons_W;
  systemState.switchEnabled = false;

  char msg[80];
  systemState.ts += 5;
  bool state = determineState(msg, sizeof(msg));
  TEST_ASSERT_TRUE(state);
  TEST_ASSERT_EQUAL_STRING("SoC 41% - boiler temperature 54.00°C < 58.00°C (min) reached", msg);

  systemState.switchEnabled = state;
  systemState.fullChargeRequest = true;
  systemState.gridFeedIn_W = systemState.prod_W - systemState.cons_W - systemConfig.loadPower_W;

  updateSystemState(&systemConfig, &systemState);
  state = determineDesiredState(msg, sizeof(msg), &systemConfig, &systemState, SystemStatus::Ok);
  TEST_ASSERT_TRUE(state);
  state = determineDesiredState(msg, sizeof(msg), &systemConfig, &systemState, SystemStatus::Ok);
  TEST_ASSERT_TRUE(state);
  state = determineDesiredState(msg, sizeof(msg), &systemConfig, &systemState, SystemStatus::Ok);
  TEST_ASSERT_TRUE(state);
  state = determineDesiredState(msg, sizeof(msg), &systemConfig, &systemState, SystemStatus::Ok);
  TEST_ASSERT_TRUE(state);
  state = determineDesiredState(msg, sizeof(msg), &systemConfig, &systemState, SystemStatus::Ok);
  TEST_ASSERT_FALSE(state); // switch off, load cannot be driven
  TEST_ASSERT_EQUAL_STRING("SoC 41% - consumption 1450W too high, to much grid purchase -2546W", msg);
}

void test_determineDesiredStateSurplusWaste()
{
  systemState.ts = 1762556400 + 12 * 3600;
  systemConfig.loadPower_W = 3251;
  systemState.cap_bat_Wh = 0;
  systemState.usoc = 0;
  systemState.cons_W = 347;
  systemState.prod_W = systemConfig.loadPower_W + systemState.cons_W + systemConfig.gridMin_W;
  systemState.gridFeedIn_W = systemState.prod_W - systemState.cons_W;

  char msg[90];

  systemState.switchEnabled = false;
  bool state = determineState(msg, sizeof(msg));
  TEST_ASSERT_TRUE(state);

  systemState.switchEnabled = state;
  systemState.cons_W = 363 + systemConfig.loadPower_W;
  updateSystemState(&systemConfig, &systemState);
  state = determineState(msg, sizeof(msg));
  assertStaysStable(true);

  systemState.switchEnabled = state;
  systemState.cons_W = 450 + systemConfig.loadPower_W;
  systemState.usoc = 25;

  state = determineState(msg, sizeof(msg));
  TEST_ASSERT_EQUAL_STRING("SoC 25% - consumption 449W, battery min capacity 2000Wh reached in ~12h", msg);
  TEST_ASSERT_FALSE(state);
}

void test_determineDesiredStateSurplusWasteNoForecastData()
{
  systemState.ts = 1762556400 + 12 * 3600;
  systemConfig.loadPower_W = 3251;
  systemState.cap_bat_Wh = 0;
  systemState.usoc = 0;
  systemState.cons_W = 347;
  systemState.prod_W = systemConfig.loadPower_W + systemState.cons_W + systemConfig.gridMin_W;
  systemState.gridFeedIn_W = systemState.prod_W - systemState.cons_W;

  char msg[80];
  systemState.pv_forecast_ts_wh[0][0] = 0;

  systemState.switchEnabled = false;
  bool state = determineState(msg, sizeof(msg));
  TEST_ASSERT_TRUE(state);
}

void test_determineDesiredStateBelowMinCapacityButSurplusWillFullCharge()
{
  char msg[90];

  systemState.ts = 1762556400 + 34 * 3600;
  systemConfig.loadPower_W = 3251;
  systemState.cons_W = 247;
  systemState.prod_W = 2500;
  systemState.gridFeedIn_W = systemState.prod_W - systemState.cons_W;

  systemState.cap_bat_Wh = 99;
  systemState.usoc = 1;
  systemState.switchEnabled = true;

  bool state = determineState(msg, sizeof(msg));
  TEST_ASSERT_EQUAL_STRING("SoC 1% - battery will full charge, but SoC too low", msg);
  TEST_ASSERT_FALSE(state);

  systemState.ts = 1762556400 + 35 * 3600;
  systemState.cap_bat_Wh = 700;
  systemState.usoc = 11;
  systemState.switchEnabled = false;

  state = determineState(msg, sizeof(msg));
  TEST_ASSERT_TRUE(state);

  systemState.switchEnabled = state;
  systemState.ts += 300;
  systemState.gridFeedIn_W = -100;

  assertStaysStable(true);

  systemState.cap_bat_Wh = 400;
  systemState.usoc = 5;
  systemState.ts = 1762556400 + 32 * 3600;
  systemState.prod_W = 230;
  systemState.gridFeedIn_W = -50;

  state = determineState(msg, sizeof(msg));
  TEST_ASSERT_EQUAL_STRING("SoC 5% - consumption 330W, battery min capacity 2000Wh reached in ~0h", msg);
  TEST_ASSERT_FALSE(state);
  assertStaysStable(false);
}

void test_determineDesiredState_BatteryCapacityFinallyBelowMinCapacity()
{
  char msg[80];

  systemState.ts = 1762556400 + (SOLAR_FORECAST_HOURS - 5) * 3600;
  systemConfig.loadPower_W = 3251;
  systemState.cons_W = 340;
  systemState.prod_W = 1050;
  systemState.gridFeedIn_W = systemState.prod_W - systemState.cons_W;

  systemState.cap_bat_Wh = 1700;
  systemState.usoc = 17;
  systemState.switchEnabled = false;

  int pvData[] = {500, 670, 500, 600, 500};
  prepareForecast(sizeof(pvData) / sizeof(int), pvData);

  bool state = determineState(msg, sizeof(msg));
  TEST_ASSERT_FALSE(state);
}

void test_determineDesiredState_BatteryCapacityFinallyAboveMinCapacityButUsocTooLow()
{
  char msg[80];

  int pvData[] = {500, 500, 500, 970, 1200, 1300, 1200, 1100, 1023};
  int len = sizeof(pvData) / sizeof(int);

  systemState.ts = 1762556400 + (SOLAR_FORECAST_HOURS - len) * 3600;
  systemConfig.loadPower_W = 3251;
  systemState.cons_W = 350;
  systemState.prod_W = 1050;
  systemState.gridFeedIn_W = systemState.prod_W - systemState.cons_W;

  systemState.cap_bat_Wh = 100;
  systemState.usoc = 1;
  systemState.switchEnabled = false;

  prepareForecast(len, pvData);

  bool state = determineState(msg, sizeof(msg));
  TEST_ASSERT_FALSE(state);
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();

  RUN_TEST(test_upateConsumption);
  RUN_TEST(test_predictBatteryCapacityStateNoData);
  RUN_TEST(test_predictBatteryCapacityStateSwitchOff);
  RUN_TEST(test_predictBatteryCapacityStateSwitchOn);
  RUN_TEST(test_predictBatteryCapacityStateSwitchOffOn);
  RUN_TEST(test_predictBatteryCapacityStateSwitchHysteresis);
  RUN_TEST(test_predictBatteryCapacityStateSwitchHysteresisAvoidFlicker);

  RUN_TEST(test_determineDesiredStateSwitchOffSystemStatusError_Network);
  RUN_TEST(test_determineDesiredStateSwitchOffSystemStatusError_Battery);
  RUN_TEST(test_determineDesiredStateSwitchOffSystemStatusError_Boiler);

  RUN_TEST(test_determineDesiredStateBatteryTargetFulfilled);
  RUN_TEST(test_determineDesiredStateFullchargeRequestedWithLatencyCount);
  RUN_TEST(test_determineDesiredStateSurplusWaste);
  RUN_TEST(test_determineDesiredStateSurplusWasteNoForecastData);
  RUN_TEST(test_determineDesiredStateBelowMinCapacityButSurplusWillFullCharge);

  RUN_TEST(test_determineDesiredState_BatteryCapacityFinallyBelowMinCapacity);
  RUN_TEST(test_determineDesiredState_BatteryCapacityFinallyAboveMinCapacityButUsocTooLow);

  return UNITY_END();
}

void prepareForecast(int count, int *data)
{
  for (int i = 0; i < SOLAR_FORECAST_HOURS; i++)
  {
    systemState.pv_forecast_ts_wh[i][0] = 1762556400 + i * 3600;
    systemState.pv_forecast_ts_wh[i][1] = i >= (SOLAR_FORECAST_HOURS - count) ? data[count - (SOLAR_FORECAST_HOURS - i)] : 0;
  }
}

void setUp(void)
{
  int weatherFactor = 2;

  systemConfig.loadPower_W = 3100;
  systemConfig.gridMin_W = 100;
  systemConfig.cap_bat_min_Wh = 2000;

  systemState.cap_bat_max_Wh = 9900;
  systemState.inv_max_w = 4600;
  systemState.utc_offset = 3600;
  systemState.fullChargeRequest = false;
  systemState.cap_bat_Wh = 4100;
  systemState.usoc = 41;

  systemState.boiler_T_max = 65.0f;
  systemState.boiler_T_nom = 55.0f;
  systemState.boiler_T_cur = 54.0f;

  int r = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762556400;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762560000;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762563600;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762567200;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762570800;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762574400;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762578000;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762581600;
  systemState.pv_forecast_ts_wh[r++][1] = 15;
  systemState.pv_forecast_ts_wh[r][0] = 1762585200;
  systemState.pv_forecast_ts_wh[r++][1] = 143;
  systemState.pv_forecast_ts_wh[r][0] = 1762588800;
  systemState.pv_forecast_ts_wh[r++][1] = 272;
  systemState.pv_forecast_ts_wh[r][0] = 1762592400;
  systemState.pv_forecast_ts_wh[r++][1] = 860;
  systemState.pv_forecast_ts_wh[r][0] = 1762596000;
  systemState.pv_forecast_ts_wh[r++][1] = 1129;
  systemState.pv_forecast_ts_wh[r][0] = 1762599600;
  systemState.pv_forecast_ts_wh[r++][1] = 1464;
  systemState.pv_forecast_ts_wh[r][0] = 1762603200;
  systemState.pv_forecast_ts_wh[r++][1] = 1950;
  systemState.pv_forecast_ts_wh[r][0] = 1762606800;
  systemState.pv_forecast_ts_wh[r++][1] = 1909;
  systemState.pv_forecast_ts_wh[r][0] = 1762610400;
  systemState.pv_forecast_ts_wh[r++][1] = 1349;
  systemState.pv_forecast_ts_wh[r][0] = 1762614000;
  systemState.pv_forecast_ts_wh[r++][1] = 979;
  systemState.pv_forecast_ts_wh[r][0] = 1762617600;
  systemState.pv_forecast_ts_wh[r++][1] = 426;
  systemState.pv_forecast_ts_wh[r][0] = 1762621200;
  systemState.pv_forecast_ts_wh[r++][1] = 129;
  systemState.pv_forecast_ts_wh[r][0] = 1762624800;
  systemState.pv_forecast_ts_wh[r++][1] = 20;
  systemState.pv_forecast_ts_wh[r][0] = 1762628400;
  systemState.pv_forecast_ts_wh[r++][1] = 3;
  systemState.pv_forecast_ts_wh[r][0] = 1762632000;
  systemState.pv_forecast_ts_wh[r++][1] = 1;
  systemState.pv_forecast_ts_wh[r][0] = 1762635600;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762639200;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762642800;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762646400;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762650000;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762653600;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762657200;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762660800;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762664400;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762668000;
  systemState.pv_forecast_ts_wh[r++][1] = 8;
  systemState.pv_forecast_ts_wh[r][0] = 1762671600;
  systemState.pv_forecast_ts_wh[r++][1] = 75 * weatherFactor;
  systemState.pv_forecast_ts_wh[r][0] = 1762675200;
  systemState.pv_forecast_ts_wh[r++][1] = 271 * weatherFactor;
  systemState.pv_forecast_ts_wh[r][0] = 1762678800;
  systemState.pv_forecast_ts_wh[r++][1] = 548 * weatherFactor;
  systemState.pv_forecast_ts_wh[r][0] = 1762682400;
  systemState.pv_forecast_ts_wh[r++][1] = 802 * weatherFactor;
  systemState.pv_forecast_ts_wh[r][0] = 1762686000;
  systemState.pv_forecast_ts_wh[r++][1] = 1011 * weatherFactor;
  systemState.pv_forecast_ts_wh[r][0] = 1762689600;
  systemState.pv_forecast_ts_wh[r++][1] = 1145 * weatherFactor;
  systemState.pv_forecast_ts_wh[r][0] = 1762693200;
  systemState.pv_forecast_ts_wh[r++][1] = 1123 * weatherFactor;
  systemState.pv_forecast_ts_wh[r][0] = 1762696800;
  systemState.pv_forecast_ts_wh[r++][1] = 1008 * weatherFactor;
  systemState.pv_forecast_ts_wh[r][0] = 1762700400;
  systemState.pv_forecast_ts_wh[r++][1] = 791 * weatherFactor;
  systemState.pv_forecast_ts_wh[r][0] = 1762704000;
  systemState.pv_forecast_ts_wh[r++][1] = 363 * weatherFactor;
  systemState.pv_forecast_ts_wh[r][0] = 1762707600;
  systemState.pv_forecast_ts_wh[r++][1] = 57 * weatherFactor;
  systemState.pv_forecast_ts_wh[r][0] = 1762711200;
  systemState.pv_forecast_ts_wh[r++][1] = 9;
  systemState.pv_forecast_ts_wh[r][0] = 1762714800;
  systemState.pv_forecast_ts_wh[r++][1] = 1;
  systemState.pv_forecast_ts_wh[r][0] = 1762718400;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762722000;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762725600;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
  systemState.pv_forecast_ts_wh[r][0] = 1762729200;
  systemState.pv_forecast_ts_wh[r++][1] = 0;
}
