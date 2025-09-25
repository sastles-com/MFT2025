#pragma once

#include <functional>

#include "config/ConfigManager.h"
#include "core/SharedState.h"
#include "storage/StorageManager.h"

class BootOrchestrator {
 public:
  struct Callbacks {
    std::function<bool()> stageAssets;
    std::function<void()> onStorageReady;
  };

  struct Services {
    std::function<bool(const ConfigManager::DisplayConfig &)> displayInitialize;
    std::function<void(const ConfigManager::Config &)> playStartupTone;
    std::function<void(const ConfigManager::Config &)> onConfigReady;
  };

  BootOrchestrator(StorageManager &storage,
                   ConfigManager &config,
                   SharedState &shared,
                   Callbacks callbacks = {},
                   Services services = {});

  bool run();

  bool hasLoadedConfig() const;

 private:
  StorageManager &storage_;
  ConfigManager &config_;
  SharedState &shared_;
  Callbacks callbacks_;
  Services services_;
  bool loadedConfig_ = false;
};
