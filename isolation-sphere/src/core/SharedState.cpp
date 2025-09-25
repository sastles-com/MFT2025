#include "core/SharedState.h"

SharedState::SharedState() {
#ifndef UNIT_TEST
  mutex_ = xSemaphoreCreateMutex();
#endif
}

SharedState::~SharedState() {
#ifndef UNIT_TEST
  if (mutex_ != nullptr) {
    vSemaphoreDelete(mutex_);
    mutex_ = nullptr;
  }
#endif
}

void SharedState::lock() const {
#ifndef UNIT_TEST
  if (mutex_ != nullptr) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
  }
#else
  mutex_.lock();
#endif
}

void SharedState::unlock() const {
#ifndef UNIT_TEST
  if (mutex_ != nullptr) {
    xSemaphoreGive(mutex_);
  }
#else
  mutex_.unlock();
#endif
}

void SharedState::updateConfig(const ConfigManager::Config &config) {
  lock();
  config_ = config;
  hasConfig_ = true;
  unlock();
}

bool SharedState::getConfigCopy(ConfigManager::Config &out) const {
  lock();
  bool available = hasConfig_;
  if (available) {
    out = config_;
  }
  unlock();
  return available;
}

void SharedState::updateImuReading(const ImuService::Reading &reading) {
  lock();
  imuReading_ = reading;
  hasImuReading_ = true;
  unlock();
}

bool SharedState::getImuReading(ImuService::Reading &out) const {
  lock();
  bool available = hasImuReading_;
  if (available) {
    out = imuReading_;
  }
  unlock();
  return available;
}
