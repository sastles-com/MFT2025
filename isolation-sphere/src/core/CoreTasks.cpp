#include "core/CoreTasks.h"

#include <Arduino.h>
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
    }
  }

  if (imuEnabled_ && imuInitialized_) {
    if (imuIntervalMs_ == 0 || now - lastImuReadMs_ >= imuIntervalMs_) {
      ImuService::Reading reading;
      if (imuService_.read(reading)) {
        sharedState_.updateImuReading(reading);
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
