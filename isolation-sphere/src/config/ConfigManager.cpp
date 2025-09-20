#include "config/ConfigManager.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
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
