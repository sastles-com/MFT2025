#include "core/CoreTasks.h"

#include <Arduino.h>

Core0Task::Core0Task(const TaskConfig &config,
                     ConfigManager &configManager,
                     StorageManager &storageManager,
                     SharedState &sharedState)
    : CoreTask(config),
      configManager_(configManager),
      storageManager_(storageManager),
      sharedState_(sharedState) {}

void Core0Task::setup() {
  Serial.println("[Core0] Task setup complete");
}

void Core0Task::loop() {
  if (!configLoaded_ && storageManager_.isLittleFsMounted()) {
    if (configManager_.load()) {
      sharedState_.updateConfig(configManager_.config());
      configLoaded_ = true;
      Serial.println("[Core0] Config loaded and shared");
    }
  }
  sleep(config().loopIntervalMs);
}

Core1Task::Core1Task(const TaskConfig &config, SharedState &sharedState)
    : CoreTask(config), sharedState_(sharedState) {}

void Core1Task::setup() {
  Serial.println("[Core1] Task setup complete");
}

void Core1Task::loop() {
  ConfigManager::Config cfg;
  if (sharedState_.getConfigCopy(cfg) && !displayedConfig_) {
    Serial.printf("[Core1] Config name=%s\n", cfg.system.name.c_str());
    displayedConfig_ = true;
  }
  sleep(config().loopIntervalMs);
}
