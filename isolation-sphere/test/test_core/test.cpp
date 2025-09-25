#include <unity.h>

#include <cstdint>

#include "core/CoreTask.h"
#include "core/CoreTasks.h"
#include "core/SharedState.h"
#include "storage/StorageManager.h"
#include "config/ConfigManager.h"

namespace {
class DummyTask : public CoreTask {
 public:
  struct Counters {
    std::uint32_t setupCalls = 0;
    std::uint32_t loopCalls = 0;
  };

  explicit DummyTask(const TaskConfig &config) : CoreTask(config) {}

  Counters counters() const { return counters_; }

 protected:
  void setup() override { counters_.setupCalls++; }
  void loop() override { counters_.loopCalls++; }

 private:
  Counters counters_;
};
}

void test_start_invokes_launch_hook_once() {
  CoreTask::TaskConfig config;
  config.name = "CoreTest";
  config.stackSize = 2048;
  config.priority = 3;
  config.coreId = 0;
  config.loopIntervalMs = 0;
  DummyTask task(config);

  int launchCount = 0;
  CoreTask::Hooks hooks;
  hooks.launch = [&](CoreTask &instance) {
    launchCount++;
    instance.runOnceForTest();
    return true;
  };
  task.setHooks(hooks);

  TEST_ASSERT_TRUE(task.start());
  TEST_ASSERT_EQUAL(1, launchCount);
  TEST_ASSERT_FALSE(task.start());  // 二重起動不可
}

void test_run_once_calls_setup_then_loop() {
  CoreTask::TaskConfig config;
  config.name = "CoreTest";
  config.stackSize = 2048;
  config.priority = 3;
  config.coreId = 1;
  config.loopIntervalMs = 5;
  DummyTask task(config);

  task.runOnceForTest();
  auto counters = task.counters();
  TEST_ASSERT_EQUAL_UINT32(1, counters.setupCalls);
  TEST_ASSERT_EQUAL_UINT32(1, counters.loopCalls);

  task.runOnceForTest();
  counters = task.counters();
  TEST_ASSERT_EQUAL_UINT32(1, counters.setupCalls);
  TEST_ASSERT_EQUAL_UINT32(2, counters.loopCalls);
}

void test_start_fails_without_launch_hook() {
  CoreTask::TaskConfig config;
  config.name = "CoreTest";
  config.stackSize = 1024;
  config.priority = 2;
  config.coreId = 0;
  config.loopIntervalMs = 1;
  DummyTask task(config);

  CoreTask::Hooks hooks;
  hooks.launch = nullptr;
  task.setHooks(hooks);

  TEST_ASSERT_FALSE(task.start());
}

ConfigManager makeConfigManagerWithJson(const char *json) {
  ConfigManager::FsProvider provider;
  provider.readFile = [json](const char *, std::string &out) {
    out = json;
    return true;
  };
  return ConfigManager(provider);
}

void test_core0_task_updates_shared_state() {
  const char *json = R"JSON({
    "system": {"name": "sphere-001", "PSRAM": true, "debug": false},
    "display": {"width": 128, "height": 64, "rotation": 0, "offset": [0, 0], "switch": true, "color_depth": 16},
    "buzzer": {"enabled": true, "volume": 32},
    "imu": {"enabled": true, "gesture_ui_mode": true, "gesture_debug_log": false, "gesture_threshold_mps2": 12.5, "gesture_window_ms": 300, "update_interval_ms": 40},
    "wifi": {"ssid": "Direct", "password": "pass", "max_retries": 2},
    "mqtt": {"enabled": true, "broker": "127.0.0.1", "port": 1883, "topic": {"ui": "sphere/ui", "status": "sphere/status", "image": "sphere/image"}}
  })JSON";

  auto configManager = makeConfigManagerWithJson(json);
  StorageManager::Hooks storageHooks;
  storageHooks.littlefsBegin = [](bool) { return true; };
  storageHooks.psramfsBegin = [](bool) { return true; };
  StorageManager storage(storageHooks);
  storage.begin(false, false);

  SharedState shared;
  CoreTask::TaskConfig config;
  config.name = "Core0";
  config.loopIntervalMs = 0;
  Core0Task task(config, configManager, storage, shared);

  CoreTask::Hooks hooks;
  hooks.launch = [](CoreTask &t) {
    t.runOnceForTest();
    return true;
  };
  task.setHooks(hooks);

  TEST_ASSERT_TRUE(task.start());

  ConfigManager::Config cfg;
  TEST_ASSERT_TRUE(shared.getConfigCopy(cfg));
  TEST_ASSERT_EQUAL_STRING("sphere-001", cfg.system.name.c_str());
  TEST_ASSERT_TRUE(cfg.buzzer.enabled);
  TEST_ASSERT_EQUAL_UINT8(32, cfg.buzzer.volume);
  TEST_ASSERT_TRUE(cfg.imu.enabled);
  TEST_ASSERT_TRUE(cfg.imu.gestureUiMode);
  TEST_ASSERT_EQUAL_FLOAT(12.5f, cfg.imu.gestureThresholdMps2);
  TEST_ASSERT_EQUAL_UINT32(300, cfg.imu.gestureWindowMs);
  TEST_ASSERT_EQUAL_UINT32(40, cfg.imu.updateIntervalMs);
}

void test_core1_task_reads_shared_state() {
  SharedState shared;
  ConfigManager::Config cfg;
  cfg.system.name = "sphere-002";
  shared.updateConfig(cfg);

  CoreTask::TaskConfig config;
  config.name = "Core1";
  config.loopIntervalMs = 0;
  Core1Task task(config, shared);

  CoreTask::Hooks hooks;
  hooks.launch = [](CoreTask &t) {
    t.runOnceForTest();
    return true;
  };
  task.setHooks(hooks);

  TEST_ASSERT_TRUE(task.start());
}

void test_core1_task_initializes_and_reads_imu_when_enabled() {
  SharedState shared;
  ConfigManager::Config cfg;
  cfg.system.name = "sphere-imu";
  cfg.imu.enabled = true;
  cfg.imu.updateIntervalMs = 0;
  shared.updateConfig(cfg);

  CoreTask::TaskConfig config;
  config.name = "Core1";
  config.loopIntervalMs = 0;
  Core1Task task(config, shared);

  bool beginCalled = false;
  bool readCalled = false;
  ImuService::Hooks imuHooks;
  imuHooks.begin = [&]() {
    beginCalled = true;
    return true;
  };
  imuHooks.read = [&](ImuService::Reading &reading) {
    readCalled = true;
    reading.qw = 1.0f;
    reading.timestampMs = 42;
    return true;
  };
#ifdef UNIT_TEST
  task.setImuHooksForTest(imuHooks);
#endif

  CoreTask::Hooks hooks;
  hooks.launch = [](CoreTask &t) {
    t.runOnceForTest();
    return true;
  };
  task.setHooks(hooks);

  TEST_ASSERT_TRUE(task.start());
  TEST_ASSERT_TRUE(beginCalled);
  TEST_ASSERT_TRUE(readCalled);

  ImuService::Reading stored;
  TEST_ASSERT_TRUE(shared.getImuReading(stored));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, stored.qw);
  TEST_ASSERT_EQUAL_UINT32(42, stored.timestampMs);
}

void setUp() {}
void tearDown() {}

int runUnityTests() {
  UNITY_BEGIN();
  RUN_TEST(test_start_invokes_launch_hook_once);
  RUN_TEST(test_run_once_calls_setup_then_loop);
  RUN_TEST(test_start_fails_without_launch_hook);
  RUN_TEST(test_core0_task_updates_shared_state);
  RUN_TEST(test_core1_task_reads_shared_state);
  RUN_TEST(test_core1_task_initializes_and_reads_imu_when_enabled);
  return UNITY_END();
}

#include <Arduino.h>

void setup() {
  delay(200);
  runUnityTests();
}

void loop() {}
