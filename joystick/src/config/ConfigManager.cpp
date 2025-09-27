#include "config/ConfigManager.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

namespace {
constexpr std::size_t kJsonCapacity = 6144;

std::string safeString(const JsonVariantConst &variant) {
  const char *value = variant.as<const char *>();
  return value ? value : "";
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

bool safeBool(const JsonVariantConst &variant, bool fallback = false) {
  if (variant.isNull()) {
    return fallback;
  }
  return variant.as<bool>();
}

std::int32_t safeInt32(const JsonVariantConst &variant, std::int32_t fallback = 0) {
  if (variant.isNull()) {
    return fallback;
  }
  return variant.as<std::int32_t>();
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
  config_.system.psramEnabled = safeBool(system["PSRAM"]);
  config_.system.debug = safeBool(system["debug"]);

  const JsonVariantConst wifi = doc["wifi"];
  config_.wifi.enabled = safeBool(wifi["enabled"], config_.wifi.enabled);
  if (!wifi["mode"].isNull()) {
    config_.wifi.mode = safeString(wifi["mode"]);
  }
  config_.wifi.ssid = safeString(wifi["ssid"]);
  config_.wifi.password = safeString(wifi["password"]);
  config_.wifi.maxRetries = safeUint8(wifi["max_retries"], config_.wifi.maxRetries);

  const JsonVariantConst wifiAp = wifi["ap"];
  if (!wifiAp.isNull()) {
    if (!wifiAp["ssid"].isNull()) {
      config_.wifi.ap.ssid = safeString(wifiAp["ssid"]);
    } else {
      config_.wifi.ap.ssid = config_.wifi.ssid;
    }
    if (!wifiAp["password"].isNull()) {
      config_.wifi.ap.password = safeString(wifiAp["password"]);
    } else {
      config_.wifi.ap.password = config_.wifi.password;
    }
    if (!wifiAp["local_ip"].isNull()) {
      config_.wifi.ap.localIp = safeString(wifiAp["local_ip"]);
    }
    if (!wifiAp["gateway"].isNull()) {
      config_.wifi.ap.gateway = safeString(wifiAp["gateway"]);
    }
    if (!wifiAp["subnet"].isNull()) {
      config_.wifi.ap.subnet = safeString(wifiAp["subnet"]);
    }
    config_.wifi.ap.channel = safeUint8(wifiAp["channel"], config_.wifi.ap.channel);
    config_.wifi.ap.hidden = safeBool(wifiAp["hidden"], config_.wifi.ap.hidden);
    config_.wifi.ap.maxConnections = safeUint8(wifiAp["max_connections"], config_.wifi.ap.maxConnections);
  } else {
    config_.wifi.ap.ssid = config_.wifi.ssid;
    config_.wifi.ap.password = config_.wifi.password;
  }

  const JsonVariantConst mqtt = doc["mqtt"];
  config_.mqtt.enabled = safeBool(mqtt["enabled"], config_.mqtt.enabled);
  config_.mqtt.broker = safeString(mqtt["broker"]);
  config_.mqtt.port = safeUint16(mqtt["port"], config_.mqtt.port);
  config_.mqtt.topicUi = safeString(mqtt["topic"]["ui"]);
  config_.mqtt.topicStatus = safeString(mqtt["topic"]["status"]);
  config_.mqtt.topicImage = safeString(mqtt["topic"]["image"]);

  const JsonVariantConst buzzer = doc["buzzer"];
  config_.buzzer.enabled = safeBool(buzzer["enabled"], config_.buzzer.enabled);
  config_.buzzer.volume = safeUint8(buzzer["volume"], config_.buzzer.volume);

  const JsonVariantConst ota = doc["ota"];
  config_.ota.enabled = safeBool(ota["enabled"], config_.ota.enabled);
  config_.ota.username = safeString(ota["username"]);
  config_.ota.password = safeString(ota["password"]);

  const JsonVariantConst ui = doc["ui"];
  config_.ui.gestureEnabled = safeBool(ui["gesture_enabled"], config_.ui.gestureEnabled);
  config_.ui.dimOnEntry = safeBool(ui["dim_on_entry"], config_.ui.dimOnEntry);
  if (!ui["overlay_mode"].isNull()) {
    config_.ui.overlayMode = safeString(ui["overlay_mode"]);
  }

  const JsonVariantConst joystick = doc["joystick"];
  if (!joystick.isNull()) {
    const JsonVariantConst joyUdp = joystick["udp"];
    if (!joyUdp.isNull()) {
      if (!joyUdp["target_ip"].isNull()) {
        config_.joystick.udp.targetIp = safeString(joyUdp["target_ip"]);
      }
      config_.joystick.udp.port = safeUint16(joyUdp["port"], config_.joystick.udp.port);
      config_.joystick.udp.updateIntervalMs = safeUint32(joyUdp["update_interval_ms"], config_.joystick.udp.updateIntervalMs);
      config_.joystick.udp.joystickReadIntervalMs = safeUint32(joyUdp["joystick_read_interval_ms"], config_.joystick.udp.joystickReadIntervalMs);
      config_.joystick.udp.maxRetryCount = safeUint8(joyUdp["max_retry_count"], config_.joystick.udp.maxRetryCount);
      config_.joystick.udp.timeoutMs = safeUint32(joyUdp["timeout_ms"], config_.joystick.udp.timeoutMs);
    }

    const JsonVariantConst joySystem = joystick["system"];
    if (!joySystem.isNull()) {
      config_.joystick.system.buzzerEnabled = safeBool(joySystem["buzzer_enabled"], config_.joystick.system.buzzerEnabled);
      config_.joystick.system.buzzerVolume = safeUint8(joySystem["buzzer_volume"], config_.joystick.system.buzzerVolume);
      config_.joystick.system.openingAnimationEnabled = safeBool(joySystem["opening_animation_enabled"], config_.joystick.system.openingAnimationEnabled);
      config_.joystick.system.lcdBrightness = safeUint8(joySystem["lcd_brightness"], config_.joystick.system.lcdBrightness);
      config_.joystick.system.debugMode = safeBool(joySystem["debug_mode"], config_.joystick.system.debugMode);
      if (!joySystem["device_name"].isNull()) {
        config_.joystick.system.deviceName = safeString(joySystem["device_name"]);
      }
    }

    const JsonVariantConst joyInput = joystick["input"];
    if (!joyInput.isNull()) {
      config_.joystick.input.deadzone = safeFloat(joyInput["deadzone"], config_.joystick.input.deadzone);
      config_.joystick.input.invertLeftY = safeBool(joyInput["invert_left_y"], config_.joystick.input.invertLeftY);
      config_.joystick.input.invertRightY = safeBool(joyInput["invert_right_y"], config_.joystick.input.invertRightY);
      config_.joystick.input.timestampOffsetMs = safeInt32(joyInput["timestamp_offset_ms"], config_.joystick.input.timestampOffsetMs);
    }

    const JsonVariantConst joyUi = joystick["ui"];
    if (!joyUi.isNull()) {
      config_.joystick.ui.useDualDial = safeBool(joyUi["use_dual_dial"], config_.joystick.ui.useDualDial);
      if (!joyUi["default_mode"].isNull()) {
        config_.joystick.ui.defaultMode = safeString(joyUi["default_mode"]);
      }
      config_.joystick.ui.buttonDebounceMs = safeUint32(joyUi["button_debounce_ms"], config_.joystick.ui.buttonDebounceMs);
    }
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
