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

void SharedState::setUiMode(bool active) {
  lock();
  uiModeActive_ = active;
  hasUiMode_ = true;
  unlock();
}

bool SharedState::getUiMode(bool &active) const {
  lock();
  bool available = hasUiMode_;
  if (available) {
    active = uiModeActive_;
  }
  unlock();
  return available;
}

void SharedState::pushUiCommand(const std::string &command, bool external) {
  lock();
  if (external) {
    uiCommandIncoming_ = command;
    hasUiCommandIncoming_ = true;
  } else {
    uiCommandOutgoing_ = command;
    hasUiCommandOutgoing_ = true;
  }
  unlock();
}

bool SharedState::popUiCommand(std::string &command, bool external) {
  lock();
  bool available = external ? hasUiCommandIncoming_ : hasUiCommandOutgoing_;
  if (available) {
    command = external ? uiCommandIncoming_ : uiCommandOutgoing_;
    if (external) {
      hasUiCommandIncoming_ = false;
      uiCommandIncoming_.clear();
    } else {
      hasUiCommandOutgoing_ = false;
      uiCommandOutgoing_.clear();
    }
  }
  unlock();
  return available;
}

void SharedState::pushSystemCommand(const std::string &command, bool external) {
  lock();
  if (external) {
    systemCommandIncoming_ = command;
    hasSystemCommandIncoming_ = true;
  } else {
    systemCommandOutgoing_ = command;
    hasSystemCommandOutgoing_ = true;
  }
  unlock();
}

bool SharedState::popSystemCommand(std::string &command, bool external) {
  lock();
  bool available = external ? hasSystemCommandIncoming_ : hasSystemCommandOutgoing_;
  if (available) {
    command = external ? systemCommandIncoming_ : systemCommandOutgoing_;
    if (external) {
      hasSystemCommandIncoming_ = false;
      systemCommandIncoming_.clear();
    } else {
      hasSystemCommandOutgoing_ = false;
      systemCommandOutgoing_.clear();
    }
  }
  unlock();
  return available;
}
