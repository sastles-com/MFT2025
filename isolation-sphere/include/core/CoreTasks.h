#pragma once

#include "config/ConfigManager.h"
#include "core/CoreTask.h"
#include "core/SharedState.h"
#include "storage/StorageManager.h"

class Core0Task : public CoreTask {
 public:
  Core0Task(const TaskConfig &config, ConfigManager &configManager, StorageManager &storageManager, SharedState &sharedState);

 protected:
  void setup() override;
  void loop() override;

 private:
  ConfigManager &configManager_;
  StorageManager &storageManager_;
  SharedState &sharedState_;
  bool configLoaded_ = false;
};

class Core1Task : public CoreTask {
 public:
  Core1Task(const TaskConfig &config, SharedState &sharedState);

 protected:
  void setup() override;
  void loop() override;

 private:
  SharedState &sharedState_;
  bool displayedConfig_ = false;
};
