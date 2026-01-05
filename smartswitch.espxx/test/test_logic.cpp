#include <unity.h>
#include <stdio.h>
#include <Logic.h>

uint32_t ts = 0;
SystemData systemData;
SystemConfig systemConfig;

void tearDown(void)
{
  // clean stuff up here
}

void test_upateConsumption()
{

  systemData.cons_W = 33;
  updateConsumption(&systemConfig, &systemData);

  TEST_ASSERT_EQUAL(30, systemData.cons_W_nom);
  TEST_ASSERT_EQUAL(30, systemData.cons_W_norm);

  systemData.cons_W = 103;
  updateConsumption(&systemConfig, &systemData);
  TEST_ASSERT_EQUAL(100, systemData.cons_W_nom);
  TEST_ASSERT_EQUAL(100, systemData.cons_W_norm);

  systemData.cons_W = 199;
  updateConsumption(&systemConfig, &systemData);
  TEST_ASSERT_EQUAL(200, systemData.cons_W_nom);
  TEST_ASSERT_EQUAL(200, systemData.cons_W_norm);

  systemData.cons_W = 207;
  updateConsumption(&systemConfig, &systemData);
  TEST_ASSERT_EQUAL(210, systemData.cons_W_nom);
  TEST_ASSERT_EQUAL(210, systemData.cons_W_norm);
}

void test_batteryCapacityTargetFulfilledNoData()
{
  updateConsumption(&systemConfig, &systemData);
  TEST_ASSERT_FALSE(batteryCapacityTargetFulfilled(&systemConfig, &systemData, &ts));
}

void test_batteryCapacityTargetFulfilledSwitchOff()
{
  systemData.ts = 1762598185;
  systemData.cap_bat_Wh = 2600;
  systemData.cons_W = 397;
  systemData.switchEnabled = true;

  updateConsumption(&systemConfig, &systemData);
  TEST_ASSERT_FALSE(batteryCapacityTargetFulfilled(&systemConfig, &systemData, &ts));
}

void test_batteryCapacityTargetFulfilledSwitchOn()
{
  systemData.ts = 1762598185;
  systemData.cap_bat_Wh = 3200;
  systemData.cons_W = 298;
  systemData.switchEnabled = false;

  updateConsumption(&systemConfig, &systemData);
  TEST_ASSERT_TRUE(batteryCapacityTargetFulfilled(&systemConfig, &systemData, &ts));
}

void test_batteryCapacityTargetFulfilledSwitchOffOn()
{
  systemData.ts = 1762598185;
  systemData.cap_bat_Wh = 3200;
  systemData.cons_W = 246;
  systemData.switchEnabled = false;
  updateConsumption(&systemConfig, &systemData);
  TEST_ASSERT_TRUE(batteryCapacityTargetFulfilled(&systemConfig, &systemData, &ts));

  systemData.switchEnabled = true;
  systemData.cons_W = systemConfig.loadPower_W + 253;
  updateConsumption(&systemConfig, &systemData);
  TEST_ASSERT_TRUE(batteryCapacityTargetFulfilled(&systemConfig, &systemData, &ts));
}

void test_batteryCapacityTargetFulfilledSwitchHysteresis()
{
  systemData.cap_bat_Wh = 3100;

  systemData.ts = 1762598185 + 26 * 3600 - 44; // reach min capacity + hysteresis
  systemData.cons_W = 245;
  systemData.switchEnabled = false;
  updateConsumption(&systemConfig, &systemData);
  TEST_ASSERT_TRUE(batteryCapacityTargetFulfilled(&systemConfig, &systemData, &ts));

  systemData.ts = 1762598185 + 26 * 3600 + 10 * 60; // 10 min
  systemData.cons_W = systemConfig.loadPower_W + 261;
  systemData.switchEnabled = true;
  updateConsumption(&systemConfig, &systemData);
  TEST_ASSERT_TRUE(batteryCapacityTargetFulfilled(&systemConfig, &systemData, &ts));
}

void test_batteryCapacityTargetFulfilledSwitchHysteresisAvoidFlicker()
{
  systemConfig.loadPower_W = 3249;
  systemData.cap_bat_Wh = 3100;

  systemData.ts = 1762598185 + 26 * 3600 - 44; // reach min capacity + hysteresis
  systemData.cons_W = 342;
  systemData.switchEnabled = false;
  updateConsumption(&systemConfig, &systemData);
  TEST_ASSERT_TRUE(batteryCapacityTargetFulfilled(&systemConfig, &systemData, &ts));

  systemData.ts = 1762598185 + 26 * 3600 - 44 + 10; // 10s later
  systemData.cons_W = systemConfig.loadPower_W + 352;
  systemData.switchEnabled = true;
  updateConsumption(&systemConfig, &systemData);
  TEST_ASSERT_TRUE(batteryCapacityTargetFulfilled(&systemConfig, &systemData, &ts));
}

void test_updateSwitch()
{
  uint32_t ts = 0;
  SystemData systemData;
  SystemConfig systemConfig;

  updateSwitch(&systemConfig, &systemData, true);
  TEST_ASSERT_TRUE(systemData.switchEnabled);

  updateSwitch(&systemConfig, &systemData, false);
  TEST_ASSERT_FALSE(systemData.switchEnabled);
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();

  RUN_TEST(test_updateSwitch);
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
  systemData.cap_bat_max_Wh = 9900;
  systemData.inv_max_w = 4500;
  systemData.utc_offset = 3600;

  int r = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762556400;
  systemData.pv_forecast_wh_h[r++][1] = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762560000;
  systemData.pv_forecast_wh_h[r++][1] = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762563600;
  systemData.pv_forecast_wh_h[r++][1] = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762567200;
  systemData.pv_forecast_wh_h[r++][1] = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762570800;
  systemData.pv_forecast_wh_h[r++][1] = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762574400;
  systemData.pv_forecast_wh_h[r++][1] = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762578000;
  systemData.pv_forecast_wh_h[r++][1] = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762581600;
  systemData.pv_forecast_wh_h[r++][1] = 15;
  systemData.pv_forecast_wh_h[r][0] = 1762585200;
  systemData.pv_forecast_wh_h[r++][1] = 143;
  systemData.pv_forecast_wh_h[r][0] = 1762588800;
  systemData.pv_forecast_wh_h[r++][1] = 272;
  systemData.pv_forecast_wh_h[r][0] = 1762592400;
  systemData.pv_forecast_wh_h[r++][1] = 860;
  systemData.pv_forecast_wh_h[r][0] = 1762596000;
  systemData.pv_forecast_wh_h[r++][1] = 1129;
  systemData.pv_forecast_wh_h[r][0] = 1762599600;
  systemData.pv_forecast_wh_h[r++][1] = 1464;
  systemData.pv_forecast_wh_h[r][0] = 1762603200;
  systemData.pv_forecast_wh_h[r++][1] = 1950;
  systemData.pv_forecast_wh_h[r][0] = 1762606800;
  systemData.pv_forecast_wh_h[r++][1] = 1909;
  systemData.pv_forecast_wh_h[r][0] = 1762610400;
  systemData.pv_forecast_wh_h[r++][1] = 1349;
  systemData.pv_forecast_wh_h[r][0] = 1762614000;
  systemData.pv_forecast_wh_h[r++][1] = 979;
  systemData.pv_forecast_wh_h[r][0] = 1762617600;
  systemData.pv_forecast_wh_h[r++][1] = 426;
  systemData.pv_forecast_wh_h[r][0] = 1762621200;
  systemData.pv_forecast_wh_h[r++][1] = 129;
  systemData.pv_forecast_wh_h[r][0] = 1762624800;
  systemData.pv_forecast_wh_h[r++][1] = 20;
  systemData.pv_forecast_wh_h[r][0] = 1762628400;
  systemData.pv_forecast_wh_h[r++][1] = 3;
  systemData.pv_forecast_wh_h[r][0] = 1762632000;
  systemData.pv_forecast_wh_h[r++][1] = 1;
  systemData.pv_forecast_wh_h[r][0] = 1762635600;
  systemData.pv_forecast_wh_h[r++][1] = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762639200;
  systemData.pv_forecast_wh_h[r++][1] = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762642800;
  systemData.pv_forecast_wh_h[r++][1] = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762646400;
  systemData.pv_forecast_wh_h[r++][1] = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762650000;
  systemData.pv_forecast_wh_h[r++][1] = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762653600;
  systemData.pv_forecast_wh_h[r++][1] = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762657200;
  systemData.pv_forecast_wh_h[r++][1] = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762660800;
  systemData.pv_forecast_wh_h[r++][1] = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762664400;
  systemData.pv_forecast_wh_h[r++][1] = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762668000;
  systemData.pv_forecast_wh_h[r++][1] = 8;
  systemData.pv_forecast_wh_h[r][0] = 1762671600;
  systemData.pv_forecast_wh_h[r++][1] = 75;
  systemData.pv_forecast_wh_h[r][0] = 1762675200;
  systemData.pv_forecast_wh_h[r++][1] = 271;
  systemData.pv_forecast_wh_h[r][0] = 1762678800;
  systemData.pv_forecast_wh_h[r++][1] = 548;
  systemData.pv_forecast_wh_h[r][0] = 1762682400;
  systemData.pv_forecast_wh_h[r++][1] = 802;
  systemData.pv_forecast_wh_h[r][0] = 1762686000;
  systemData.pv_forecast_wh_h[r++][1] = 1011;
  systemData.pv_forecast_wh_h[r][0] = 1762689600;
  systemData.pv_forecast_wh_h[r++][1] = 1145;
  systemData.pv_forecast_wh_h[r][0] = 1762693200;
  systemData.pv_forecast_wh_h[r++][1] = 1123;
  systemData.pv_forecast_wh_h[r][0] = 1762696800;
  systemData.pv_forecast_wh_h[r++][1] = 1008;
  systemData.pv_forecast_wh_h[r][0] = 1762700400;
  systemData.pv_forecast_wh_h[r++][1] = 791;
  systemData.pv_forecast_wh_h[r][0] = 1762704000;
  systemData.pv_forecast_wh_h[r++][1] = 363;
  systemData.pv_forecast_wh_h[r][0] = 1762707600;
  systemData.pv_forecast_wh_h[r++][1] = 57;
  systemData.pv_forecast_wh_h[r][0] = 1762711200;
  systemData.pv_forecast_wh_h[r++][1] = 9;
  systemData.pv_forecast_wh_h[r][0] = 1762714800;
  systemData.pv_forecast_wh_h[r++][1] = 1;
  systemData.pv_forecast_wh_h[r][0] = 1762718400;
  systemData.pv_forecast_wh_h[r++][1] = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762722000;
  systemData.pv_forecast_wh_h[r++][1] = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762725600;
  systemData.pv_forecast_wh_h[r++][1] = 0;
  systemData.pv_forecast_wh_h[r][0] = 1762729200;
  systemData.pv_forecast_wh_h[r++][1] = 0;
}
