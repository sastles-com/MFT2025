#include "core/CoreTasks.h"

#include <Arduino.h>
#include <ESP.h>
#include <LittleFS.h>
#include <M5Unified.h>
#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr std::uint32_t kImuRetryDelayMs = 5000;

float normalizeAngle(float angle) {
  while (angle > static_cast<float>(M_PI)) {
    angle -= static_cast<float>(2.0 * M_PI);
  }
  while (angle < -static_cast<float>(M_PI)) {
    angle += static_cast<float>(2.0 * M_PI);
  }
  return angle;
}

float quaternionToRoll(const ImuService::Reading &r) {
  const float qw = r.qw;
  const float qx = r.qx;
  const float qy = r.qy;
  const float qz = r.qz;
  const float t0 = +2.0f * (qw * qx + qy * qz);
  const float t1 = +1.0f - 2.0f * (qx * qx + qy * qy);
  return atan2f(t0, t1);
}

float quaternionToPitch(const ImuService::Reading &r) {
  const float qw = r.qw;
  const float qx = r.qx;
  const float qy = r.qy;
  const float qz = r.qz;
  float t2 = +2.0f * (qw * qy - qz * qx);
  if (t2 > 1.0f) t2 = 1.0f;
  if (t2 < -1.0f) t2 = -1.0f;
  return asinf(t2);
}

float quaternionToYaw(const ImuService::Reading &r) {
  const float qw = r.qw;
  const float qx = r.qx;
  const float qy = r.qy;
  const float qz = r.qz;
  const float t3 = +2.0f * (qw * qz + qx * qy);
  const float t4 = +1.0f - 2.0f * (qy * qy + qz * qz);
  return atan2f(t3, t4);
}

constexpr float kRadToDeg = 180.0f / static_cast<float>(M_PI);
}

Core0Task::Core0Task(const TaskConfig &config,
                     ConfigManager &configManager,
                     StorageManager &storageManager,
                     SharedState &sharedState)
    : CoreTask(config),
      configManager_(configManager),
      storageManager_(storageManager),
      sharedState_(sharedState),
      mqttService_(sharedState) {}

Core0Task::~Core0Task() {
  if (wifiManager_) {
    delete wifiManager_;
    wifiManager_ = nullptr;
  }
  if (mqttBroker_) {
    delete mqttBroker_;
    mqttBroker_ = nullptr;
  }
}

void Core0Task::setup() {
  Serial.println("[Core0] Task setup starting...");
  
  // StorageManager初期化
  if (!storageManager_.begin()) {
    Serial.println("[Core0] StorageManager initialization failed");
  } else {
    Serial.println("[Core0] StorageManager initialized successfully");
  }
  
  // WiFiManager初期化
  wifiManager_ = new WiFiManager();
  if (!wifiManager_) {
    Serial.println("[Core0] Failed to allocate WiFiManager");
  } else {
    Serial.println("[Core0] WiFiManager allocated");
  }
  
  // MqttBroker初期化
  mqttBroker_ = new MqttBroker();
  if (!mqttBroker_) {
    Serial.println("[Core0] Failed to allocate MqttBroker");
  } else {
    Serial.println("[Core0] MqttBroker allocated");
  }
  
  Serial.println("[Core0] Task setup complete");
}

void Core0Task::loop() {
  if (!configLoaded_) {
    // StorageManagerをバイパスして直接LittleFSからconfig.jsonを読み込み
    if (LittleFS.exists("/config.json")) {
      if (configManager_.load()) {
        sharedState_.updateConfig(configManager_.config());
        configLoaded_ = true;
        Serial.println("[Core0] Config loaded and shared successfully");
      } else {
        Serial.println("[Core0] Config loading failed");
      }
    } else {
      Serial.println("[Core0] Config file not found: /config.json");
    }
  }
  
  if (configLoaded_) {
    const auto &cfg = configManager_.config();
    
    // WiFiManager設定（1回だけ実行）
    if (wifiManager_ && !wifiConfigured_) {
      if (wifiManager_->initialize(cfg)) {
        wifiConfigured_ = true;
        Serial.println("[Core0] WiFiManager initialized successfully");
      } else {
        Serial.println("[Core0] WiFiManager initialization failed");
      }
    }
    
    // WiFiManagerループ処理
    if (wifiManager_ && wifiConfigured_) {
      wifiManager_->loop();
    }
    
    // MqttBroker設定（WiFi初期化後、1回だけ実行）
    if (mqttBroker_ && !mqttBrokerConfigured_ && wifiConfigured_) {
      if (mqttBroker_->applyConfig(cfg)) {
        mqttBrokerConfigured_ = true;
        Serial.println("[Core0] MqttBroker initialized successfully");
      } else {
        Serial.println("[Core0] MqttBroker initialization failed");
      }
    }
    
    // MqttBrokerループ処理
    if (mqttBroker_ && mqttBrokerConfigured_) {
      mqttBroker_->loop();
    }
    
    if (!otaInitialized_) {
      uint32_t now = millis();
      if (now >= nextOtaRetryMs_) {
        if (otaService_.begin(cfg)) {
          otaInitialized_ = true;
          Serial.println("[Core0] OTA service initialized");
        } else {
          nextOtaRetryMs_ = now + 5000;
          Serial.println("[Core0] OTA initialization failed, retrying in 5s");
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

    mqttConfigured_ = mqttService_.applyConfig(cfg);
    mqttService_.loop();
    if (mqttConfigured_) {
      std::string outgoingCommand;
      if (sharedState_.popUiCommand(outgoingCommand, false)) {
        mqttService_.publishUiEvent(outgoingCommand);
      }
    }
  } else {
    Serial.println("[Core0] Waiting for config to load...");
  }
  sleep(config().loopIntervalMs);
}

Core1Task::Core1Task(const TaskConfig &config, SharedState &sharedState)
    : CoreTask(config), sharedState_(sharedState) {}

Core1Task::~Core1Task() {
  if (buzzerService_) {
    buzzerService_->stop();
    buzzerService_.reset();
  }
}

#ifdef UNIT_TEST
void Core1Task::setImuHooksForTest(ImuService::Hooks hooks) {
  imuService_.setHooksForTest(std::move(hooks));
}

void Core1Task::setBuzzerHooksForTest(BuzzerService::Hooks hooks) {
  buzzerHooksForTest_ = std::move(hooks);
  useBuzzerHooksForTest_ = true;
}
#endif

void Core1Task::markImuWireInitialized() {
  imuService_.markWireInitialized();
}

void Core1Task::requestImuCalibration(std::uint8_t seconds) {
  imuService_.requestCalibration(seconds);
}

void Core1Task::playButtonSound() {
  if (buzzerService_ && buzzerInitialized_) {
    buzzerService_->playEffect(buzzer::Effect::kBeep);
  }
}

void Core1Task::playErrorSound() {
  if (buzzerService_ && buzzerInitialized_) {
    buzzerService_->playEffect(buzzer::Effect::kError);
  }
}

void Core1Task::playSuccessSound() {
  if (buzzerService_ && buzzerInitialized_) {
    buzzerService_->playEffect(buzzer::Effect::kSuccess);
  }
}

void Core1Task::configureBuzzer(const ConfigManager::Config &cfg) {
  const bool shouldEnable = cfg.buzzer.enabled;
  const bool wasEnabled = buzzerEnabled_;
  buzzerEnabled_ = shouldEnable;

  if (shouldEnable) {
    if (!buzzerService_) {
#ifdef UNIT_TEST
      if (useBuzzerHooksForTest_) {
        buzzerService_ = std::unique_ptr<BuzzerService>(new BuzzerService(buzzerHooksForTest_));
      } else {
        buzzerService_ = std::unique_ptr<BuzzerService>(new BuzzerService());
      }
#else
      buzzerService_ = std::unique_ptr<BuzzerService>(new BuzzerService());
#endif
    }

    if (buzzerService_ && !buzzerInitialized_) {
      if (buzzerService_->begin()) {
        buzzerInitialized_ = true;
        Serial.println("[Core1] BuzzerService initialized successfully");
        buzzerService_->playStartupTone();
      } else {
        Serial.println("[Core1] BuzzerService initialization failed");
        buzzerService_.reset();
        buzzerInitialized_ = false;
      }
    }
  } else if (wasEnabled) {
    if (buzzerService_ && buzzerInitialized_) {
      buzzerService_->stop();
    }
    buzzerInitialized_ = false;
  }
}

void Core1Task::setup() {
  Serial.println("[Core1] Task setup starting...");
  
  ConfigManager::Config cfg;
  if (sharedState_.getConfigCopy(cfg)) {
    configureBuzzer(cfg);
  } else {
    Serial.println("[Core1] Config not available for BuzzerService initialization");
  }
  
  Serial.println("[Core1] Task setup complete");
  sharedState_.setUiMode(false);
}

void Core1Task::loop() {
  ConfigManager::Config cfg;
  const bool haveConfig = sharedState_.getConfigCopy(cfg);
  const std::uint32_t now = millis();
  if (haveConfig) {
    configureBuzzer(cfg);

    uiConfig_ = cfg.ui;
    uiGestureEnabled_ = cfg.ui.gestureEnabled;
    if (!uiGestureEnabled_ && uiModeActive_) {
      exitUiMode();
    }

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
        lastImuReading_ = reading;
        sharedState_.updateImuReading(reading);
        if (gestureUiModeEnabled_) {
          handleShakeGesture(reading);
        }
        if (uiModeActive_) {
          processUiMode(reading);
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
    if (imuService_.pollCalibrationCompleted()) {
      playSuccessSound();
    }
  }
  processIncomingUiCommands();
  sleep(config().loopIntervalMs);
}

void Core1Task::handleShakeGesture(const ImuService::Reading &reading) {
  constexpr float kGravity = 9.80665f;
  if (!uiGestureEnabled_) {
    return;
  }

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

  const uint32_t configuredWindow = imuConfig_.uiShakeWindowMs > 0 ? imuConfig_.uiShakeWindowMs : 900;
  const uint32_t windowMs = std::max(gestureWindowMs_ > 0 ? gestureWindowMs_ : configuredWindow, configuredWindow);
  shakeLastPeakMs_ = now;
  if (shakeEventCount_ == 0 || (now - shakeFirstEventMs_) > windowMs) {
    shakeEventCount_ = 0;
    shakeFirstEventMs_ = now;
  }

  ++shakeEventCount_;
  if (imuDebugLogging_) {
    Serial.printf("[Core1][IMU] shake event count=%u\n", static_cast<unsigned>(shakeEventCount_));
  }

  const uint8_t triggerCount = std::max<uint8_t>(imuConfig_.uiShakeTriggerCount > 0 ? imuConfig_.uiShakeTriggerCount : 3, 1);
  if (shakeEventCount_ >= triggerCount) {
    shakeEventCount_ = 0;
    shakeFirstEventMs_ = 0;
    if (uiModeActive_) {
      Serial.println("[Core1][UI] Shake gesture -> UI mode OFF");
      exitUiMode();
    } else {
      Serial.println("[Core1][UI] Shake gesture -> UI mode ON");
      enterUiMode();
    }
  }
}

void Core1Task::enterUiMode() {
  uiModeActive_ = true;
  sharedState_.setUiMode(true);
  uiInteractionMode_ = UiInteractionMode::kNavigation;
  uiXPositiveReady_ = true;
  uiXNegativeReady_ = true;
  uiCommandCooldownEndMs_ = 0;
  updateUiReference(lastImuReading_);
  applyUiBrightnessSettings(true);

  M5.Speaker.tone(880, 80);
  delay(30);
  M5.Speaker.tone(1230, 80);

  if (uiConfig_.overlayMode == ConfigManager::UiConfig::OverlayMode::kBlackout) {
    M5.Display.fillScreen(TFT_BLACK);
  }
}

void Core1Task::exitUiMode() {
  uiModeActive_ = false;
  sharedState_.setUiMode(false);
  uiInteractionMode_ = UiInteractionMode::kNavigation;
  uiXPositiveReady_ = true;
  uiXNegativeReady_ = true;
  uiCommandCooldownEndMs_ = 0;
  applyUiBrightnessSettings(false);
}

void Core1Task::processUiMode(const ImuService::Reading &reading) {
  const uint32_t now = reading.timestampMs != 0 ? reading.timestampMs : millis();
  const float roll = quaternionToRoll(reading);
  const float deltaRollDeg = normalizeAngle(roll - uiReferenceRoll_) * kRadToDeg;

  if (std::fabs(deltaRollDeg) < kUiCommandResetDeg_) {
    uiXPositiveReady_ = true;
    uiXNegativeReady_ = true;
    if (now > uiCommandCooldownEndMs_) {
      uiCommandCooldownEndMs_ = 0;
    }
  }

  if (now < uiCommandCooldownEndMs_) {
    return;
  }

  if (deltaRollDeg > kUiCommandTriggerDeg_ && uiXPositiveReady_) {
    triggerLocalUiCommand("ui:x_pos");
    uiXPositiveReady_ = false;
    uiCommandCooldownEndMs_ = now + kUiCommandCooldownMs_;
    return;
  }

  if (deltaRollDeg < -kUiCommandTriggerDeg_ && uiXNegativeReady_) {
    triggerLocalUiCommand("ui:x_neg");
    uiXNegativeReady_ = false;
    uiCommandCooldownEndMs_ = now + kUiCommandCooldownMs_;
  }
}

void Core1Task::updateUiReference(const ImuService::Reading &reading) {
  uiReferenceRoll_ = quaternionToRoll(reading);
  uiReferencePitch_ = quaternionToPitch(reading);
  uiReferenceYaw_ = quaternionToYaw(reading);
}

void Core1Task::handleUiCommand(const std::string &command, bool external) {
  if (command == "ui:x_pos") {
    Serial.printf("[Core1][UI] Next content requested (%s)\n", external ? "external" : "local");
  } else if (command == "ui:x_neg") {
    Serial.printf("[Core1][UI] Play/Pause toggle (%s)\n", external ? "external" : "local");
  } else if (command == "ui:mode:on") {
    if (!uiModeActive_) {
      enterUiMode();
    }
  } else if (command == "ui:mode:off") {
    if (uiModeActive_) {
      exitUiMode();
    }
  }
}

void Core1Task::triggerLocalUiCommand(const char *command) {
  if (command == nullptr) {
    return;
  }
  sharedState_.pushUiCommand(command, false);
  handleUiCommand(command, false);
}

void Core1Task::applyUiBrightnessSettings(bool entering) {
  if (!uiConfig_.dimOnEntry) {
    return;
  }

  if (entering) {
    uiPreviousBrightness_ = M5.Display.getBrightness();
    uint8_t target = uiPreviousBrightness_ > 0 ? std::max<uint8_t>(uiPreviousBrightness_ / 2, 8) : 64;
    M5.Display.setBrightness(target);
    uiModeDimmed_ = true;
  } else if (uiModeDimmed_) {
    M5.Display.setBrightness(uiPreviousBrightness_);
    uiModeDimmed_ = false;
  }
}

void Core1Task::processIncomingUiCommands() {
  std::string command;
  if (sharedState_.popUiCommand(command, true)) {
    handleUiCommand(command, true);
  }
}
