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

void SharedState::setJoystickInput(const JoystickInput &input) {
  lock();
  joystickInput_ = input;
  hasJoystickInput_ = true;
  unlock();
}

bool SharedState::getJoystickInput(JoystickInput &out) const {
  lock();
  const bool has = hasJoystickInput_;
  if (has) {
    out = joystickInput_;
  }
  unlock();
  return has;
}

void SharedState::setCommunicationStatus(const CommunicationStatus &status) {
  lock();
  commStatus_ = status;
  hasCommStatus_ = true;
  unlock();
}

bool SharedState::getCommunicationStatus(CommunicationStatus &out) const {
  lock();
  const bool has = hasCommStatus_;
  if (has) {
    out = commStatus_;
  }
  unlock();
  return has;
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
