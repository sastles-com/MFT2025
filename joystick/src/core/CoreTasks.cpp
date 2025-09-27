#include "core/CoreTasks.h"

#include <Arduino.h>

Core0Task::Core0Task(const CoreTask::TaskConfig &config,
                     SharedState &sharedState,
                     ConfigManager &configManager)
    : CoreTask(config), sharedState_(sharedState), configManager_(configManager) {}

void Core0Task::setup() {
  Serial.println("[Core0] Joystick controller task started");

  if (!configManager_.isLoaded()) {
    if (configManager_.load()) {
      sharedState_.setConfig(configManager_.config());
      configLoaded_ = true;
      Serial.println("[Core0] Config loaded successfully");
      ensureWifiAp();
    } else {
      Serial.println("[Core0] Failed to load config.json");
    }
  } else {
    sharedState_.setConfig(configManager_.config());
    configLoaded_ = true;
    ensureWifiAp();
  }
}

void Core0Task::loop() {
  if (!configLoaded_) {
    const uint32_t now = millis();
    if (now - lastConfigLogMs_ > 2000) {
      Serial.println("[Core0] Config not loaded yet. Retrying...");
      lastConfigLogMs_ = now;
    }
    if (configManager_.load()) {
      sharedState_.setConfig(configManager_.config());
      configLoaded_ = true;
      Serial.println("[Core0] Config loaded on retry");
      ensureWifiAp();
    }
  } else if (!wifiInitialized_) {
    ensureWifiAp();
  }

  SharedState::JoystickInput input;
  input.sequence = ++sequence_;
  input.timestampMs = millis();
  input.leftX = static_cast<std::int16_t>((sequence_ % 200) - 100);
  input.leftY = static_cast<std::int16_t>(100 - (sequence_ % 200));
  input.rightX = static_cast<std::int16_t>((sequence_ % 150) - 75);
  input.rightY = static_cast<std::int16_t>((sequence_ % 90) - 45);
  input.leftButton = (sequence_ % 40) < 20;
  input.rightButton = (sequence_ % 60) < 10;
  sharedState_.setJoystickInput(input);

  SharedState::CommunicationStatus status{};
  sharedState_.getCommunicationStatus(status);
  status.wifiConnected = true;
  status.udpSent = sequence_;
  if (sequence_ % 50 == 0) {
    status.udpErrors += 1;
  }
  sharedState_.setCommunicationStatus(status);
}

Core1Task::Core1Task(const CoreTask::TaskConfig &config, SharedState &sharedState)
    : CoreTask(config), sharedState_(sharedState) {}

void Core1Task::setup() {
  Serial.println("[Core1] Telemetry task started");
}

void Core1Task::loop() {
  const std::uint32_t now = millis();
  SharedState::JoystickInput input;
  if (sharedState_.getJoystickInput(input)) {
    const bool updated = (!hasLogged_) || (input.sequence != lastLoggedSequence_);
    const bool due = (!hasLogged_) || (now - lastLogMs_ >= 1000);
    if (updated && due) {
      Serial.printf("[Core1] seq=%lu left(%d,%d) right(%d,%d) buttons L:%d R:%d\n",
                    static_cast<unsigned long>(input.sequence),
                    input.leftX,
                    input.leftY,
                    input.rightX,
                    input.rightY,
                    input.leftButton,
                    input.rightButton);
      lastLoggedSequence_ = input.sequence;
      lastLogMs_ = now;
      hasLogged_ = true;
    }
  }

  SharedState::CommunicationStatus status{};
  if (sharedState_.getCommunicationStatus(status)) {
    if (now - lastCommLogMs_ >= 2000) {
      Serial.printf("[Core1] comm wifi=%s sent=%lu errors=%lu\n",
                    status.wifiConnected ? "ON" : "OFF",
                    static_cast<unsigned long>(status.udpSent),
                    static_cast<unsigned long>(status.udpErrors));
      lastCommLogMs_ = now;
    }
  }
}

bool Core0Task::ensureWifiAp() {
  if (wifiInitialized_) {
    return true;
  }
  if (!configLoaded_) {
    return false;
  }
  const auto &wifi = configManager_.config().wifi;
  if (!wifi.enabled || (wifi.mode != "ap" && wifi.mode != "sta_ap")) {
    wifiInitialized_ = true;
    SharedState::CommunicationStatus status{};
    if (sharedState_.getCommunicationStatus(status)) {
      status.wifiConnected = false;
    }
    sharedState_.setCommunicationStatus(status);
    Serial.println("[Core0] WiFi AP disabled or mode not ap/sta_ap; skipping AP start");
    return true;
  }
  if (startWifiAp(configManager_.config())) {
    wifiInitialized_ = true;
    SharedState::CommunicationStatus status{};
    if (sharedState_.getCommunicationStatus(status)) {
      status.wifiConnected = true;
      sharedState_.setCommunicationStatus(status);
    } else {
      status.wifiConnected = true;
      sharedState_.setCommunicationStatus(status);
    }
    Serial.println("[Core0] WiFi AP initialized");
    return true;
  }
  const uint32_t now = millis();
  if (now - lastWifiLogMs_ > 2000) {
    Serial.println("[Core0] WiFi AP initialization failed, will retry");
    lastWifiLogMs_ = now;
  }
  return false;
}

bool Core0Task::startWifiAp(const ConfigManager::Config &config) {
  const auto &wifi = config.wifi;
  if (!wifi.enabled) {
    Serial.println("[Core0] WiFi AP disabled via config");
    return false;
  }
  if (wifi.mode != "ap" && wifi.mode != "sta_ap") {
    Serial.printf("[Core0] WiFi mode %s not starting AP\n", wifi.mode.c_str());
    return false;
  }
  if (!configureSoftAp(wifi)) {
    return false;
  }
  const auto &ap = wifi.ap;
  const std::string &ssid = ap.ssid.empty() ? wifi.ssid : ap.ssid;
  const char *password = ap.password.empty() ? nullptr : ap.password.c_str();
  return WiFi.softAP(ssid.c_str(), password, ap.channel, ap.hidden, ap.maxConnections);
}

bool Core0Task::configureSoftAp(const ConfigManager::WifiConfig &apConfig) {
  WiFi.mode(WIFI_AP);

  IPAddress localIp(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  parseIp(apConfig.ap.localIp, localIp);
  parseIp(apConfig.ap.gateway, gateway);
  parseIp(apConfig.ap.subnet, subnet);

  if (!WiFi.softAPConfig(localIp, gateway, subnet)) {
    Serial.println("[Core0] softAPConfig failed");
    return false;
  }
  return true;
}

bool Core0Task::parseIp(const std::string &text, IPAddress &out) {
  if (text.empty()) {
    return false;
  }
  IPAddress parsed;
  if (!parsed.fromString(text.c_str())) {
    return false;
  }
  out = parsed;
  return true;
}
