#include <unity.h>
#include <vector>

#include "led/LEDSphereManager.h"
#include "../../src/led/LEDSphereManager.cpp"

using LEDSphere::LEDSphereManager;

void test_initialize_led_hardware_allocates_buffer() {
  std::vector<uint16_t> lengths{3, 2};
  std::vector<uint8_t> pins{5, 6};
  LEDSphereManager manager;
  TEST_ASSERT_TRUE(manager.initializeLedHardware(static_cast<uint8_t>(lengths.size()), lengths, pins));
  TEST_ASSERT_NOT_NULL(manager.frameBufferForTest());
  TEST_ASSERT_EQUAL_UINT32(5, manager.totalLedsForTest());
}

void test_set_led_updates_framebuffer() {
  std::vector<uint16_t> lengths{4};
  std::vector<uint8_t> pins{5};
  LEDSphereManager manager;
  TEST_ASSERT_TRUE(manager.initializeLedHardware(static_cast<uint8_t>(lengths.size()), lengths, pins));

  manager.clearAllLEDs();
  auto buffer = manager.frameBufferForTest();
  TEST_ASSERT_EQUAL_UINT8(0, buffer[0].r);
  TEST_ASSERT_EQUAL_UINT8(0, buffer[0].g);
  TEST_ASSERT_EQUAL_UINT8(0, buffer[0].b);

  manager.setLED(2, CRGB(10, 20, 30));
  TEST_ASSERT_EQUAL_UINT8(10, buffer[2].r);
  TEST_ASSERT_EQUAL_UINT8(20, buffer[2].g);
  TEST_ASSERT_EQUAL_UINT8(30, buffer[2].b);
}

void test_show_sets_flag_under_unit_test() {
  std::vector<uint16_t> lengths{2};
  std::vector<uint8_t> pins{5};
  LEDSphereManager manager;
  TEST_ASSERT_TRUE(manager.initializeLedHardware(static_cast<uint8_t>(lengths.size()), lengths, pins));
  manager.resetShowFlagForTest();
  TEST_ASSERT_FALSE(manager.wasShowCalledForTest());
  manager.show();
  TEST_ASSERT_TRUE(manager.wasShowCalledForTest());
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_initialize_led_hardware_allocates_buffer);
  RUN_TEST(test_set_led_updates_framebuffer);
  RUN_TEST(test_show_sets_flag_under_unit_test);
  return UNITY_END();
}
