#include "config/ConfigManager.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <cstring>
#include <utility>

namespace {
constexpr std::size_t kJsonCapacity = 4096;

std::string safeString(const JsonVariantConst &variant) {
  const char *value = variant.as<const char *>();
  return value ? value : "";
}

bool safeBool(const JsonVariantConst &variant, bool fallback = false) {
  if (variant.isNull()) {
    return fallback;
  }
  return variant.as<bool>();
}

std::uint8_t safeUint8(const JsonVariantConst &variant, std::uint8_t fallback = 0) {
  if (variant.isNull()) {
    return fallback;
  }
  return static_cast<std::uint8_t>(variant.as<std::uint32_t>());
}

std::uint16_t safeUint16(const JsonVariantConst &variant, std::uint16_t fallback = 0) {
  if (variant.isNull()) {
    return fallback;
  }
  return static_cast<std::uint16_t>(variant.as<std::uint32_t>());
}

std::uint32_t safeUint32(const JsonVariantConst &variant, std::uint32_t fallback = 0) {
  if (variant.isNull()) {
    return fallback;
  }
  return variant.as<std::uint32_t>();
}

float safeFloat(const JsonVariantConst &variant, float fallback = 0.0f) {
  if (variant.isNull()) {
    return fallback;
  }
  return variant.as<float>();
}

std::int32_t safeInt32(const JsonVariantConst &variant, std::int32_t fallback = 0) {
  if (variant.isNull()) {
    return fallback;
  }
  return variant.as<std::int32_t>();
}

JsonVariantConst getObjectMember(const JsonVariantConst &object, const char *key) {
  if (!object.is<JsonObjectConst>()) {
    return JsonVariantConst();
  }
  return object[key];
}
}  // namespace

ConfigManager::ConfigManager(FsProvider provider) : provider_(std::move(provider)) {
  if (!provider_.readFile) {
    provider_ = makeLittleFsProvider();
  }
}

bool ConfigManager::load(const char *path) {
  if (!provider_.readFile) {
    loaded_ = false;
    return false;
  }

  std::string raw;
  if (!provider_.readFile(path, raw)) {
    loaded_ = false;
    return false;
  }

  DynamicJsonDocument doc(kJsonCapacity);
  auto error = deserializeJson(doc, raw);
  if (error) {
    loaded_ = false;
    return false;
  }

  const JsonVariantConst system = doc["system"];
  config_.system.name = safeString(system["name"]);
  config_.system.psramEnabled = safeBool(system["PSRAM"], config_.system.psramEnabled);
  config_.system.debug = safeBool(system["debug"], config_.system.debug);

  const JsonVariantConst sphere = doc["sphere"];
  JsonVariantConst audio = getObjectMember(sphere, "buzzer");
  if (audio.isNull()) {
    audio = doc["buzzer"];
  }
  config_.buzzer.enabled = safeBool(audio["enabled"], config_.buzzer.enabled);
  config_.buzzer.volume = safeUint8(audio["volume"], config_.buzzer.volume);

  JsonVariantConst display = getObjectMember(sphere, "display");
  if (display.isNull()) {
    display = doc["display"];
  }
  if (!display.isNull()) {
    config_.display.width = safeUint16(display["width"], config_.display.width);
    config_.display.height = safeUint16(display["height"], config_.display.height);
    config_.display.rotation = static_cast<std::int8_t>(safeInt32(display["rotation"], config_.display.rotation));
    config_.display.displaySwitch = safeBool(display["switch"], config_.display.displaySwitch);
    config_.display.colorDepth = safeUint8(display["color_depth"], config_.display.colorDepth);
    const JsonArrayConst offset = display["offset"].as<JsonArrayConst>();
    if (!offset.isNull() && offset.size() >= 2) {
      config_.display.offsetX = static_cast<std::int16_t>(safeInt32(offset[0], config_.display.offsetX));
      config_.display.offsetY = static_cast<std::int16_t>(safeInt32(offset[1], config_.display.offsetY));
    }
  }

  const JsonVariantConst wifi = doc["wifi"];
  config_.wifi.ssid = safeString(wifi["ssid"]);
  config_.wifi.password = safeString(wifi["password"]);
  config_.wifi.maxRetries = safeUint8(wifi["max_retries"], config_.wifi.maxRetries);

  const JsonVariantConst mqtt = doc["mqtt"];
  config_.mqtt.enabled = safeBool(mqtt["enabled"], config_.mqtt.enabled);
  config_.mqtt.broker = safeString(mqtt["broker"]);
  config_.mqtt.port = safeUint16(mqtt["port"], config_.mqtt.port);
  config_.mqtt.topicUi = safeString(mqtt["topic"]["ui"]);
  config_.mqtt.topicStatus = safeString(mqtt["topic"]["status"]);
  config_.mqtt.topicImage = safeString(mqtt["topic"]["image"]);

  JsonVariantConst imuContainer = getObjectMember(sphere, "imu");
  if (imuContainer.isNull()) {
    imuContainer = doc["imu"];
  }
  if (!imuContainer.isNull()) {
    config_.imu.enabled = safeBool(imuContainer["enabled"], config_.imu.enabled);
    config_.imu.gestureUiMode = safeBool(imuContainer["gesture_ui_mode"], config_.imu.gestureUiMode);
    config_.imu.gestureDebugLog = safeBool(imuContainer["gesture_debug_log"], config_.imu.gestureDebugLog);
    config_.imu.gestureThresholdMps2 = safeFloat(imuContainer["gesture_threshold_mps2"], config_.imu.gestureThresholdMps2);
    config_.imu.gestureWindowMs = safeUint32(imuContainer["gesture_window_ms"], config_.imu.gestureWindowMs);
    config_.imu.updateIntervalMs = safeUint32(imuContainer["update_interval_ms"], config_.imu.updateIntervalMs);
    config_.imu.uiShakeTriggerCount = safeUint8(imuContainer["ui_shake_trigger_count"], config_.imu.uiShakeTriggerCount);
    config_.imu.uiShakeWindowMs = safeUint32(imuContainer["ui_shake_window_ms"], config_.imu.uiShakeWindowMs);
    if (config_.imu.updateIntervalMs == 0) {
      config_.imu.updateIntervalMs = 33;
    }
  } else {
    config_.imu = ConfigManager::ImuConfig{};
  }

  const JsonVariantConst ota = doc["ota"];
  if (!ota.isNull()) {
    config_.ota.enabled = safeBool(ota["enabled"], config_.ota.enabled);
    config_.ota.username = safeString(ota["username"]);
    config_.ota.password = safeString(ota["password"]);
  } else {
    config_.ota = ConfigManager::OtaConfig{};
  }

  JsonVariantConst uiContainer = getObjectMember(sphere, "ui");
  if (uiContainer.isNull()) {
    uiContainer = doc["ui"];
  }
  if (!uiContainer.isNull()) {
    config_.ui.gestureEnabled = safeBool(uiContainer["gesture_enabled"], config_.ui.gestureEnabled);
    config_.ui.dimOnEntry = safeBool(uiContainer["dim_on_entry"], config_.ui.dimOnEntry);
    const std::string overlay = safeString(uiContainer["overlay_mode"]);
    if (!overlay.empty()) {
      if (strcasecmp(overlay.c_str(), "black") == 0 || strcasecmp(overlay.c_str(), "blackout") == 0) {
        config_.ui.overlayMode = ConfigManager::UiConfig::OverlayMode::kBlackout;
      } else {
        config_.ui.overlayMode = ConfigManager::UiConfig::OverlayMode::kOverlay;
      }
    } else {
      config_.ui.overlayMode = ConfigManager::UiConfig::OverlayMode::kOverlay;
    }
  } else {
    config_.ui = ConfigManager::UiConfig{};
  }

  loaded_ = true;
  return true;
}

bool ConfigManager::isLoaded() const {
  return loaded_;
}

const ConfigManager::Config &ConfigManager::config() const {
  return config_;
}

ConfigManager::FsProvider ConfigManager::makeLittleFsProvider() {
  FsProvider provider;
  provider.readFile = [](const char *path, std::string &out) {
    if (!path) {
      return false;
    }
    File file = LittleFS.open(path, FILE_READ);
    if (!file) {
      return false;
    }
    out.clear();
    while (file.available()) {
      int c = file.read();
      if (c < 0) {
        break;
      }
      out.push_back(static_cast<char>(c));
    }
    file.close();
    return true;
  };
  return provider;
}
