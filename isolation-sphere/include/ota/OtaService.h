#pragma once

#include "config/ConfigManager.h"

#include <ESPAsyncWebServer.h>
#include <WiFi.h>

class OtaService {
 public:
  OtaService();
  ~OtaService();

  bool begin(const ConfigManager::Config &config);
  void loop();
  bool isActive() const { return active_; }
  bool shouldReboot() const { return needsReboot_; }

 private:
  bool connectWiFi(const ConfigManager::WifiConfig &wifiConfig);
  void setupServer(const ConfigManager::Config &config);
  void handleFsUpload(AsyncWebServerRequest *request,
                      const String &filename,
                      size_t index,
                      uint8_t *data,
                      size_t len,
                      bool final);

  AsyncWebServer server_;
  bool active_ = false;
  bool wifiConnected_ = false;
  bool serverStarted_ = false;
  bool needsReboot_ = false;
  std::string hostname_;
  std::string username_;
  std::string password_;
  ConfigManager::WifiConfig wifiConfig_{};
  bool wifiConfigValid_ = false;
  uint32_t lastWifiAttemptMs_ = 0;
  bool authEnabled_ = false;
};
