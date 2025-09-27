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

  struct BuzzerConfig {
    bool enabled = false;
    std::uint8_t volume = 0;
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
    bool enabled = true;
    std::string mode = "ap";
    bool visible = true;
    std::string ssid;
    std::string password;
    std::uint8_t maxRetries = 0;
    
    struct ApConfig {
      std::string ssid;
      std::string password;
      std::string localIp = "192.168.100.1";
      std::string gateway = "192.168.100.1";
      std::string subnet = "255.255.255.0";
      std::uint8_t channel = 6;
      bool hidden = false;
      std::uint8_t maxConnections = 8;
    } ap;
  };

  struct MqttConfig {
    bool enabled = false;
    std::string broker;
    std::uint16_t port = 0;
    std::string topicUi;
    std::string topicStatus;
    std::string topicImage;
  };

  struct ImuConfig {
    bool enabled = false;
    bool gestureUiMode = false;
    bool gestureDebugLog = false;
    float gestureThresholdMps2 = 0.0f;
    std::uint32_t gestureWindowMs = 0;
    std::uint32_t updateIntervalMs = 33;
    std::uint8_t uiShakeTriggerCount = 3;
    std::uint32_t uiShakeWindowMs = 900;
  };

  struct OtaConfig {
    bool enabled = false;
    std::string username;
    std::string password;
  };

  struct UiConfig {
    bool gestureEnabled = true;
    bool dimOnEntry = true;
    enum class OverlayMode : std::uint8_t {
      kOverlay,
      kBlackout,
    } overlayMode = OverlayMode::kOverlay;
  };

  struct Config {
    SystemConfig system;
    DisplayConfig display;
    WifiConfig wifi;
    MqttConfig mqtt;
    BuzzerConfig buzzer;
    ImuConfig imu;
    OtaConfig ota;
    UiConfig ui;
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
