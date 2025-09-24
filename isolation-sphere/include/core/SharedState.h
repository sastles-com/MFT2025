#pragma once

#ifdef UNIT_TEST
#include <mutex>
#else
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

#include "config/ConfigManager.h"

class SharedState {
 public:
  SharedState();
  ~SharedState();

  void updateConfig(const ConfigManager::Config &config);
  bool getConfigCopy(ConfigManager::Config &out) const;

 private:
  void lock() const;
  void unlock() const;

#ifndef UNIT_TEST
  mutable SemaphoreHandle_t mutex_ = nullptr;
#else
  mutable std::mutex mutex_;
#endif
  ConfigManager::Config config_{};
  bool hasConfig_ = false;
};
