#pragma once

#ifdef UNIT_TEST
#include <mutex>
#else
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

#include "config/ConfigManager.h"
#include "imu/ImuService.h"

class SharedState {
 public:
  SharedState();
  ~SharedState();

  void updateConfig(const ConfigManager::Config &config);
  bool getConfigCopy(ConfigManager::Config &out) const;

  void updateImuReading(const ImuService::Reading &reading);
  bool getImuReading(ImuService::Reading &out) const;

  void setUiMode(bool active);
  bool getUiMode(bool &active) const;

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
  ImuService::Reading imuReading_{};
  bool hasImuReading_ = false;
  bool uiModeActive_ = false;
  bool hasUiMode_ = false;
};
