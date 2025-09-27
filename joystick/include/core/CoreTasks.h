#pragma once

#include "config/ConfigManager.h"
#include "core/CoreTask.h"
#include "core/SharedState.h"

#include <WiFi.h>
#include <cstdint>

class Core0Task : public CoreTask {
 public:
  Core0Task(const CoreTask::TaskConfig &config,
            SharedState &sharedState,
            ConfigManager &configManager);

 protected:
  void setup() override;
  void loop() override;

 private:
  bool ensureWifiAp();
  bool startWifiAp(const ConfigManager::Config &config);
  bool configureSoftAp(const ConfigManager::WifiConfig &apConfig);
  static bool parseIp(const std::string &text, IPAddress &out);

  SharedState &sharedState_;
  ConfigManager &configManager_;
  bool configLoaded_ = false;
  uint32_t lastConfigLogMs_ = 0;
  std::uint32_t sequence_ = 0;
  bool wifiInitialized_ = false;
  uint32_t lastWifiLogMs_ = 0;
};

class Core1Task : public CoreTask {
 public:
  Core1Task(const CoreTask::TaskConfig &config, SharedState &sharedState);

 protected:
  void setup() override;
  void loop() override;

 private:
 SharedState &sharedState_;
  std::uint32_t lastLoggedSequence_ = 0;
  bool hasLogged_ = false;
  std::uint32_t lastLogMs_ = 0;
  std::uint32_t lastCommLogMs_ = 0;
};
