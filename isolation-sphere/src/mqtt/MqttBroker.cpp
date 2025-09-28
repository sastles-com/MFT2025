#include "mqtt/MqttBroker.h"
#include <Arduino.h>

MqttBroker::MqttBroker() {}

MqttBroker::~MqttBroker() {
  stop();
}

bool MqttBroker::applyConfig(const ConfigManager::Config &config) {
  const auto &mqttConfig = config.mqtt;
  
  enabled_ = mqttConfig.enabled;
  if (!enabled_) {
    Serial.println("[MQTT] MQTT broker disabled in config");
    if (brokerActive_) {
      stop();
    }
    return true;
  }

  brokerPort_ = mqttConfig.port == 0 ? 1883 : mqttConfig.port;
  
  // Start broker if not already active
  if (!brokerActive_) {
    return start(brokerPort_);
  }

  return true;
}

bool MqttBroker::start(int port) {
  if (brokerActive_) {
    Serial.println("[MQTT] Broker already running");
    return true;
  }

  // Check WiFi AP is active
  if (WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) {
    Serial.println("[MQTT] WiFi AP must be active before starting MQTT broker");
    return false;
  }

  server_ = new WiFiServer(port);
  server_->begin();
  brokerActive_ = true;
  brokerPort_ = port;
  stats_.brokerActive = true;
  stats_.port = port;
  stats_.startTime = millis();
  
  Serial.printf("[MQTT] Broker started on port %d\n", port);
  return true;
}

void MqttBroker::stop() {
  if (!brokerActive_) return;

  if (server_) {
    server_->end();
    delete server_;
    server_ = nullptr;
  }

  brokerActive_ = false;
  stats_.brokerActive = false;
  Serial.println("[MQTT] Broker stopped");
}

void MqttBroker::loop() {
  if (!enabled_ || !brokerActive_) {
    return;
  }

  const uint32_t now = millis();
  
  if (now - lastLogMs_ >= 60000) { // 60秒間隔
    Serial.printf("[MQTT] Broker Status: Port %d, Messages: %d\n", 
                 brokerPort_, totalMessages_);
    lastLogMs_ = now;
  }
}

MqttBroker::Stats MqttBroker::getStats() const {
  return stats_;
}

bool MqttBroker::publish(const char* topic, const char* payload, bool retain) {
  if (!brokerActive_ || !topic || !payload) {
    return false;
  }

  Serial.printf("[MQTT] Publish: %s = %s (retain: %s)\n", 
                topic, payload, retain ? "true" : "false");
  
  totalMessages_++;
  stats_.totalMessages = totalMessages_;
  stats_.lastTopic = topic;
  stats_.lastPayload = payload;
  
  return true;
}

bool MqttBroker::publishJoystickState(float leftX, float leftY, float rightX, float rightY, 
                                     bool buttonA, bool buttonB, bool leftClick, bool rightClick) {
  if (!brokerActive_) return false;
  
  char payload[256];
  snprintf(payload, sizeof(payload), 
           "{\"leftX\":%.2f,\"leftY\":%.2f,\"rightX\":%.2f,\"rightY\":%.2f,"
           "\"buttonA\":%s,\"buttonB\":%s,\"leftClick\":%s,\"rightClick\":%s}",
           leftX, leftY, rightX, rightY,
           buttonA ? "true" : "false", buttonB ? "true" : "false",
           leftClick ? "true" : "false", rightClick ? "true" : "false");
  
  return publish("joystick/state", payload, false);
}

bool MqttBroker::publishSystemStatus(const char* status) {
  if (!brokerActive_) return false;
  return publish("joystick/system/status", status, true);
}

bool MqttBroker::publishWiFiClients(int clientCount) {
  if (!brokerActive_) return false;
  
  char payload[64];
  snprintf(payload, sizeof(payload), "{\"clients\":%d}", clientCount);
  return publish("joystick/system/wifi_clients", payload, true);
}
