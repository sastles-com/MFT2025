#include "boot/BootOrchestrator.h"

#include <utility>

BootOrchestrator::BootOrchestrator(StorageManager &storage,
                                   ConfigManager &config,
                                   SharedState &shared,
                                   Callbacks callbacks)
    : storage_(storage), config_(config), shared_(shared), callbacks_(std::move(callbacks)) {}

bool BootOrchestrator::run() {
  loadedConfig_ = false;

  if (!storage_.begin()) {
    return false;
  }

  if (callbacks_.onStorageReady) {
    callbacks_.onStorageReady();
  }

  bool hasConfig = false;
  ConfigManager::Config configCopy;
  if (storage_.isLittleFsMounted()) {
    if (config_.load()) {
      configCopy = config_.config();
      hasConfig = true;
    }
  }

  if (callbacks_.stageAssets) {
    if (!callbacks_.stageAssets()) {
      return false;
    }
  }

  if (hasConfig) {
    shared_.updateConfig(configCopy);
    loadedConfig_ = true;
  }

  return true;
}

bool BootOrchestrator::hasLoadedConfig() const {
  return loadedConfig_;
}
