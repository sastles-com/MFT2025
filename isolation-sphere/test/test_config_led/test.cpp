#include <unity.h>

#include <string>

#include "config/ConfigManager.h"
#include "../../src/config/ConfigManager.cpp"

namespace {

ConfigManager makeConfigManagerWithJson(const char *json) {
  ConfigManager::FsProvider provider;
  provider.readFile = [json](const char *, std::string &out) {
    out = json;
    return true;
  };
  return ConfigManager(provider);
}

}  // namespace

void test_led_config_parses_arrays() {
  const char *json = R"JSON({
    "leds": {
      "leds_per_strip": [180, 220, 200],
      "strip_gpios": [5, 6, 7]
    }
  })JSON";

  auto configManager = makeConfigManagerWithJson(json);
  TEST_ASSERT_TRUE(configManager.load("/config.json"));

  const auto &cfg = configManager.config();
  TEST_ASSERT_EQUAL_UINT8(3, cfg.led.numStrips);
  TEST_ASSERT_EQUAL_UINT16(180, cfg.led.ledsPerStrip[0]);
  TEST_ASSERT_EQUAL_UINT16(220, cfg.led.ledsPerStrip[1]);
  TEST_ASSERT_EQUAL_UINT16(200, cfg.led.ledsPerStrip[2]);
  TEST_ASSERT_EQUAL_UINT8(5, cfg.led.stripGpios[0]);
  TEST_ASSERT_EQUAL_UINT8(6, cfg.led.stripGpios[1]);
  TEST_ASSERT_EQUAL_UINT8(7, cfg.led.stripGpios[2]);
}

void test_led_config_defaults_when_missing() {
  const char *json = R"JSON({})JSON";

  auto configManager = makeConfigManagerWithJson(json);
  TEST_ASSERT_TRUE(configManager.load("/config.json"));

  const auto &cfg = configManager.config();
  TEST_ASSERT_EQUAL_UINT8(4, cfg.led.numStrips);
  TEST_ASSERT_EQUAL_UINT16(200, cfg.led.ledsPerStrip[0]);
  TEST_ASSERT_EQUAL_UINT8(5, cfg.led.stripGpios[0]);
}

#ifdef ARDUINO
#include <Arduino.h>

void setup() {
  delay(200);
  UNITY_BEGIN();
  RUN_TEST(test_led_config_parses_arrays);
  RUN_TEST(test_led_config_defaults_when_missing);
  UNITY_END();
}

void loop() {}
#else
int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_led_config_parses_arrays);
  RUN_TEST(test_led_config_defaults_when_missing);
  return UNITY_END();
}
#endif
