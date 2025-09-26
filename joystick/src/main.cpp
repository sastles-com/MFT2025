#include <Arduino.h>

#include "core/CoreTasks.h"
#include "core/SharedState.h"

namespace {

SharedState gSharedState;

CoreTask::TaskConfig makeConfig(const char *name,
                                int coreId,
                                std::uint32_t intervalMs,
                                std::uint32_t stackSize = 4096,
                                std::uint32_t priority = 1) {
  CoreTask::TaskConfig cfg;
  cfg.name = name;
  cfg.stackSize = stackSize;
  cfg.priority = priority;
  cfg.coreId = coreId;
  cfg.loopIntervalMs = intervalMs;
  return cfg;
}

Core0Task gCore0(makeConfig("Core0Task", 0, 5), gSharedState);
Core1Task gCore1(makeConfig("Core1Task", 1, 5), gSharedState);

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("==============================");
  Serial.println(" MFT2025 Joystick Controller");
  Serial.println("==============================");

  if (!gCore0.start()) {
    Serial.println("[Main] Failed to start Core0 task");
  }
  if (!gCore1.start()) {
    Serial.println("[Main] Failed to start Core1 task");
  }
}

void loop() {
  delay(1000);
}
