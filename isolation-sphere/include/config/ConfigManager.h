#pragma once

#include <cstdint>
#include <functional>
#include <string>

class ConfigManager {
 public:
  struct FsProvider {
    std::function<bool(const char *, std::string &)> readFile;
  };

  struct SystemConfig {
    std::string name;
    bool psramEnabled = false;
    bool debug = false;
  };

  struct DisplayConfig {
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::int8_t rotation = 0;
    std::int16_t offsetX = 0;
    std::int16_t offsetY = 0;
    bool displaySwitch = false;
    std::uint8_t colorDepth = 0;
  };

  struct WifiConfig {
    std::string ssid;
    std::string password;
    std::uint8_t maxRetries = 0;
  };

  struct MqttConfig {
    bool enabled = false;
    std::string broker;
    std::uint16_t port = 0;
    std::string topicUi;
    std::string topicStatus;
    std::string topicImage;
  };

  struct Config {
    SystemConfig system;
    DisplayConfig display;
    WifiConfig wifi;
    MqttConfig mqtt;
  };

  explicit ConfigManager(FsProvider provider = FsProvider{});

  bool load(const char *path = "/config.json");

  bool isLoaded() const;

  const Config &config() const;

  static FsProvider makeLittleFsProvider();

 private:
  FsProvider provider_;
  Config config_;
  bool loaded_ = false;
};
