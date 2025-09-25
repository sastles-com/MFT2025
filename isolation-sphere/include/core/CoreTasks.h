#pragma once

#include "config/ConfigManager.h"
#include "core/CoreTask.h"
#include "core/SharedState.h"
#include "storage/StorageManager.h"
#include "imu/ImuService.h"

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
};

class Core1Task : public CoreTask {
 public:
  Core1Task(const TaskConfig &config, SharedState &sharedState);

 protected:
  void setup() override;
  void loop() override;

 private:
 SharedState &sharedState_;
  bool displayedConfig_ = false;
  ImuService imuService_;
  bool imuInitialized_ = false;
  bool imuEnabled_ = false;
  std::uint32_t imuIntervalMs_ = 33;
  std::uint32_t lastImuReadMs_ = 0;
  std::uint32_t nextImuRetryMs_ = 0;
  bool imuDebugLogging_ = false;
#ifdef UNIT_TEST
 public:
  void setImuHooksForTest(ImuService::Hooks hooks);
#endif

 public:
 void markImuWireInitialized();
  void requestImuCalibration(std::uint8_t seconds = 10);
};
