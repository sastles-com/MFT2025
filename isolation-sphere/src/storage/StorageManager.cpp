#include "storage/StorageManager.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <PSRamFS.h>

namespace {
constexpr const char *kLittleFsBasePath = "/littlefs";
constexpr const char *kLittleFsPartition = "littlefs";

bool defaultLittleFsBegin(bool formatOnFail) {
  return LittleFS.begin(formatOnFail, kLittleFsBasePath, 10, kLittleFsPartition);
}

bool defaultPsRamFsBegin(bool formatOnFail) {
  return PSRamFS.begin(formatOnFail);
}
}

StorageManager::StorageManager(Hooks hooks) : hooks_(std::move(hooks)) {
  if (!hooks_.littlefsBegin) {
    hooks_.littlefsBegin = defaultLittleFsBegin;
  }
  if (!hooks_.psramfsBegin) {
    hooks_.psramfsBegin = defaultPsRamFsBegin;
  }
}

bool StorageManager::begin(bool formatOnLittleFail, bool formatOnPsFail) {
  littleMounted_ = false;
  psMounted_ = false;

  if (!hooks_.littlefsBegin) {
    Serial.println("[Storage] LittleFS begin hook not provided");
    return false;
  }

  littleMounted_ = hooks_.littlefsBegin(false);
  if (!littleMounted_ && formatOnLittleFail) {
    Serial.println("[Storage] LittleFS mount failed, attempting format...");
    littleMounted_ = hooks_.littlefsBegin(true);
  }

  if (!littleMounted_) {
    Serial.println("[Storage] LittleFS mount failed");
    return false;
  }

  // PSRamFS は既知の問題により現時点では無効化
  psMounted_ = false;
  Serial.println("[Storage] PSRamFS disabled (skipping mount)");
  return true;
}

bool StorageManager::isLittleFsMounted() const {
  return littleMounted_;
}

bool StorageManager::isPsRamFsMounted() const {
  return psMounted_;
}
