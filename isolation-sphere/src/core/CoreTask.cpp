#include "core/CoreTask.h"

#ifndef UNIT_TEST
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

CoreTask::CoreTask(TaskConfig config) : config_(config) {
  hooks_ = makeDefaultHooks();
}

bool CoreTask::start() {
  if (started_) {
    return false;
  }
  if (!hooks_.launch) {
    return false;
  }
  if (!hooks_.launch(*this)) {
    return false;
  }
  started_ = true;
  return true;
}

bool CoreTask::isStarted() const {
  return started_;
}

void CoreTask::setHooks(Hooks hooks) {
  hooks_ = hooks;
}

CoreTask::Hooks CoreTask::hooks() const {
  return hooks_;
}

void CoreTask::runOnceForTest() {
  if (!setupDone_) {
    setup();
    setupDone_ = true;
  }
  loop();
}

const CoreTask::TaskConfig &CoreTask::config() const {
  return config_;
}

void CoreTask::sleep(std::uint32_t ms) {
  if (hooks_.delay) {
    hooks_.delay(ms);
    return;
  }
#ifndef UNIT_TEST
  vTaskDelay(pdMS_TO_TICKS(ms));
#else
  (void)ms;
#endif
}

void CoreTask::taskEntry(void *param) {
  auto *task = static_cast<CoreTask *>(param);
  task->runTaskLoop();
}

void CoreTask::runTaskLoop() {
  if (!setupDone_) {
    setup();
    setupDone_ = true;
  }
  while (true) {
    loop();
    if (config_.loopIntervalMs > 0) {
      sleep(config_.loopIntervalMs);
    }
  }
}

CoreTask::Hooks CoreTask::makeDefaultHooks() {
  Hooks hooks;
#ifndef UNIT_TEST
  hooks.launch = [](CoreTask &task) {
    TaskHandle_t handle = nullptr;
    BaseType_t result = xTaskCreatePinnedToCore(
        CoreTask::taskEntry,
        task.config_.name,
        task.config_.stackSize,
        &task,
        task.config_.priority,
        &handle,
        task.config_.coreId);
    return result == pdPASS;
  };
  hooks.delay = [](std::uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
  };
#endif
  return hooks;
}
