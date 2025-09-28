#include "config/ConfigManager.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <cstring>
#include <utility>

namespace {
constexpr std::size_t kJsonCapacity = 8192;  // 4096から8192に増加

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

  Serial.printf("[Config] Loading config from %s, size: %zu bytes\n", path, raw.length());
  
  DynamicJsonDocument doc(kJsonCapacity);
  auto error = deserializeJson(doc, raw);
  if (error) {
    Serial.printf("[Config] JSON parse error: %s\n", error.c_str());
    loaded_ = false;
    return false;
  }
  
  Serial.println("[Config] JSON parsed successfully");

  const JsonVariantConst system = doc["system"];
  config_.system.name = safeString(system["name"]);
  config_.system.psramEnabled = safeBool(system["PSRAM"], config_.system.psramEnabled);
  config_.system.debug = safeBool(system["debug"], config_.system.debug);

  const JsonVariantConst sphere = doc["sphere"];
  
  // Sphere instances configuration
  if (!sphere.isNull()) {
    const JsonArrayConst instances = sphere["instances"].as<JsonArrayConst>();
    if (!instances.isNull()) {
      config_.sphere.instances.clear();
      for (JsonVariantConst instance : instances) {
        ConfigManager::SphereConfig::InstanceConfig instConfig;
        instConfig.id = safeString(instance["id"]);
        instConfig.mac = safeString(instance["mac"]);
        instConfig.staticIp = safeString(instance["static_ip"]);
        instConfig.mqttPrefix = safeString(instance["mqtt_prefix"]);
        instConfig.friendlyName = safeString(instance["friendly_name"]);
        instConfig.notes = safeString(instance["notes"]);
        
        const JsonVariantConst features = instance["features"];
        if (!features.isNull()) {
          instConfig.features.led = safeBool(features["led"], instConfig.features.led);
          instConfig.features.imu = safeBool(features["imu"], instConfig.features.imu);
          instConfig.features.ui = safeBool(features["ui"], instConfig.features.ui);
        }
        config_.sphere.instances.push_back(instConfig);
      }
    }
  }
  
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
  config_.wifi.enabled = safeBool(wifi["enabled"], config_.wifi.enabled);
  config_.wifi.mode = safeString(wifi["mode"]);
  config_.wifi.visible = safeBool(wifi["visible"], config_.wifi.visible);
  config_.wifi.ssid = safeString(wifi["ssid"]);
  config_.wifi.password = safeString(wifi["password"]);
  config_.wifi.maxRetries = safeUint8(wifi["max_retries"], config_.wifi.maxRetries);
  
  const JsonVariantConst wifiAp = wifi["ap"];
  if (!wifiAp.isNull()) {
    config_.wifi.ap.ssid = safeString(wifiAp["ssid"]);
    config_.wifi.ap.password = safeString(wifiAp["password"]);
    config_.wifi.ap.localIp = safeString(wifiAp["local_ip"]);
    config_.wifi.ap.gateway = safeString(wifiAp["gateway"]);
    config_.wifi.ap.subnet = safeString(wifiAp["subnet"]);
    config_.wifi.ap.channel = safeUint8(wifiAp["channel"], config_.wifi.ap.channel);
    config_.wifi.ap.hidden = safeBool(wifiAp["hidden"], config_.wifi.ap.hidden);
    config_.wifi.ap.maxConnections = safeUint8(wifiAp["max_connections"], config_.wifi.ap.maxConnections);
  }

  const JsonVariantConst mqtt = doc["mqtt"];
  config_.mqtt.enabled = safeBool(mqtt["enabled"], config_.mqtt.enabled);
  config_.mqtt.broker = safeString(mqtt["broker"]);
  config_.mqtt.port = safeUint16(mqtt["port"], config_.mqtt.port);
  config_.mqtt.username = safeString(mqtt["username"]);
  config_.mqtt.password = safeString(mqtt["password"]);
  config_.mqtt.keepAlive = safeUint16(mqtt["keep_alive"], config_.mqtt.keepAlive);
  
  // Backward compatibility topics
  config_.mqtt.topicUi = safeString(mqtt["topic"]["ui"]);
  config_.mqtt.topicImage = safeString(mqtt["topic"]["image"]);
  config_.mqtt.topicCommand = safeString(mqtt["topic"]["command"]);
  
  // Individual device topics
  config_.mqtt.topicUiIndividual = safeString(mqtt["topic"]["ui_individual"]);
  config_.mqtt.topicImageIndividual = safeString(mqtt["topic"]["image_individual"]);
  config_.mqtt.topicCommandIndividual = safeString(mqtt["topic"]["command_individual"]);
  config_.mqtt.topicStatus = safeString(mqtt["topic"]["status"]);
  config_.mqtt.topicInput = safeString(mqtt["topic"]["input"]);
  
  // Broadcast topics
  config_.mqtt.topicUiAll = safeString(mqtt["topic"]["ui_all"]);
  config_.mqtt.topicImageAll = safeString(mqtt["topic"]["image_all"]);
  config_.mqtt.topicCommandAll = safeString(mqtt["topic"]["command_all"]);
  config_.mqtt.topicSync = safeString(mqtt["topic"]["sync"]);
  config_.mqtt.topicEmergency = safeString(mqtt["topic"]["emergency"]);

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

  // Joystick configuration
  const JsonVariantConst joystick = doc["joystick"];
  if (!joystick.isNull()) {
    // UDP settings
    const JsonVariantConst udp = joystick["udp"];
    if (!udp.isNull()) {
      config_.joystick.udp.targetIp = safeString(udp["target_ip"]);
      config_.joystick.udp.port = safeUint16(udp["port"], config_.joystick.udp.port);
      config_.joystick.udp.updateIntervalMs = safeUint32(udp["update_interval_ms"], config_.joystick.udp.updateIntervalMs);
      config_.joystick.udp.joystickReadIntervalMs = safeUint32(udp["joystick_read_interval_ms"], config_.joystick.udp.joystickReadIntervalMs);
      config_.joystick.udp.maxRetryCount = safeUint32(udp["max_retry_count"], config_.joystick.udp.maxRetryCount);
      config_.joystick.udp.timeoutMs = safeUint32(udp["timeout_ms"], config_.joystick.udp.timeoutMs);
    }
    
    // System settings
    const JsonVariantConst joySystem = joystick["system"];
    if (!joySystem.isNull()) {
      config_.joystick.system.buzzerEnabled = safeBool(joySystem["buzzer_enabled"], config_.joystick.system.buzzerEnabled);
      config_.joystick.system.buzzerVolume = safeUint8(joySystem["buzzer_volume"], config_.joystick.system.buzzerVolume);
      config_.joystick.system.openingAnimationEnabled = safeBool(joySystem["opening_animation_enabled"], config_.joystick.system.openingAnimationEnabled);
      config_.joystick.system.lcdBrightness = safeUint8(joySystem["lcd_brightness"], config_.joystick.system.lcdBrightness);
      config_.joystick.system.debugMode = safeBool(joySystem["debug_mode"], config_.joystick.system.debugMode);
      config_.joystick.system.deviceName = safeString(joySystem["device_name"]);
    }
    
    // Input settings
    const JsonVariantConst input = joystick["input"];
    if (!input.isNull()) {
      config_.joystick.input.deadzone = safeFloat(input["deadzone"], config_.joystick.input.deadzone);
      config_.joystick.input.invertLeftY = safeBool(input["invert_left_y"], config_.joystick.input.invertLeftY);
      config_.joystick.input.invertRightY = safeBool(input["invert_right_y"], config_.joystick.input.invertRightY);
      config_.joystick.input.timestampOffsetMs = safeUint32(input["timestamp_offset_ms"], config_.joystick.input.timestampOffsetMs);
      config_.joystick.input.sensitivityProfile = safeString(input["sensitivity_profile"]);
    }
    
    // UI settings
    const JsonVariantConst joyUi = joystick["ui"];
    if (!joyUi.isNull()) {
      config_.joystick.ui.useDualDial = safeBool(joyUi["use_dual_dial"], config_.joystick.ui.useDualDial);
      config_.joystick.ui.defaultMode = safeString(joyUi["default_mode"]);
      config_.joystick.ui.buttonDebounceMs = safeUint32(joyUi["button_debounce_ms"], config_.joystick.ui.buttonDebounceMs);
      config_.joystick.ui.ledFeedback = safeBool(joyUi["led_feedback"], config_.joystick.ui.ledFeedback);
    }
  } else {
    config_.joystick = ConfigManager::JoystickConfig{};
  }

  loaded_ = true;
  Serial.printf("[Config] Configuration loaded successfully. WiFi enabled: %s, MQTT enabled: %s\n", 
               config_.wifi.enabled ? "true" : "false",
               config_.mqtt.enabled ? "true" : "false");
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
