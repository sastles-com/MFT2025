#include <unity.h>

#include <string>

#include "config/ConfigManager.h"
#include "../../src/config/ConfigManager.cpp"

namespace {

ConfigManager makeManagerWithJson(const char *json) {
  ConfigManager::FsProvider provider;
  provider.readFile = [json](const char *, std::string &out) {
    out = json;
    return true;
  };
  return ConfigManager(provider);
}

}  // namespace

void test_full_config_matches_expectations() {
  const char *json = R"JSON({
    "system": {
      "name": "MFT2025-01",
      "PSRAM": true,
      "debug": true
    },
    "wifi": {
      "enabled": true,
      "mode": "ap",
      "visible": true,
      "max_retries": 0,
      "ap": {
        "ssid": "isolation-joystick",
        "password": "",
        "local_ip": "192.168.100.1",
        "gateway": "192.168.100.1",
        "subnet": "255.255.255.0",
        "channel": 6,
        "hidden": false,
        "max_connections": 8
      }
    },
    "mqtt": {
      "enabled": true,
      "broker": "192.168.100.1",
      "port": 1883,
      "keep_alive": 60,
      "topic": {
        "status": "sphere/001/status",
        "ui": "sphere/ui",
        "ui_individual": "sphere/001/ui",
        "ui_all": "sphere/all/ui",
        "image": "sphere/image",
        "image_individual": "sphere/001/image",
        "image_all": "sphere/all/image",
        "command": "sphere/command",
        "command_individual": "sphere/001/command",
        "command_all": "sphere/all/command",
        "input": "sphere/001/input",
        "sync": "system/all/sync",
        "emergency": "system/all/emergency"
      }
    },
    "sphere": {
      "instances": [
        {
          "id": "sphere001",
          "mac": "E4:B3:23:F6:93:8C",
          "static_ip": "192.168.100.100",
          "mqtt_prefix": "sphere/001/",
          "friendly_name": "Main Sphere",
          "notes": "Primary sphere device",
          "features": {
            "led": true,
            "imu": true,
            "ui": true
          }
        }
      ],
      "led": {
        "enabled": true,
        "brightness": 128,
        "strip_gpios": [5, 6, 7, 8],
        "num_strips": 4,
        "leds_per_strip": [180, 220, 180, 220],
        "total_leds": 800
      }
    }
  })JSON";

  auto manager = makeManagerWithJson(json);
  TEST_ASSERT_TRUE(manager.load("/config.json"));
  const auto &cfg = manager.config();

  TEST_ASSERT_EQUAL_STRING("MFT2025-01", cfg.system.name.c_str());
  TEST_ASSERT_TRUE(cfg.system.psramEnabled);
  TEST_ASSERT_TRUE(cfg.system.debug);

  TEST_ASSERT_TRUE(cfg.wifi.enabled);
  TEST_ASSERT_EQUAL_STRING("ap", cfg.wifi.mode.c_str());
  TEST_ASSERT_EQUAL_STRING("isolation-joystick", cfg.wifi.ap.ssid.c_str());
  TEST_ASSERT_EQUAL_UINT8(6, cfg.wifi.ap.channel);

  TEST_ASSERT_TRUE(cfg.mqtt.enabled);
  TEST_ASSERT_EQUAL_STRING("sphere/001/status", cfg.mqtt.topicStatus.c_str());
  TEST_ASSERT_EQUAL_STRING("sphere/001/ui", cfg.mqtt.topicUiIndividual.c_str());
  TEST_ASSERT_EQUAL_STRING("sphere/all/command", cfg.mqtt.topicCommandAll.c_str());

  TEST_ASSERT_EQUAL_UINT32(1u, cfg.sphere.instances.size());
  TEST_ASSERT_EQUAL_STRING("sphere001", cfg.sphere.instances[0].id.c_str());
  TEST_ASSERT_TRUE(cfg.sphere.instances[0].features.led);

  TEST_ASSERT_EQUAL_UINT8(4, cfg.led.numStrips);
  TEST_ASSERT_EQUAL_UINT16(180, cfg.led.ledsPerStrip[0]);
  TEST_ASSERT_EQUAL_UINT16(220, cfg.led.ledsPerStrip[1]);
  TEST_ASSERT_EQUAL_UINT8(5, cfg.led.stripGpios[0]);
  TEST_ASSERT_EQUAL_UINT8(6, cfg.led.stripGpios[1]);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_full_config_matches_expectations);
  return UNITY_END();
}
