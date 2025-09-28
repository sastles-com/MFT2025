#include "core/SphereCoreTask.h"
#include <LittleFS.h>

SphereCore0Task::SphereCore0Task(const TaskConfig &cfg,
                                 ConfigManager &configManager,
                                 StorageManager &storageManager,
                                 SharedState &sharedState)
  : CoreTask(cfg),
    configManager_(configManager),
    storageManager_(storageManager),
    sharedState_(sharedState),
    wifiManager_(nullptr),
    otaService_(),
    mqttService_(sharedState_) {   // 修正: 引数付きコンストラクタ呼び出し
}

SphereCore0Task::~SphereCore0Task() {
  if (wifiManager_) {
    delete wifiManager_;
    wifiManager_ = nullptr;
  }
}

void SphereCore0Task::setup() {
  Serial.println("[Core0] setup start");

  if (!LittleFS.begin()) {
    Serial.println("[Core0] LittleFS mount failed");
  }

  if (LittleFS.exists("/config.json")) {
    if (configManager_.load()) {
      sharedState_.updateConfig(configManager_.config());
      configLoaded_ = true;
      Serial.println("[Core0] Config loaded");
    } else {
      Serial.println("[Core0] Config load failed");
    }
  } else {
    Serial.println("[Core0] /config.json not found");
  }

  wifiManager_ = new WiFiManager();
  Serial.println("[Core0] setup done");
}

void SphereCore0Task::loop() {
  if (!configLoaded_) {
    return;
  }

  const auto &cfg = configManager_.config();

  if (!wifiConfigured_ && wifiManager_) {
    if (wifiManager_->initialize(cfg)) {
      wifiConfigured_ = true;
      Serial.println("[Core0] WiFiManager initialized");
    }
  }

  if (wifiConfigured_ && wifiManager_) {
    wifiManager_->loop();
  }

  if (!otaInitialized_) {
    uint32_t now = millis();
    if (now >= nextOtaRetryMs_) {
      if (otaService_.begin(cfg)) {
        otaInitialized_ = true;
        Serial.println("[Core0] OTA service initialized");
      } else {
        nextOtaRetryMs_ = now + 10000;
      }
    }
  }

  if (wifiConfigured_ && !mqttConfigured_) {
    mqttConfigured_ = mqttService_.applyConfig(cfg);
    if (mqttConfigured_) {
      Serial.println("[Core0] MQTT client configured");
    }
  }

  if (mqttConfigured_) {
    mqttService_.loop();
  }
}

// ---- SphereCore1Task minimal stubs (既存実装が別にあるならそちら優先) ----
SphereCore1Task::SphereCore1Task(const TaskConfig &config, SharedState &sharedState)
  : CoreTask(config),
    sharedState_(sharedState) {}

void SphereCore1Task::setup() {
  // 既存詳細実装があるなら移植
}

void SphereCore1Task::loop() {
  // 既存詳細実装があるなら移植
}

void SphereCore1Task::markImuWireInitialized() {
  // Sphere専用: IMU I2Cワイヤの初期化完了を通知
  Serial.println("[SphereCore1] IMU wire initialized");
}

void SphereCore1Task::requestImuCalibration(std::uint8_t seconds) {
  // Sphere専用: IMUキャリブレーション要求
  Serial.printf("[SphereCore1] IMU calibration requested for %d seconds\n", seconds);
  Serial.printf("[SphereCore1] IMU calibration requested for %d seconds\n", seconds);
}

#ifdef UNIT_TEST
// テスト用: IMU のフックを差し替える（本番コードでは未使用）
void SphereCore1Task::setImuHooksForTest(ImuService::Hooks hooks) {
  imuService_.setHooksForTest(hooks);
}
#endif
