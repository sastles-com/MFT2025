#include "boot/BootOrchestrator.h"

#include <utility>

BootOrchestrator::BootOrchestrator(StorageManager &storage,
                                   ConfigManager &config,
                                   SharedState &shared,
                                   Callbacks callbacks,
                                   Services services)
    : storage_(storage),
      config_(config),
      shared_(shared),
      callbacks_(std::move(callbacks)),
      services_(std::move(services)) {}

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

  if (hasConfig && services_.playStartupTone) {
    services_.playStartupTone(configCopy);
  }

  if (hasConfig) {
    shared_.updateConfig(configCopy);
    if (services_.onConfigReady) {
      services_.onConfigReady(configCopy);
    }
  }

  if (callbacks_.stageAssets) {
    if (!callbacks_.stageAssets()) {
      return false;
    }
  }

  if (hasConfig) {
    if (services_.displayInitialize) {
      if (!services_.displayInitialize(configCopy.display)) {
        return false;
      }
    }

    shared_.updateConfig(configCopy);
    loadedConfig_ = true;
  }

  return true;
}

bool BootOrchestrator::hasLoadedConfig() const {
  return loadedConfig_;
}
