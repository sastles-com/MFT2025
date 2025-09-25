#include "core/CoreTasks.h"

#include <Arduino.h>
#include <ESP.h>
#include <cmath>
#include <utility>

namespace {
constexpr std::uint32_t kImuRetryDelayMs = 5000;
}

Core0Task::Core0Task(const TaskConfig &config,
                     ConfigManager &configManager,
                     StorageManager &storageManager,
                     SharedState &sharedState)
    : CoreTask(config),
      configManager_(configManager),
      storageManager_(storageManager),
      sharedState_(sharedState) {}

void Core0Task::setup() {
  Serial.println("[Core0] Task setup complete");
}

void Core0Task::loop() {
  if (!configLoaded_ && storageManager_.isLittleFsMounted()) {
    if (configManager_.load()) {
      sharedState_.updateConfig(configManager_.config());
      configLoaded_ = true;
      Serial.println("[Core0] Config loaded and shared");
    }
  }
  if (configLoaded_) {
    if (!otaInitialized_) {
      uint32_t now = millis();
      if (now >= nextOtaRetryMs_) {
        if (otaService_.begin(configManager_.config())) {
          otaInitialized_ = true;
        } else {
          nextOtaRetryMs_ = now + 5000;
        }
      }
    } else {
      otaService_.loop();
      if (otaService_.shouldReboot()) {
        Serial.println("[OTA] Rebooting to finalize update");
        delay(500);
        ESP.restart();
      }
    }
  }
  sleep(config().loopIntervalMs);
}

Core1Task::Core1Task(const TaskConfig &config, SharedState &sharedState)
    : CoreTask(config), sharedState_(sharedState) {}

#ifdef UNIT_TEST
void Core1Task::setImuHooksForTest(ImuService::Hooks hooks) {
  imuService_.setHooksForTest(std::move(hooks));
}
#endif

void Core1Task::markImuWireInitialized() {
  imuService_.markWireInitialized();
}

void Core1Task::requestImuCalibration(std::uint8_t seconds) {
  imuService_.requestCalibration(seconds);
}

void Core1Task::setup() {
  Serial.println("[Core1] Task setup complete");
  sharedState_.setUiMode(false);
}

void Core1Task::loop() {
  ConfigManager::Config cfg;
  const bool haveConfig = sharedState_.getConfigCopy(cfg);
  const std::uint32_t now = millis();
  if (haveConfig) {
    if (!displayedConfig_) {
      Serial.printf("[Core1] Config name=%s\n", cfg.system.name.c_str());
      displayedConfig_ = true;
    }

    if (cfg.imu.enabled) {
      if (!imuEnabled_) {
        Serial.println("[Core1] IMU enabled via config");
        imuEnabled_ = true;
        imuInitialized_ = false;
        nextImuRetryMs_ = 0;
      }

      imuIntervalMs_ = cfg.imu.updateIntervalMs == 0 ? 0 : cfg.imu.updateIntervalMs;
      imuDebugLogging_ = cfg.imu.gestureDebugLog;
      imuConfig_ = cfg.imu;
      gestureUiModeEnabled_ = cfg.imu.gestureUiMode;
      gestureThresholdMps2_ = (cfg.imu.gestureThresholdMps2 > 0.0f)
                                  ? cfg.imu.gestureThresholdMps2
                                  : kDefaultShakeThresholdMps2_;
      gestureWindowMs_ = (cfg.imu.gestureWindowMs > 0)
                             ? cfg.imu.gestureWindowMs
                             : kDefaultShakeWindowMs_;
      if (!gestureUiModeEnabled_) {
        shakeEventCount_ = 0;
        shakeFirstEventMs_ = 0;
        shakeLastPeakMs_ = 0;
        if (uiModeActive_) {
          uiModeActive_ = false;
          sharedState_.setUiMode(uiModeActive_);
        }
      }

      if (!imuInitialized_ && now >= nextImuRetryMs_) {
        Serial.println("[Core1] Initializing IMU...");
        if (imuService_.begin()) {
          imuInitialized_ = true;
          lastImuReadMs_ = now;
          Serial.println("[Core1] IMU initialization successful");
        } else {
          imuInitialized_ = false;
          nextImuRetryMs_ = now + kImuRetryDelayMs;
          Serial.println("[Core1] IMU initialization failed, retry scheduled");
        }
      }
    } else {
      if (imuEnabled_) {
        Serial.println("[Core1] IMU disabled via config");
      }
      imuEnabled_ = false;
      imuInitialized_ = false;
      nextImuRetryMs_ = 0;
      imuDebugLogging_ = false;
      gestureUiModeEnabled_ = false;
      shakeEventCount_ = 0;
      shakeFirstEventMs_ = 0;
      shakeLastPeakMs_ = 0;
    }
  }

  if (imuEnabled_ && imuInitialized_) {
    if (imuIntervalMs_ == 0 || now - lastImuReadMs_ >= imuIntervalMs_) {
      ImuService::Reading reading;
      if (imuService_.read(reading)) {
        sharedState_.updateImuReading(reading);
        if (gestureUiModeEnabled_) {
          handleShakeGesture(reading);
        }
        if (imuDebugLogging_) {
          Serial.printf("[Core1][IMU] q=(%.3f, %.3f, %.3f, %.3f) ts=%lu\n",
                        reading.qw,
                        reading.qx,
                        reading.qy,
                        reading.qz,
                        static_cast<unsigned long>(reading.timestampMs));
        }
      } else if (imuDebugLogging_) {
        Serial.println("[Core1][IMU] read failed");
      }
      lastImuReadMs_ = now;
    }
  }
  sleep(config().loopIntervalMs);
}

void Core1Task::handleShakeGesture(const ImuService::Reading &reading) {
  constexpr float kGravity = 9.80665f;
  const float magnitude = reading.accelMagnitudeMps2;
  if (!std::isfinite(magnitude)) {
    return;
  }

  const float linearAccel = fabsf(magnitude - kGravity);
  if (imuDebugLogging_) {
    Serial.printf("[Core1][IMU] linear accel %.3f m/s^2\n", linearAccel);
  }

  if (linearAccel < gestureThresholdMps2_) {
    return;
  }

  const uint32_t now = reading.timestampMs != 0 ? reading.timestampMs : millis();
  if (now - shakeLastPeakMs_ < kShakeRefractoryMs_) {
    return;
  }

  shakeLastPeakMs_ = now;
  if (shakeEventCount_ == 0 || (now - shakeFirstEventMs_) > gestureWindowMs_) {
    shakeEventCount_ = 0;
    shakeFirstEventMs_ = now;
  }

  ++shakeEventCount_;
  if (imuDebugLogging_) {
    Serial.printf("[Core1][IMU] shake event count=%u\n", static_cast<unsigned>(shakeEventCount_));
  }

  if (shakeEventCount_ >= 2) {
    shakeEventCount_ = 0;
    shakeFirstEventMs_ = 0;
    uiModeActive_ = !uiModeActive_;
    sharedState_.setUiMode(uiModeActive_);
    Serial.printf("[Core1][IMU] Shake gesture detected -> UI mode %s\n",
                  uiModeActive_ ? "ON" : "OFF");
  }
}
