#pragma once

#include <cstdint>

#ifdef UNIT_TEST
#include <mutex>
#else
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

/**
 * @brief コア間で共有するジョイスティック状態を保持する。
 */
class SharedState {
 public:
  struct JoystickInput {
    std::int16_t leftX = 0;
    std::int16_t leftY = 0;
    std::int16_t rightX = 0;
    std::int16_t rightY = 0;
    bool leftButton = false;
    bool rightButton = false;
    std::uint32_t sequence = 0;
    std::uint32_t timestampMs = 0;
  };

  struct CommunicationStatus {
    bool wifiConnected = false;
    std::uint32_t udpSent = 0;
    std::uint32_t udpErrors = 0;
  };

  SharedState();
  ~SharedState();

  void setJoystickInput(const JoystickInput &input);
  bool getJoystickInput(JoystickInput &out) const;

  void setCommunicationStatus(const CommunicationStatus &status);
  bool getCommunicationStatus(CommunicationStatus &out) const;

 private:
  void lock() const;
  void unlock() const;

#ifndef UNIT_TEST
  mutable SemaphoreHandle_t mutex_ = nullptr;
#else
  mutable std::mutex mutex_;
#endif

  JoystickInput joystickInput_{};
  bool hasJoystickInput_ = false;

  CommunicationStatus commStatus_{};
  bool hasCommStatus_ = false;
};
