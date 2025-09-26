#pragma once

#include "config/ConfigManager.h"
#include "core/CoreTask.h"
#include "core/SharedState.h"
#include "storage/StorageManager.h"
#include "imu/ImuService.h"
#include "ota/OtaService.h"
#include "mqtt/MqttService.h"

class Core0Task : public CoreTask {
 public:
  Core0Task(const TaskConfig &config, ConfigManager &configManager, StorageManager &storageManager, SharedState &sharedState);

 protected:
  void setup() override;
  void loop() override;

 private:
 ConfigManager &configManager_;
  StorageManager &storageManager_;
  SharedState &sharedState_;
  bool configLoaded_ = false;
  OtaService otaService_;
  bool otaInitialized_ = false;
  uint32_t nextOtaRetryMs_ = 0;
  MqttService mqttService_;
  bool mqttConfigured_ = false;
};

class Core1Task : public CoreTask {
 public:
  Core1Task(const TaskConfig &config, SharedState &sharedState);

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
#ifdef UNIT_TEST
 public:
  void setImuHooksForTest(ImuService::Hooks hooks);
#endif

 public:
 void markImuWireInitialized();
  void requestImuCalibration(std::uint8_t seconds = 10);

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
