#include "core/CoreTasks.h"
#include <LittleFS.h>

Core0Task::Core0Task(const TaskConfig &cfg,
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

Core0Task::~Core0Task() {
  if (wifiManager_) {
    delete wifiManager_;
    wifiManager_ = nullptr;
  }
}

void Core0Task::setup() {
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

void Core0Task::loop() {
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

// ---- Core1Task minimal stubs (既存実装が別にあるならそちら優先) ----
Core1Task::Core1Task(const TaskConfig &config, SharedState &sharedState)
  : CoreTask(config),
    sharedState_(sharedState) {}

void Core1Task::setup() {
  // 既存詳細実装があるなら移植
}

void Core1Task::loop() {
  // 既存詳細実装があるなら移植
}
