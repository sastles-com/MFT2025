#pragma once

#include "config/ConfigManager.h"
#include "core/SharedState.h"

#include <AsyncMqttClient.h>
#include <WiFi.h>

#include <string>
#include <vector>

class MqttService {
 public:
  explicit MqttService(SharedState &sharedState);

  bool applyConfig(const ConfigManager::Config &config);
  void loop();
  bool isEnabled() const { return enabled_; }
  bool isConnected() const { return connected_; }

  bool publishStatus();
  bool publishImage(const uint8_t *data, size_t length, bool retain = false, uint8_t qos = 0);
  void stop();

 private:
  void ensureWifi();
  void connectIfNeeded();
  void handleIncomingMessage(const char *topic, const std::string &payload);
  void resetIncomingBuffer(size_t totalLength);

  SharedState &sharedState_;
  AsyncMqttClient client_;

  bool configured_ = false;
  bool enabled_ = false;
  bool connected_ = false;

  std::string broker_;
  uint16_t port_ = 1883;
  std::string clientId_;
  std::string topicUi_;
  std::string topicStatus_;
  std::string topicImage_;

  ConfigManager::WifiConfig wifiConfig_{};

  std::vector<uint8_t> incomingBuffer_;
  std::string incomingTopic_;

  uint32_t lastWifiAttemptMs_ = 0;
  uint32_t lastReconnectMs_ = 0;
  uint32_t lastStatusMs_ = 0;

  static constexpr uint32_t kWifiRetryIntervalMs = 10000;
  static constexpr uint32_t kReconnectIntervalMs = 5000;
  static constexpr uint32_t kStatusIntervalMs = 10000;
};
