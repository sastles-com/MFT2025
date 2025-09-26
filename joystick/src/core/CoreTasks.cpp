#include "core/CoreTasks.h"

#include <Arduino.h>

Core0Task::Core0Task(const CoreTask::TaskConfig &config, SharedState &sharedState)
    : CoreTask(config), sharedState_(sharedState) {}

void Core0Task::setup() {
  Serial.println("[Core0] Joystick controller task started");
}

void Core0Task::loop() {
  SharedState::JoystickInput input;
  input.sequence = ++sequence_;
  input.timestampMs = millis();
  input.leftX = static_cast<std::int16_t>((sequence_ % 200) - 100);
  input.leftY = static_cast<std::int16_t>(100 - (sequence_ % 200));
  input.rightX = static_cast<std::int16_t>((sequence_ % 150) - 75);
  input.rightY = static_cast<std::int16_t>((sequence_ % 90) - 45);
  input.leftButton = (sequence_ % 40) < 20;
  input.rightButton = (sequence_ % 60) < 10;
  sharedState_.setJoystickInput(input);

  SharedState::CommunicationStatus status{};
  sharedState_.getCommunicationStatus(status);
  status.wifiConnected = true;
  status.udpSent = sequence_;
  if (sequence_ % 50 == 0) {
    status.udpErrors += 1;
  }
  sharedState_.setCommunicationStatus(status);
}

Core1Task::Core1Task(const CoreTask::TaskConfig &config, SharedState &sharedState)
    : CoreTask(config), sharedState_(sharedState) {}

void Core1Task::setup() {
  Serial.println("[Core1] Telemetry task started");
}

void Core1Task::loop() {
  const std::uint32_t now = millis();
  SharedState::JoystickInput input;
  if (sharedState_.getJoystickInput(input)) {
    const bool updated = (!hasLogged_) || (input.sequence != lastLoggedSequence_);
    const bool due = (!hasLogged_) || (now - lastLogMs_ >= 1000);
    if (updated && due) {
      Serial.printf("[Core1] seq=%lu left(%d,%d) right(%d,%d) buttons L:%d R:%d\n",
                    static_cast<unsigned long>(input.sequence),
                    input.leftX,
                    input.leftY,
                    input.rightX,
                    input.rightY,
                    input.leftButton,
                    input.rightButton);
      lastLoggedSequence_ = input.sequence;
      lastLogMs_ = now;
      hasLogged_ = true;
    }
  }

  SharedState::CommunicationStatus status{};
  if (sharedState_.getCommunicationStatus(status)) {
    if (now - lastCommLogMs_ >= 2000) {
      Serial.printf("[Core1] comm wifi=%s sent=%lu errors=%lu\n",
                    status.wifiConnected ? "ON" : "OFF",
                    static_cast<unsigned long>(status.udpSent),
                    static_cast<unsigned long>(status.udpErrors));
      lastCommLogMs_ = now;
    }
  }
}
