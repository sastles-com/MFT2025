#include <unity.h>

#include <string>
#include <vector>

#include "boot/BootOrchestrator.h"

namespace {
const char *kSampleConfigJson = R"JSON({
  "system": {"name": "sphere-boot", "PSRAM": true, "debug": false},
  "display": {"width": 128, "height": 64, "rotation": 0, "offset": [0, 0], "switch": true, "color_depth": 16},
  "buzzer": {"enabled": true, "volume": 40},
  "wifi": {"ssid": "isol", "password": "pw", "max_retries": 1},
  "mqtt": {"enabled": true, "broker": "127.0.0.1", "port": 1883,
             "topic": {"ui": "sphere/ui", "status": "sphere/status", "image": "sphere/image"}}
})JSON";

ConfigManager makeConfigManager(const char *json, bool succeed) {
  ConfigManager::FsProvider provider;
  provider.readFile = [json, succeed](const char *, std::string &out) {
    if (!succeed) {
      return false;
    }
    out = json ? json : "";
    return true;
  };
  return ConfigManager(provider);
}

struct StorageTracker {
  std::uint8_t littleAttempts = 0;
  std::uint8_t littleFormats = 0;
  std::uint8_t psAttempts = 0;
};

StorageManager makeStorageManagerForSuccess(StorageTracker &tracker) {
  StorageManager::Hooks hooks;
  hooks.littlefsBegin = [&tracker](bool format) {
    ++tracker.littleAttempts;
    if (format) {
      ++tracker.littleFormats;
    }
    return !format;
  };
  hooks.psramfsBegin = [&tracker](bool format) {
    ++tracker.psAttempts;
    (void)format;
    return true;
  };
  return StorageManager(hooks);
}

StorageManager makeStorageManagerAlwaysFail(StorageTracker &tracker) {
  StorageManager::Hooks hooks;
  hooks.littlefsBegin = [&tracker](bool format) {
    ++tracker.littleAttempts;
    if (format) {
      ++tracker.littleFormats;
    }
    return false;
  };
  hooks.psramfsBegin = [&tracker](bool format) {
    ++tracker.psAttempts;
    (void)format;
    return false;
  };
  return StorageManager(hooks);
}

}  // namespace

void test_boot_orchestrator_success_updates_config_and_stages_assets() {
  StorageTracker tracker;
  auto storage = makeStorageManagerForSuccess(tracker);
  auto config = makeConfigManager(kSampleConfigJson, true);
  SharedState shared;

  std::vector<std::string> callOrder;

  bool displayInitialized = false;
  bool buzzerPlayed = false;

  BootOrchestrator::Services services;
  services.displayInitialize = [&](const ConfigManager::DisplayConfig &displayCfg) {
    displayInitialized = displayCfg.displaySwitch;
    return true;
  };
  services.playStartupTone = [&](const ConfigManager::Config &) { buzzerPlayed = true; };

  BootOrchestrator::Callbacks callbacks;
  callbacks.onStorageReady = [&]() { callOrder.push_back("storage"); };
  callbacks.stageAssets = [&]() {
    callOrder.push_back("stage");
    return true;
  };

  BootOrchestrator orchestrator(storage, config, shared, callbacks, services);
  TEST_ASSERT_TRUE(orchestrator.run());
  TEST_ASSERT_TRUE(orchestrator.hasLoadedConfig());

  ConfigManager::Config cfg;
  TEST_ASSERT_TRUE(shared.getConfigCopy(cfg));
  TEST_ASSERT_EQUAL_STRING("sphere-boot", cfg.system.name.c_str());

  TEST_ASSERT_EQUAL_UINT8(1, tracker.littleAttempts);
  TEST_ASSERT_EQUAL_UINT8(0, tracker.littleFormats);
  TEST_ASSERT_EQUAL_UINT8(1, tracker.psAttempts);

  TEST_ASSERT_EQUAL_INT(2, callOrder.size());
  TEST_ASSERT_EQUAL_STRING("storage", callOrder[0].c_str());
  TEST_ASSERT_EQUAL_STRING("stage", callOrder[1].c_str());
  TEST_ASSERT_TRUE(displayInitialized);
  TEST_ASSERT_TRUE(buzzerPlayed);
}

void test_boot_orchestrator_fails_when_stage_callback_returns_false() {
  StorageTracker tracker;
  auto storage = makeStorageManagerForSuccess(tracker);
  auto config = makeConfigManager(kSampleConfigJson, true);
  SharedState shared;

  BootOrchestrator::Services services;
  services.displayInitialize = [](const ConfigManager::DisplayConfig &) { return true; };

  BootOrchestrator::Callbacks callbacks;
  callbacks.stageAssets = []() { return false; };

  BootOrchestrator orchestrator(storage, config, shared, callbacks, services);
  TEST_ASSERT_FALSE(orchestrator.run());
  TEST_ASSERT_FALSE(orchestrator.hasLoadedConfig());
}

void test_boot_orchestrator_fails_when_display_initialization_fails() {
  StorageTracker tracker;
  auto storage = makeStorageManagerForSuccess(tracker);
  auto config = makeConfigManager(kSampleConfigJson, true);
  SharedState shared;

  bool stageCalled = false;
  bool buzzerPlayed = false;

  BootOrchestrator::Callbacks callbacks;
  callbacks.stageAssets = [&]() {
    stageCalled = true;
    return true;
  };

  BootOrchestrator::Services services;
  services.displayInitialize = [](const ConfigManager::DisplayConfig &) { return false; };
  services.playStartupTone = [&](const ConfigManager::Config &) { buzzerPlayed = true; };

  BootOrchestrator orchestrator(storage, config, shared, callbacks, services);
  TEST_ASSERT_FALSE(orchestrator.run());
  TEST_ASSERT_FALSE(orchestrator.hasLoadedConfig());

  ConfigManager::Config cfg;
  TEST_ASSERT_FALSE(shared.getConfigCopy(cfg));
  TEST_ASSERT_TRUE(stageCalled);
  TEST_ASSERT_TRUE(buzzerPlayed);
}

void test_boot_orchestrator_aborts_when_storage_begin_fails() {
  StorageTracker tracker;
  auto storage = makeStorageManagerAlwaysFail(tracker);
  auto config = makeConfigManager(kSampleConfigJson, true);
  SharedState shared;

  bool stageCalled = false;
  bool displayCalled = false;
  BootOrchestrator::Callbacks callbacks;
  callbacks.stageAssets = [&]() {
    stageCalled = true;
    return true;
  };

  BootOrchestrator::Services services;
  services.displayInitialize = [&](const ConfigManager::DisplayConfig &) {
    displayCalled = true;
    return true;
  };

  BootOrchestrator orchestrator(storage, config, shared, callbacks, services);
  TEST_ASSERT_FALSE(orchestrator.run());
  TEST_ASSERT_FALSE(stageCalled);
  TEST_ASSERT_FALSE(orchestrator.hasLoadedConfig());
  TEST_ASSERT_EQUAL_UINT8(2, tracker.littleAttempts);
  TEST_ASSERT_EQUAL_UINT8(1, tracker.littleFormats);
  TEST_ASSERT_EQUAL_UINT8(0, tracker.psAttempts);
  TEST_ASSERT_FALSE(displayCalled);
}

void test_boot_orchestrator_handles_config_load_failure() {
  StorageTracker tracker;
  auto storage = makeStorageManagerForSuccess(tracker);
  auto config = makeConfigManager(nullptr, false);
  SharedState shared;

  bool stageCalled = false;
  bool displayCalled = false;
  bool buzzerPlayed = false;
  BootOrchestrator::Callbacks callbacks;
  callbacks.stageAssets = [&]() {
    stageCalled = true;
    return true;
  };

  BootOrchestrator::Services services;
  services.displayInitialize = [&](const ConfigManager::DisplayConfig &) {
    displayCalled = true;
    return true;
  };
  services.playStartupTone = [&](const ConfigManager::Config &) { buzzerPlayed = true; };

  BootOrchestrator orchestrator(storage, config, shared, callbacks, services);
  TEST_ASSERT_TRUE(orchestrator.run());
  TEST_ASSERT_FALSE(orchestrator.hasLoadedConfig());

  ConfigManager::Config cfg;
  TEST_ASSERT_FALSE(shared.getConfigCopy(cfg));
  TEST_ASSERT_TRUE(stageCalled);
  TEST_ASSERT_FALSE(displayCalled);
  TEST_ASSERT_FALSE(buzzerPlayed);
}

void setUp() {}
void tearDown() {}

int runUnityTests() {
  UNITY_BEGIN();
  RUN_TEST(test_boot_orchestrator_success_updates_config_and_stages_assets);
  RUN_TEST(test_boot_orchestrator_fails_when_stage_callback_returns_false);
  RUN_TEST(test_boot_orchestrator_fails_when_display_initialization_fails);
  RUN_TEST(test_boot_orchestrator_aborts_when_storage_begin_fails);
  RUN_TEST(test_boot_orchestrator_handles_config_load_failure);
  return UNITY_END();
}

#include <Arduino.h>

void setup() {
  delay(200);
  runUnityTests();
}

void loop() {}
