#include "mqtt/MqttService.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include <cstring>

MqttService::MqttService(SharedState &sharedState) : sharedState_(sharedState) {
  client_.onConnect([this](bool /*sessionPresent*/) {
    connected_ = true;
    lastStatusMs_ = 0;  // publish immediately
    // Subscribe to backward compatibility topics
    if (!topicUi_.empty()) {
      client_.subscribe(topicUi_.c_str(), 1);
    }
    if (!topicCommand_.empty()) {
      client_.subscribe(topicCommand_.c_str(), 1);
    }
    
    // Subscribe to individual device topics
    if (!topicUiIndividual_.empty()) {
      client_.subscribe(topicUiIndividual_.c_str(), 1);
    }
    if (!topicCommandIndividual_.empty()) {
      client_.subscribe(topicCommandIndividual_.c_str(), 1);
    }
    
    // Subscribe to broadcast topics
    if (!topicUiAll_.empty()) {
      client_.subscribe(topicUiAll_.c_str(), 1);
    }
    if (!topicCommandAll_.empty()) {
      client_.subscribe(topicCommandAll_.c_str(), 1);
    }
    if (!topicSync_.empty()) {
      client_.subscribe(topicSync_.c_str(), 2);  // QoS 2 for critical sync commands
    }
    if (!topicEmergency_.empty()) {
      client_.subscribe(topicEmergency_.c_str(), 2);  // QoS 2 for emergency commands
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
                     topicUi_ != config.mqtt.topicUi || topicUiAll_ != config.mqtt.topicUiAll ||
                     topicStatus_ != config.mqtt.topicStatus || topicImage_ != config.mqtt.topicImage ||
                     topicCommand_ != config.mqtt.topicCommand || topicCommandAll_ != config.mqtt.topicCommandAll ||
                     wifiConfig_.ssid != config.wifi.ssid || wifiConfig_.password != config.wifi.password;

  broker_ = config.mqtt.broker;
  port_ = config.mqtt.port == 0 ? 1883 : config.mqtt.port;
  
  // Backward compatibility topics
  topicUi_ = config.mqtt.topicUi.empty() ? "sphere/ui" : config.mqtt.topicUi;
  topicImage_ = config.mqtt.topicImage.empty() ? "sphere/image" : config.mqtt.topicImage;
  topicCommand_ = config.mqtt.topicCommand.empty() ? "sphere/command" : config.mqtt.topicCommand;
  
  // Individual device topics
  topicUiIndividual_ = config.mqtt.topicUiIndividual.empty() ? "sphere/001/ui" : config.mqtt.topicUiIndividual;
  topicImageIndividual_ = config.mqtt.topicImageIndividual.empty() ? "sphere/001/image" : config.mqtt.topicImageIndividual;
  topicCommandIndividual_ = config.mqtt.topicCommandIndividual.empty() ? "sphere/001/command" : config.mqtt.topicCommandIndividual;
  topicStatus_ = config.mqtt.topicStatus.empty() ? "sphere/001/status" : config.mqtt.topicStatus;
  topicInput_ = config.mqtt.topicInput.empty() ? "sphere/001/input" : config.mqtt.topicInput;
  
  // Broadcast topics
  topicUiAll_ = config.mqtt.topicUiAll.empty() ? "sphere/all/ui" : config.mqtt.topicUiAll;
  topicImageAll_ = config.mqtt.topicImageAll.empty() ? "sphere/all/image" : config.mqtt.topicImageAll;
  topicCommandAll_ = config.mqtt.topicCommandAll.empty() ? "sphere/all/command" : config.mqtt.topicCommandAll;
  topicSync_ = config.mqtt.topicSync.empty() ? "system/all/sync" : config.mqtt.topicSync;
  topicEmergency_ = config.mqtt.topicEmergency.empty() ? "system/all/emergency" : config.mqtt.topicEmergency;
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
  bool uiMode = false;
  if (sharedState_.getUiMode(uiMode)) {
    doc["ui_mode"] = uiMode;
  } else {
    doc["ui_mode"] = false;
  }

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

bool MqttService::publishUiEvent(const std::string &command, const char *source) {
  if (!enabled_ || !connected_ || topicUi_.empty() || command.empty()) {
    return false;
  }

  StaticJsonDocument<128> doc;
  doc["command"] = command.c_str();
  doc["timestamp"] = static_cast<uint32_t>(millis());
  if (source != nullptr) {
    doc["source"] = source;
  }

  std::string payload;
  serializeJson(doc, payload);

  const auto packetId = client_.publish(topicUi_.c_str(), 1, false, payload.c_str(), payload.size());
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
  
  Serial.printf("[MQTT] Received message on topic: %s\n", topic);
  
  // Handle UI topics (backward compatibility + individual + broadcast)
  if ((!topicUi_.empty() && topicUi_ == topic) ||
      (!topicUiIndividual_.empty() && topicUiIndividual_ == topic) ||
      (!topicUiAll_.empty() && topicUiAll_ == topic)) {
    Serial.printf("[MQTT] Processing UI command: %s\n", payload.c_str());
    if (tryParseUiMessage(payload)) {
      return;
    }
    sharedState_.pushUiCommand(payload, true);
    return;
  }
  
  // Handle Command topics (backward compatibility + individual + broadcast)
  if ((!topicCommand_.empty() && topicCommand_ == topic) ||
      (!topicCommandIndividual_.empty() && topicCommandIndividual_ == topic) ||
      (!topicCommandAll_.empty() && topicCommandAll_ == topic)) {
    Serial.printf("[MQTT] Processing system command: %s\n", payload.c_str());
    sharedState_.pushSystemCommand(payload, true);
    return;
  }
  
  // Handle System Sync commands
  if (!topicSync_.empty() && topicSync_ == topic) {
    Serial.printf("[MQTT] Processing sync command: %s\n", payload.c_str());
    sharedState_.pushSystemCommand(payload, true);
    return;
  }
  
  // Handle Emergency commands
  if (!topicEmergency_.empty() && topicEmergency_ == topic) {
    Serial.printf("[MQTT] Processing EMERGENCY command: %s\n", payload.c_str());
    sharedState_.pushSystemCommand(payload, true);
    return;
  }
  
  Serial.printf("[MQTT] Unhandled topic: %s\n", topic);
}

void MqttService::resetIncomingBuffer(size_t totalLength) {
  incomingBuffer_.clear();
  incomingBuffer_.resize(totalLength);
}

bool MqttService::tryParseUiMessage(const std::string &payload) {
  if (payload.empty()) {
    return false;
  }

  StaticJsonDocument<256> doc;
  auto error = deserializeJson(doc, payload);
  if (error) {
    return false;
  }

  const char *command = doc["command"];
  if (command && command[0] != '\0') {
    sharedState_.pushUiCommand(command, true);
  }

  // Additional settings (dim/overlay) can be parsed here in the future.

  return true;
}
