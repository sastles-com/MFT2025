#pragma once

#include <cstdint>
#include <functional>
#include <string>

class ConfigManager {
 public:
  struct FsProvider {
    std::function<bool(const char *path, std::string &out)> readFile;
  };

  struct SystemConfig {
    std::string name;
    bool psramEnabled = false;
    bool debug = false;
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

  struct BuzzerConfig {
    bool enabled = false;
    std::uint8_t volume = 0;
  };

  struct OtaConfig {
    bool enabled = false;
    std::string username;
    std::string password;
  };

  struct UiConfig {
    bool gestureEnabled = true;
    bool dimOnEntry = true;
    std::string overlayMode = "overlay";
  };

  struct JoystickUdpConfig {
    std::string targetIp = "192.168.100.100";
    std::uint16_t port = 1884;
    std::uint32_t updateIntervalMs = 30;
    std::uint32_t joystickReadIntervalMs = 16;
    std::uint8_t maxRetryCount = 3;
    std::uint32_t timeoutMs = 1000;
  };

  struct JoystickSystemConfig {
    bool buzzerEnabled = true;
    std::uint8_t buzzerVolume = 64;
    bool openingAnimationEnabled = true;
    std::uint8_t lcdBrightness = 200;
    bool debugMode = false;
    std::string deviceName = "AtomJoyStick-01";
  };

  struct JoystickInputConfig {
    float deadzone = 0.05f;
    bool invertLeftY = true;
    bool invertRightY = false;
    std::int32_t timestampOffsetMs = 0;
  };

  struct JoystickUiConfig {
    bool useDualDial = true;
    std::string defaultMode = "live";
    std::uint32_t buttonDebounceMs = 200;
  };

  struct JoystickConfig {
    JoystickUdpConfig udp;
    JoystickSystemConfig system;
    JoystickInputConfig input;
    JoystickUiConfig ui;
  };

  struct Config {
    SystemConfig system;
    WifiConfig wifi;
    MqttConfig mqtt;
    BuzzerConfig buzzer;
    OtaConfig ota;
    UiConfig ui;
    JoystickConfig joystick;
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
