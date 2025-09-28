#pragma once

#include "config/ConfigManager.h"
#include <WiFi.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <string>
#include <vector>

class MqttBroker {
 public:
  static constexpr int kMaxClients = 8;
  static constexpr int kMaxTopics = 50;
  static constexpr int kKeepAliveSeconds = 60;

  struct Stats {
    bool brokerActive = false;
    int port = 1883;
    int connectedClients = 0;
    int maxClients = kMaxClients;
    int totalMessages = 0;
    int activeTopics = 0;
    unsigned long uptimeMs = 0;
    unsigned long startTime = 0;
    std::string lastTopic;
    std::string lastPayload;
  };

  MqttBroker();
  ~MqttBroker();

  // 初期化・設定
  bool applyConfig(const ConfigManager::Config &config);
  bool start(int port = 1883);
  void stop();
  void loop();

  // 状態確認
  bool isEnabled() const { return enabled_; }
  bool isActive() const { return brokerActive_; }
  int getConnectedClients() const { return connectedClients_; }
  Stats getStats() const;

  // メッセージ配信
  bool publish(const char* topic, const char* payload, bool retain = false);
  bool publishJoystickState(float leftX, float leftY, float rightX, float rightY, 
                           bool buttonA, bool buttonB, bool leftClick, bool rightClick);
  bool publishSystemStatus(const char* status);
  bool publishWiFiClients(int clientCount);

 private:
  bool enabled_ = false;
  bool brokerActive_ = false;
  int brokerPort_ = 1883;
  int connectedClients_ = 0;
  
  WiFiServer *server_ = nullptr;
  uint32_t lastLogMs_ = 0;
  int totalMessages_ = 0;
  Stats stats_;
};
