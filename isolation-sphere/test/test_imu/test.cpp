#include <unity.h>

#include "core/SharedState.h"
#include "imu/ImuService.h"

void test_imu_service_calls_begin_hook() {
  bool beginCalled = false;
  ImuService::Hooks hooks;
  hooks.begin = [&]() {
    beginCalled = true;
    return true;
  };
  ImuService service(hooks);

  TEST_ASSERT_TRUE(service.begin());
  TEST_ASSERT_TRUE(beginCalled);
  TEST_ASSERT_TRUE(service.isInitialized());
}

void test_imu_service_read_requires_initialization() {
  ImuService service;

  ImuService::Reading reading{};
  TEST_ASSERT_FALSE(service.read(reading));
}

void test_imu_service_reads_quaternion() {
  ImuService::Reading expected;
  expected.qw = 1.0f;
  expected.qx = 0.0f;
  expected.qy = 0.5f;
  expected.qz = 0.5f;
  expected.ax = 0.1f;
  expected.ay = 0.2f;
  expected.az = 9.8f;
  expected.accelMagnitudeMps2 = 9.802f;
  expected.timestampMs = 123;

  ImuService::Hooks hooks;
  hooks.begin = []() { return true; };
  hooks.read = [&](ImuService::Reading &out) {
    out = expected;
    return true;
  };

  ImuService service(hooks);
  TEST_ASSERT_TRUE(service.begin());

  ImuService::Reading actual{};
  TEST_ASSERT_TRUE(service.read(actual));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected.qw, actual.qw);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected.qx, actual.qx);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected.qy, actual.qy);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected.qz, actual.qz);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, expected.accelMagnitudeMps2, actual.accelMagnitudeMps2);
  TEST_ASSERT_EQUAL_UINT32(expected.timestampMs, actual.timestampMs);
}

void test_shared_state_stores_imu_reading() {
  SharedState state;
  ImuService::Reading reading;
  reading.qw = 0.7f;
  reading.qx = 0.1f;
  reading.qy = 0.2f;
  reading.qz = 0.3f;
  reading.ax = 0.4f;
  reading.ay = 0.5f;
  reading.az = 9.0f;
  reading.accelMagnitudeMps2 = 9.03f;
  reading.timestampMs = 456;

  state.updateImuReading(reading);

  ImuService::Reading copy;
  TEST_ASSERT_TRUE(state.getImuReading(copy));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, reading.qw, copy.qw);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, reading.ax, copy.ax);
  TEST_ASSERT_EQUAL_UINT32(reading.timestampMs, copy.timestampMs);
}

void test_shared_state_ui_mode() {
  SharedState state;
  bool active = false;
  TEST_ASSERT_FALSE(state.getUiMode(active));
  state.setUiMode(true);
  TEST_ASSERT_TRUE(state.getUiMode(active));
  TEST_ASSERT_TRUE(active);
}

void test_shared_state_ui_command() {
  SharedState state;
  std::string command;
  TEST_ASSERT_FALSE(state.getUiCommand(command));
  state.updateUiCommand("{\"cmd\":\"test\"}");
  TEST_ASSERT_TRUE(state.getUiCommand(command));
  TEST_ASSERT_EQUAL_STRING("{\"cmd\":\"test\"}", command.c_str());
}

void setUp() {}
void tearDown() {}

int runUnityTests() {
  UNITY_BEGIN();
  RUN_TEST(test_imu_service_calls_begin_hook);
  RUN_TEST(test_imu_service_read_requires_initialization);
  RUN_TEST(test_imu_service_reads_quaternion);
  RUN_TEST(test_shared_state_stores_imu_reading);
  RUN_TEST(test_shared_state_ui_mode);
  RUN_TEST(test_shared_state_ui_command);
  return UNITY_END();
}

#include <Arduino.h>

void setup() {
  delay(200);
  runUnityTests();
}

void loop() {}
