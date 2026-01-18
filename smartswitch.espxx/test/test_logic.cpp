#include <unity.h>
#include <stdio.h>
#include <Logic.h>

uint32_t ts = 0;
SystemState systemState;
SystemConfig systemConfig;

void tearDown(void)
{
  // clean stuff up here
}

void test_upateConsumption()
{

  systemState.cons_W = 33;
  updateConsumption(&systemConfig, &systemState);

  TEST_ASSERT_EQUAL(30, systemState.cons_W_nom);
  TEST_ASSERT_EQUAL(30, systemState.cons_W_norm);

  systemState.cons_W = 103;
  updateConsumption(&systemConfig, &systemState);
  TEST_ASSERT_EQUAL(100, systemState.cons_W_nom);
  TEST_ASSERT_EQUAL(100, systemState.cons_W_norm);

  systemState.cons_W = 199;
  updateConsumption(&systemConfig, &systemState);
  TEST_ASSERT_EQUAL(200, systemState.cons_W_nom);
  TEST_ASSERT_EQUAL(200, systemState.cons_W_norm);

  systemState.cons_W = 207;
  updateConsumption(&systemConfig, &systemState);
  TEST_ASSERT_EQUAL(210, systemState.cons_W_nom);
  TEST_ASSERT_EQUAL(210, systemState.cons_W_norm);
}

void test_batteryCapacityTargetFulfilledNoData()
{
  updateConsumption(&systemConfig, &systemState);
  TEST_ASSERT_FALSE(batteryCapacityTargetFulfilled(&systemConfig, &systemState, &ts));
}

void test_batteryCapacityTargetFulfilledSwitchOff()
{
  systemState.ts = 1762598185;
  systemState.cap_bat_Wh = 2600;
  systemState.cons_W = 397;
  systemState.switchEnabled = true;

  updateConsumption(&systemConfig, &systemState);
  TEST_ASSERT_FALSE(batteryCapacityTargetFulfilled(&systemConfig, &systemState, &ts));
}

void test_batteryCapacityTargetFulfilledSwitchOn()
{
  systemState.ts = 1762598185;
  systemState.cap_bat_Wh = 3200;
  systemState.cons_W = 298;
  systemState.switchEnabled = false;

  updateConsumption(&systemConfig, &systemState);
  TEST_ASSERT_TRUE(batteryCapacityTargetFulfilled(&systemConfig, &systemState, &ts));
}

void test_batteryCapacityTargetFulfilledSwitchOffOn()
{
  systemState.ts = 1762598185;
  systemState.cap_bat_Wh = 3200;
  systemState.cons_W = 246;
  systemState.switchEnabled = false;
  updateConsumption(&systemConfig, &systemState);
  TEST_ASSERT_TRUE(batteryCapacityTargetFulfilled(&systemConfig, &systemState, &ts));

  systemState.switchEnabled = true;
  systemState.cons_W = systemConfig.loadPower_W + 253;
  updateConsumption(&systemConfig, &systemState);
  TEST_ASSERT_TRUE(batteryCapacityTargetFulfilled(&systemConfig, &systemState, &ts));
}

void test_batteryCapacityTargetFulfilledSwitchHysteresis()
{
  systemState.cap_bat_Wh = 3100;

  systemState.ts = 1762598185 + 26 * 3600 - 44; // reach min capacity + hysteresis
  systemState.cons_W = 245;
  systemState.switchEnabled = false;
  updateConsumption(&systemConfig, &systemState);
  TEST_ASSERT_TRUE(batteryCapacityTargetFulfilled(&systemConfig, &systemState, &ts));

  systemState.ts = 1762598185 + 26 * 3600 + 10 * 60; // 10 min
  systemState.cons_W = systemConfig.loadPower_W + 261;
  systemState.switchEnabled = true;
  updateConsumption(&systemConfig, &systemState);
  TEST_ASSERT_TRUE(batteryCapacityTargetFulfilled(&systemConfig, &systemState, &ts));
}

void test_batteryCapacityTargetFulfilledSwitchHysteresisAvoidFlicker()
{
  systemConfig.loadPower_W = 3249;
  systemState.cap_bat_Wh = 3100;

  systemState.ts = 1762598185 + 26 * 3600 - 44; // reach min capacity + hysteresis
  systemState.cons_W = 342;
  systemState.switchEnabled = false;
  updateConsumption(&systemConfig, &systemState);
  TEST_ASSERT_TRUE(batteryCapacityTargetFulfilled(&systemConfig, &systemState, &ts));

  systemState.ts = 1762598185 + 26 * 3600 - 44 + 10; // 10s later
  systemState.cons_W = systemConfig.loadPower_W + 352;
  systemState.switchEnabled = true;
  updateConsumption(&systemConfig, &systemState);
  TEST_ASSERT_TRUE(batteryCapacityTargetFulfilled(&systemConfig, &systemState, &ts));
}
/*
void test_updateSwitch()
{
  uint32_t ts = 0;
  SystemState systemState;
  SystemConfig systemConfig;

  updateSwitch(&systemConfig, &systemState, true);
  TEST_ASSERT_TRUE(systemState.switchEnabled);

  updateSwitch(&systemConfig, &systemState, false);
  TEST_ASSERT_FALSE(systemState.switchEnabled);
}
*/

int main(int argc, char **argv)
{
  UNITY_BEGIN();

  //RUN_TEST(test_updateSwitch);
  RUN_TEST(test_upateConsumption);
  RUN_TEST(test_batteryCapacityTargetFulfilledNoData);
  RUN_TEST(test_batteryCapacityTargetFulfilledSwitchOff);
  RUN_TEST(test_batteryCapacityTargetFulfilledSwitchOn);
  RUN_TEST(test_batteryCapacityTargetFulfilledSwitchOffOn);
  RUN_TEST(test_batteryCapacityTargetFulfilledSwitchHysteresis);
  RUN_TEST(test_batteryCapacityTargetFulfilledSwitchHysteresisAvoidFlicker);

  return UNITY_END();
}

void setUp(void)
{
  systemConfig.loadPower_W = 3100;
  systemConfig.gridMin_W = 100;
  systemConfig.cap_bat_min_Wh = 2500;
  systemState.cap_bat_max_Wh = 9900;
  systemState.inv_max_w = 4500;
  systemState.utc_offset = 3600;

  int r = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762556400;
  systemState.pv_forecast_wh_h[r++][1] = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762560000;
  systemState.pv_forecast_wh_h[r++][1] = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762563600;
  systemState.pv_forecast_wh_h[r++][1] = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762567200;
  systemState.pv_forecast_wh_h[r++][1] = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762570800;
  systemState.pv_forecast_wh_h[r++][1] = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762574400;
  systemState.pv_forecast_wh_h[r++][1] = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762578000;
  systemState.pv_forecast_wh_h[r++][1] = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762581600;
  systemState.pv_forecast_wh_h[r++][1] = 15;
  systemState.pv_forecast_wh_h[r][0] = 1762585200;
  systemState.pv_forecast_wh_h[r++][1] = 143;
  systemState.pv_forecast_wh_h[r][0] = 1762588800;
  systemState.pv_forecast_wh_h[r++][1] = 272;
  systemState.pv_forecast_wh_h[r][0] = 1762592400;
  systemState.pv_forecast_wh_h[r++][1] = 860;
  systemState.pv_forecast_wh_h[r][0] = 1762596000;
  systemState.pv_forecast_wh_h[r++][1] = 1129;
  systemState.pv_forecast_wh_h[r][0] = 1762599600;
  systemState.pv_forecast_wh_h[r++][1] = 1464;
  systemState.pv_forecast_wh_h[r][0] = 1762603200;
  systemState.pv_forecast_wh_h[r++][1] = 1950;
  systemState.pv_forecast_wh_h[r][0] = 1762606800;
  systemState.pv_forecast_wh_h[r++][1] = 1909;
  systemState.pv_forecast_wh_h[r][0] = 1762610400;
  systemState.pv_forecast_wh_h[r++][1] = 1349;
  systemState.pv_forecast_wh_h[r][0] = 1762614000;
  systemState.pv_forecast_wh_h[r++][1] = 979;
  systemState.pv_forecast_wh_h[r][0] = 1762617600;
  systemState.pv_forecast_wh_h[r++][1] = 426;
  systemState.pv_forecast_wh_h[r][0] = 1762621200;
  systemState.pv_forecast_wh_h[r++][1] = 129;
  systemState.pv_forecast_wh_h[r][0] = 1762624800;
  systemState.pv_forecast_wh_h[r++][1] = 20;
  systemState.pv_forecast_wh_h[r][0] = 1762628400;
  systemState.pv_forecast_wh_h[r++][1] = 3;
  systemState.pv_forecast_wh_h[r][0] = 1762632000;
  systemState.pv_forecast_wh_h[r++][1] = 1;
  systemState.pv_forecast_wh_h[r][0] = 1762635600;
  systemState.pv_forecast_wh_h[r++][1] = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762639200;
  systemState.pv_forecast_wh_h[r++][1] = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762642800;
  systemState.pv_forecast_wh_h[r++][1] = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762646400;
  systemState.pv_forecast_wh_h[r++][1] = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762650000;
  systemState.pv_forecast_wh_h[r++][1] = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762653600;
  systemState.pv_forecast_wh_h[r++][1] = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762657200;
  systemState.pv_forecast_wh_h[r++][1] = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762660800;
  systemState.pv_forecast_wh_h[r++][1] = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762664400;
  systemState.pv_forecast_wh_h[r++][1] = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762668000;
  systemState.pv_forecast_wh_h[r++][1] = 8;
  systemState.pv_forecast_wh_h[r][0] = 1762671600;
  systemState.pv_forecast_wh_h[r++][1] = 75;
  systemState.pv_forecast_wh_h[r][0] = 1762675200;
  systemState.pv_forecast_wh_h[r++][1] = 271;
  systemState.pv_forecast_wh_h[r][0] = 1762678800;
  systemState.pv_forecast_wh_h[r++][1] = 548;
  systemState.pv_forecast_wh_h[r][0] = 1762682400;
  systemState.pv_forecast_wh_h[r++][1] = 802;
  systemState.pv_forecast_wh_h[r][0] = 1762686000;
  systemState.pv_forecast_wh_h[r++][1] = 1011;
  systemState.pv_forecast_wh_h[r][0] = 1762689600;
  systemState.pv_forecast_wh_h[r++][1] = 1145;
  systemState.pv_forecast_wh_h[r][0] = 1762693200;
  systemState.pv_forecast_wh_h[r++][1] = 1123;
  systemState.pv_forecast_wh_h[r][0] = 1762696800;
  systemState.pv_forecast_wh_h[r++][1] = 1008;
  systemState.pv_forecast_wh_h[r][0] = 1762700400;
  systemState.pv_forecast_wh_h[r++][1] = 791;
  systemState.pv_forecast_wh_h[r][0] = 1762704000;
  systemState.pv_forecast_wh_h[r++][1] = 363;
  systemState.pv_forecast_wh_h[r][0] = 1762707600;
  systemState.pv_forecast_wh_h[r++][1] = 57;
  systemState.pv_forecast_wh_h[r][0] = 1762711200;
  systemState.pv_forecast_wh_h[r++][1] = 9;
  systemState.pv_forecast_wh_h[r][0] = 1762714800;
  systemState.pv_forecast_wh_h[r++][1] = 1;
  systemState.pv_forecast_wh_h[r][0] = 1762718400;
  systemState.pv_forecast_wh_h[r++][1] = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762722000;
  systemState.pv_forecast_wh_h[r++][1] = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762725600;
  systemState.pv_forecast_wh_h[r++][1] = 0;
  systemState.pv_forecast_wh_h[r][0] = 1762729200;
  systemState.pv_forecast_wh_h[r++][1] = 0;
}
