#include "mqtt/MqttService.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include <cstring>

MqttService::MqttService(SharedState &sharedState) : sharedState_(sharedState) {
  client_.onConnect([this](bool /*sessionPresent*/) {
    connected_ = true;
    lastStatusMs_ = 0;  // publish immediately
    if (!topicUi_.empty()) {
      client_.subscribe(topicUi_.c_str(), 1);
    }
    publishStatus();
  });

  client_.onDisconnect([this](AsyncMqttClientDisconnectReason /*reason*/) {
    connected_ = false;
    lastReconnectMs_ = millis();
  });

  client_.onMessage([this](char *topic, char *payload, AsyncMqttClientMessageProperties /*properties*/, size_t len,
                           size_t index, size_t total) {
    if (index == 0) {
      resetIncomingBuffer(total);
      incomingTopic_.assign(topic ? topic : "");
    }
    if (index + len > incomingBuffer_.size()) {
      incomingBuffer_.resize(index + len);
    }
    if (payload != nullptr && len > 0) {
      std::memcpy(incomingBuffer_.data() + index, payload, len);
    }
    if (index + len == total) {
      std::string message(reinterpret_cast<char *>(incomingBuffer_.data()), total);
      handleIncomingMessage(incomingTopic_.c_str(), message);
    }
  });
}

bool MqttService::applyConfig(const ConfigManager::Config &config) {
  if (!config.mqtt.enabled || config.mqtt.broker.empty()) {
    stop();
    configured_ = false;
    enabled_ = false;
    return false;
  }

  bool newSettings = (!configured_) || broker_ != config.mqtt.broker || port_ != config.mqtt.port ||
                     topicUi_ != config.mqtt.topicUi || topicStatus_ != config.mqtt.topicStatus ||
                     topicImage_ != config.mqtt.topicImage || wifiConfig_.ssid != config.wifi.ssid ||
                     wifiConfig_.password != config.wifi.password;

  broker_ = config.mqtt.broker;
  port_ = config.mqtt.port == 0 ? 1883 : config.mqtt.port;
  topicUi_ = config.mqtt.topicUi.empty() ? "sphere/ui" : config.mqtt.topicUi;
  topicStatus_ = config.mqtt.topicStatus.empty() ? "sphere/status" : config.mqtt.topicStatus;
  topicImage_ = config.mqtt.topicImage.empty() ? "sphere/image" : config.mqtt.topicImage;
  wifiConfig_ = config.wifi;
  clientId_ = config.system.name.empty() ? "isolation-sphere" : config.system.name;

  enabled_ = true;

  if (newSettings) {
    stop();
    client_.setServer(broker_.c_str(), port_);
    client_.setClientId(clientId_.c_str());
    configured_ = true;
    lastReconnectMs_ = 0;
    lastStatusMs_ = 0;
    lastWifiAttemptMs_ = 0;
  }

  return true;
}

void MqttService::loop() {
  if (!enabled_) {
    return;
  }

  ensureWifi();
  connectIfNeeded();

  if (connected_) {
    const uint32_t now = millis();
    if (now - lastStatusMs_ >= kStatusIntervalMs) {
      publishStatus();
    }
  }
}

bool MqttService::publishStatus() {
  if (!enabled_ || !connected_ || topicStatus_.empty()) {
    return false;
  }

  StaticJsonDocument<256> doc;
  doc["status"] = "online";
  doc["uptime_ms"] = static_cast<uint32_t>(millis());
  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  doc["client"] = clientId_;

  std::string payload;
  payload.reserve(128);
  serializeJson(doc, payload);

  const auto packetId = client_.publish(topicStatus_.c_str(), 1, true, payload.c_str(), payload.size());
  if (packetId != 0) {
    lastStatusMs_ = millis();
    return true;
  }
  return false;
}

bool MqttService::publishImage(const uint8_t *data, size_t length, bool retain, uint8_t qos) {
  if (!enabled_ || !connected_ || topicImage_.empty() || data == nullptr || length == 0) {
    return false;
  }
  const auto packetId = client_.publish(topicImage_.c_str(), qos, retain,
                                       reinterpret_cast<const char *>(data), length);
  return packetId != 0;
}

void MqttService::stop() {
  if (connected_) {
    client_.disconnect();
  }
  connected_ = false;
}

void MqttService::ensureWifi() {
  if (wifiConfig_.ssid.empty()) {
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  const uint32_t now = millis();
  if (now - lastWifiAttemptMs_ < kWifiRetryIntervalMs) {
    return;
  }
  lastWifiAttemptMs_ = now;
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiConfig_.ssid.c_str(), wifiConfig_.password.c_str());
}

void MqttService::connectIfNeeded() {
  if (!enabled_ || WiFi.status() != WL_CONNECTED) {
    return;
  }
  if (connected_) {
    return;
  }
  const uint32_t now = millis();
  if (now - lastReconnectMs_ < kReconnectIntervalMs) {
    return;
  }
  lastReconnectMs_ = now;
  client_.connect();
}

void MqttService::handleIncomingMessage(const char *topic, const std::string &payload) {
  if (topic == nullptr) {
    return;
  }
  if (!topicUi_.empty() && topicUi_ == topic) {
    sharedState_.updateUiCommand(payload);
  }
}

void MqttService::resetIncomingBuffer(size_t totalLength) {
  incomingBuffer_.clear();
  incomingBuffer_.resize(totalLength);
}
