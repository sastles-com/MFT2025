#pragma once

#include "config/ConfigManager.h"
#include <WiFi.h>
#include <IPAddress.h>

class WiFiManager {
 public:
  WiFiManager();
  ~WiFiManager();

  // 基本機能
  bool initialize(const ConfigManager::Config &config);
  void loop();
  void shutdown();

  // 状態確認
  bool isActive() const { return apActive_; }
  IPAddress getLocalIP() const { return WiFi.softAPIP(); }
  int getClientCount() const { return WiFi.softAPgetStationNum(); }

 private:
  bool apActive_ = false;
};