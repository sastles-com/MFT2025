#pragma once
#include <Arduino.h>
#include "core/CoreTask.h"
#include "core/SharedState.h"
#include "config/ConfigManager.h"
#include "storage/StorageManager.h"
#include "ota/OtaService.h"
#include "wifi/WiFiManager.h"
#include "mqtt/MqttService.h"

// Isolation-sphere専用のCore0タスク実装
// joystick側のJoystickCoreTaskとは別実装
class SphereCore0Task : public CoreTask {
 public:
  SphereCore0Task(const TaskConfig &cfg,
                  ConfigManager &configManager,
                  StorageManager &storageManager,
                  SharedState &sharedState);
  ~SphereCore0Task();

 protected:
  void setup() override;
  void loop() override;

 private:
  ConfigManager &configManager_;
  StorageManager &storageManager_;
  SharedState &sharedState_;
  WiFiManager *wifiManager_ = nullptr;
  OtaService otaService_;
  MqttService mqttService_;   // sharedState_ を渡して構築

  bool configLoaded_ = false;
  bool wifiConfigured_ = false;
  bool otaInitialized_ = false;
  bool mqttConfigured_ = false;
  uint32_t nextOtaRetryMs_ = 0;
};

// Isolation-sphere専用のCore1タスク実装
// joystick側のJoystickCoreTaskとは別実装
class SphereCore1Task : public CoreTask {
 public:
  SphereCore1Task(const TaskConfig &config, SharedState &sharedState);

 protected:
  void setup() override;
  void loop() override;

 private:
  enum class UiInteractionMode : std::uint8_t {
    kNavigation,
    kBrightnessAdjust,
    kCentering,
  };

  SharedState &sharedState_;
  bool displayedConfig_ = false;
  ImuService imuService_;
  bool imuInitialized_ = false;
  bool imuEnabled_ = false;
  std::uint32_t imuIntervalMs_ = 33;
  std::uint32_t lastImuReadMs_ = 0;
  std::uint32_t nextImuRetryMs_ = 0;
  bool imuDebugLogging_ = false;
  ConfigManager::ImuConfig imuConfig_{};
  bool gestureUiModeEnabled_ = false;
  bool uiModeActive_ = false;
  std::uint32_t shakeFirstEventMs_ = 0;
  std::uint32_t shakeLastPeakMs_ = 0;
  std::uint8_t shakeEventCount_ = 0;
  float gestureThresholdMps2_ = kDefaultShakeThresholdMps2_;
  std::uint32_t gestureWindowMs_ = kDefaultShakeWindowMs_;
  static constexpr float kDefaultShakeThresholdMps2_ = 5.0f;
  static constexpr std::uint32_t kDefaultShakeWindowMs_ = 600;
  static constexpr std::uint32_t kShakeRefractoryMs_ = 200;

  void handleShakeGesture(const ImuService::Reading &reading);
  void enterUiMode();
  void exitUiMode();
  void processUiMode(const ImuService::Reading &reading);
  void updateUiReference(const ImuService::Reading &reading);
  void handleUiCommand(const std::string &command, bool external);
  void triggerLocalUiCommand(const char *command);
  void applyUiBrightnessSettings(bool entering);
  void processIncomingUiCommands();

 public:
  void markImuWireInitialized();
  void requestImuCalibration(std::uint8_t seconds = 10);

#ifdef UNIT_TEST
 public:
  void setImuHooksForTest(ImuService::Hooks hooks);
#endif

 private:
  ImuService::Reading lastImuReading_{};
  ConfigManager::UiConfig uiConfig_{};
  bool uiGestureEnabled_ = true;
  UiInteractionMode uiInteractionMode_ = UiInteractionMode::kNavigation;
  bool uiModeDimmed_ = false;
  uint8_t uiPreviousBrightness_ = 128;
  float uiReferenceRoll_ = 0.0f;
  float uiReferencePitch_ = 0.0f;
  float uiReferenceYaw_ = 0.0f;
  bool uiXPositiveReady_ = true;
  bool uiXNegativeReady_ = true;
  uint32_t uiCommandCooldownEndMs_ = 0;

  static constexpr float kUiCommandTriggerDeg_ = 25.0f;
  static constexpr float kUiCommandResetDeg_ = 10.0f;
  static constexpr uint32_t kUiCommandCooldownMs_ = 750;
};
