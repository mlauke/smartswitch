#include <unity.h>
#include <string.h>

#include <JsonFields.h>
#include <SonnenApi.h>

#define ERROR_SIZE 128

static const char STATUS_RESPONSE[] = R"({
  "USOC": 99, "GridFeedIn_W": -335, "Production_W": 109, "Consumption_W": 436,
  "Pac_total_W": -3, "BatteryCharging": false, "BatteryDischarging": false,
  "Timestamp": "2026-01-12 10:00:28"
})";

static const char STATUS_RESPONSE_WITHOUT_USOC[] = R"({
  "GridFeedIn_W": -335, "Production_W": 109, "Consumption_W": 436,
  "Pac_total_W": -3, "BatteryCharging": false, "BatteryDischarging": false,
  "Timestamp": "2026-01-12 10:00:28"
})";

static const char LATEST_DATA_RESPONSE[] = R"({
  "UTC_Offset": 1,
  "ic_status": {
    "Setpoint Priority": { "Full Charge Request": true },
    "nextfullchargestarttime": 2795534,
    "secondssincefullcharge": 2830037
  }
})";

static const char LATEST_DATA_RESPONSE_WITHOUT_PRIORITY[] = R"({
  "UTC_Offset": 1,
  "ic_status": {
    "nextfullchargestarttime": 2795534,
    "secondssincefullcharge": 2830037
  }
})";

static char error[ERROR_SIZE];

void setUp(void)
{
  error[0] = '\0';
}

void tearDown(void)
{
}

static bool validate(const char *response, const JsonField *fields, uint8_t count)
{
  JsonDocument doc;
  deserializeJson(doc, response);
  return validateJsonFields(doc.as<JsonVariantConst>(), fields, count, error, sizeof(error));
}

static void test_validateCompleteStatusResponse(void)
{
  TEST_ASSERT_TRUE(validate(STATUS_RESPONSE, SONNEN_FIELDS_STATUS, JSON_FIELD_COUNT(SONNEN_FIELDS_STATUS)));
}

static void test_validateCompleteLatestDataResponse(void)
{
  TEST_ASSERT_TRUE(validate(LATEST_DATA_RESPONSE, SONNEN_FIELDS_LATEST_DATA, JSON_FIELD_COUNT(SONNEN_FIELDS_LATEST_DATA)));
}

static void test_validateWithoutExpectedFields(void)
{
  TEST_ASSERT_TRUE(validate(STATUS_RESPONSE, NULL, 0));
}

static void test_validateDetectsMissingField(void)
{
  TEST_ASSERT_FALSE(validate(STATUS_RESPONSE_WITHOUT_USOC, SONNEN_FIELDS_STATUS, JSON_FIELD_COUNT(SONNEN_FIELDS_STATUS)));
}

static void test_validateDetectsMissingNestedField(void)
{
  TEST_ASSERT_FALSE(validate(LATEST_DATA_RESPONSE_WITHOUT_PRIORITY, SONNEN_FIELDS_LATEST_DATA, JSON_FIELD_COUNT(SONNEN_FIELDS_LATEST_DATA)));
}

static void test_validateReportsMissingFieldPath(void)
{
  validate(LATEST_DATA_RESPONSE_WITHOUT_PRIORITY, SONNEN_FIELDS_LATEST_DATA, JSON_FIELD_COUNT(SONNEN_FIELDS_LATEST_DATA));
  TEST_ASSERT_EQUAL_STRING("missing field: ic_status/Setpoint Priority/Full Charge Request", error);
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();

  RUN_TEST(test_validateCompleteStatusResponse);
  RUN_TEST(test_validateCompleteLatestDataResponse);
  RUN_TEST(test_validateWithoutExpectedFields);
  RUN_TEST(test_validateDetectsMissingField);
  RUN_TEST(test_validateDetectsMissingNestedField);
  RUN_TEST(test_validateReportsMissingFieldPath);

  return UNITY_END();
}
