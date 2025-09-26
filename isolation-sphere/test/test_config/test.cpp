#include <unity.h>

#include <string>

#include "config/ConfigManager.h"

namespace {
const char *kSampleJson = R"json({
  "system": {
    "name": "sphere-001",
    "PSRAM": true,
    "debug": false
  },
  "display": {
    "width": 128,
    "height": 64,
    "rotation": 1,
    "offset": [2, 4],
    "switch": true,
    "color_depth": 16
  },
  "wifi": {
    "ssid": "IsolationSphere",
    "password": "password123",
    "max_retries": 3
  },
  "mqtt": {
    "enabled": true,
    "broker": "192.168.10.5",
    "port": 1883,
    "topic": {
      "ui": "sphere/ui",
      "status": "sphere/status",
      "image": "sphere/image"
    }
  },
  "imu": {
    "enabled": true,
    "gesture_ui_mode": true,
    "gesture_debug_log": false,
    "gesture_threshold_mps2": 12.5,
    "gesture_window_ms": 300,
    "update_interval_ms": 40,
    "ui_shake_trigger_count": 4,
    "ui_shake_window_ms": 1200
  },
  "ui": {
    "gesture_enabled": true,
    "dim_on_entry": false,
    "overlay_mode": "black"
  }
})json";

struct TestFsProvider {
  std::string lastPath;
  std::string content;
  bool shouldSucceed = true;

  ConfigManager::FsProvider make() {
    ConfigManager::FsProvider provider;
    provider.readFile = [this](const char *path, std::string &out) {
      lastPath = path ? path : "";
      if (!shouldSucceed) {
        return false;
      }
      out = content;
      return true;
    };
    return provider;
  }
};
}

void test_config_loader_parses_expected_fields() {
  TestFsProvider fs;
  fs.content = kSampleJson;
  ConfigManager manager(fs.make());

  TEST_ASSERT_TRUE(manager.load("/littlefs/config.json"));
  TEST_ASSERT_TRUE(manager.isLoaded());
  TEST_ASSERT_EQUAL_STRING("/littlefs/config.json", fs.lastPath.c_str());

  const auto &cfg = manager.config();
  TEST_ASSERT_EQUAL_STRING("sphere-001", cfg.system.name.c_str());
  TEST_ASSERT_TRUE(cfg.system.psramEnabled);
  TEST_ASSERT_FALSE(cfg.system.debug);

  TEST_ASSERT_EQUAL_UINT16(128, cfg.display.width);
  TEST_ASSERT_EQUAL_UINT16(64, cfg.display.height);
  TEST_ASSERT_EQUAL_INT8(1, cfg.display.rotation);
  TEST_ASSERT_TRUE(cfg.display.displaySwitch);
  TEST_ASSERT_EQUAL_UINT8(16, cfg.display.colorDepth);
  TEST_ASSERT_EQUAL_INT16(2, cfg.display.offsetX);
  TEST_ASSERT_EQUAL_INT16(4, cfg.display.offsetY);

  TEST_ASSERT_EQUAL_STRING("IsolationSphere", cfg.wifi.ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("password123", cfg.wifi.password.c_str());
  TEST_ASSERT_EQUAL_UINT8(3, cfg.wifi.maxRetries);

  TEST_ASSERT_TRUE(cfg.mqtt.enabled);
  TEST_ASSERT_EQUAL_STRING("192.168.10.5", cfg.mqtt.broker.c_str());
  TEST_ASSERT_EQUAL_UINT16(1883, cfg.mqtt.port);
  TEST_ASSERT_EQUAL_STRING("sphere/ui", cfg.mqtt.topicUi.c_str());
  TEST_ASSERT_EQUAL_STRING("sphere/status", cfg.mqtt.topicStatus.c_str());
  TEST_ASSERT_EQUAL_STRING("sphere/image", cfg.mqtt.topicImage.c_str());

  TEST_ASSERT_TRUE(cfg.imu.enabled);
  TEST_ASSERT_TRUE(cfg.imu.gestureUiMode);
  TEST_ASSERT_EQUAL_FLOAT(12.5f, cfg.imu.gestureThresholdMps2);
  TEST_ASSERT_EQUAL_UINT32(300, cfg.imu.gestureWindowMs);
  TEST_ASSERT_EQUAL_UINT32(40, cfg.imu.updateIntervalMs);
  TEST_ASSERT_EQUAL_UINT8(4, cfg.imu.uiShakeTriggerCount);
  TEST_ASSERT_EQUAL_UINT32(1200, cfg.imu.uiShakeWindowMs);

  TEST_ASSERT_TRUE(cfg.ui.gestureEnabled);
  TEST_ASSERT_FALSE(cfg.ui.dimOnEntry);
  TEST_ASSERT_EQUAL(static_cast<int>(ConfigManager::UiConfig::OverlayMode::kBlackout),
                    static_cast<int>(cfg.ui.overlayMode));
}

void test_config_loader_returns_false_when_file_missing() {
  TestFsProvider fs;
  fs.shouldSucceed = false;
  ConfigManager manager(fs.make());

  TEST_ASSERT_FALSE(manager.load("/littlefs/config.json"));
  TEST_ASSERT_FALSE(manager.isLoaded());
}

void test_config_loader_returns_false_on_invalid_json() {
  TestFsProvider fs;
  fs.content = "{ invalid json";
  ConfigManager manager(fs.make());

  TEST_ASSERT_FALSE(manager.load("/littlefs/config.json"));
  TEST_ASSERT_FALSE(manager.isLoaded());
}

void setUp() {}

void tearDown() {}

int runUnityTests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_config_loader_parses_expected_fields);
  RUN_TEST(test_config_loader_returns_false_when_file_missing);
  RUN_TEST(test_config_loader_returns_false_on_invalid_json);
  return UNITY_END();
}

#include <Arduino.h>

void setup() {
  delay(200);
  runUnityTests();
}

void loop() {}
