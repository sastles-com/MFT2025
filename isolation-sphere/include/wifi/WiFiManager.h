#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config/ConfigManager.h"  // 完全なインクルード

class WiFiManager {
public:
  WiFiManager();
  ~WiFiManager();
  
  bool begin();
  void end();
  bool isConnected();
  String getLocalIP();
  
  // JsonObject版とConfigManager::Config版の両方をサポート
  bool initialize(const JsonObject& config);
  bool initialize(const ConfigManager::Config& config);
  void loop();
  
private:
  bool connected;
};