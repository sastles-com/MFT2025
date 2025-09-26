#include "config/ConfigManager.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <cstring>
#include <utility>

namespace {
constexpr std::size_t kJsonCapacity = 4096;
}

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

  config_.system.name = doc["system"]["name"].as<const char *>() ? doc["system"]["name"].as<const char *>() : "";
  config_.system.psramEnabled = doc["system"]["PSRAM"].as<bool>();
  config_.system.debug = doc["system"]["debug"].as<bool>();

  const JsonVariantConst audio = doc["buzzer"];
  config_.buzzer.enabled = audio["enabled"].as<bool>();
  config_.buzzer.volume = static_cast<std::uint8_t>(audio["volume"].as<std::uint32_t>());

  const JsonVariantConst display = doc["display"];
  config_.display.width = static_cast<std::uint16_t>(display["width"].as<std::uint16_t>());
  config_.display.height = static_cast<std::uint16_t>(display["height"].as<std::uint16_t>());
  config_.display.rotation = static_cast<std::int8_t>(display["rotation"].as<std::int32_t>());
  config_.display.displaySwitch = display["switch"].as<bool>();
  config_.display.colorDepth = static_cast<std::uint8_t>(display["color_depth"].as<std::uint32_t>());
  config_.display.offsetX = static_cast<std::int16_t>(display["offset"][0].as<int>());
  config_.display.offsetY = static_cast<std::int16_t>(display["offset"][1].as<int>());

  const JsonVariantConst wifi = doc["wifi"];
  config_.wifi.ssid = wifi["ssid"].as<const char *>() ? wifi["ssid"].as<const char *>() : "";
  config_.wifi.password = wifi["password"].as<const char *>() ? wifi["password"].as<const char *>() : "";
  config_.wifi.maxRetries = static_cast<std::uint8_t>(wifi["max_retries"].as<std::uint32_t>());

  const JsonVariantConst mqtt = doc["mqtt"];
  config_.mqtt.enabled = mqtt["enabled"].as<bool>();
  config_.mqtt.broker = mqtt["broker"].as<const char *>() ? mqtt["broker"].as<const char *>() : "";
  config_.mqtt.port = static_cast<std::uint16_t>(mqtt["port"].as<std::uint32_t>());
  config_.mqtt.topicUi = mqtt["topic"]["ui"].as<const char *>() ? mqtt["topic"]["ui"].as<const char *>() : "";
  config_.mqtt.topicStatus = mqtt["topic"]["status"].as<const char *>() ? mqtt["topic"]["status"].as<const char *>() : "";
  config_.mqtt.topicImage = mqtt["topic"]["image"].as<const char *>() ? mqtt["topic"]["image"].as<const char *>() : "";

  const JsonVariantConst imu = doc["imu"];
  if (!imu.isNull()) {
    config_.imu.enabled = imu["enabled"].as<bool>();
    config_.imu.gestureUiMode = imu["gesture_ui_mode"].as<bool>();
    config_.imu.gestureDebugLog = imu["gesture_debug_log"].as<bool>();
    config_.imu.gestureThresholdMps2 = imu["gesture_threshold_mps2"].as<float>();
    config_.imu.gestureWindowMs = imu["gesture_window_ms"].as<std::uint32_t>();
    config_.imu.updateIntervalMs = imu["update_interval_ms"].as<std::uint32_t>();
    config_.imu.uiShakeTriggerCount = imu["ui_shake_trigger_count"].isNull()
                                         ? 3
                                         : static_cast<std::uint8_t>(imu["ui_shake_trigger_count"].as<std::uint32_t>());
    config_.imu.uiShakeWindowMs = imu["ui_shake_window_ms"].isNull()
                                     ? 900
                                     : imu["ui_shake_window_ms"].as<std::uint32_t>();
    if (config_.imu.updateIntervalMs == 0) {
      config_.imu.updateIntervalMs = 33;
    }
  } else {
    config_.imu = ConfigManager::ImuConfig{};
  }

  const JsonVariantConst ota = doc["ota"];
  if (!ota.isNull()) {
    config_.ota.enabled = ota["enabled"].as<bool>();
    config_.ota.username = ota["username"].as<const char *>() ? ota["username"].as<const char *>() : "";
    config_.ota.password = ota["password"].as<const char *>() ? ota["password"].as<const char *>() : "";
  } else {
    config_.ota = ConfigManager::OtaConfig{};
  }

  const JsonVariantConst ui = doc["ui"];
  if (!ui.isNull()) {
    config_.ui.gestureEnabled = ui["gesture_enabled"].isNull() ? true : ui["gesture_enabled"].as<bool>();
    config_.ui.dimOnEntry = ui["dim_on_entry"].isNull() ? true : ui["dim_on_entry"].as<bool>();
    const char *overlay = ui["overlay_mode"].as<const char *>();
    if (overlay != nullptr) {
      if (strcasecmp(overlay, "black") == 0 || strcasecmp(overlay, "blackout") == 0) {
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
