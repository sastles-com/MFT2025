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

  BootOrchestrator(StorageManager &storage,
                   ConfigManager &config,
                   SharedState &shared,
                   Callbacks callbacks = {});

  bool run();

  bool hasLoadedConfig() const;

 private:
  StorageManager &storage_;
  ConfigManager &config_;
  SharedState &shared_;
  Callbacks callbacks_;
  bool loadedConfig_ = false;
};
