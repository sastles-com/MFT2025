#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

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
    std::string username;
    std::string password;
    std::uint16_t keepAlive = 60;
    
    // Backward compatibility topics
    std::string topicUi;
    std::string topicImage;
    std::string topicCommand;
    
    // Individual device topics
    std::string topicUiIndividual;
    std::string topicImageIndividual;
    std::string topicCommandIndividual;
    std::string topicStatus;
    std::string topicInput;
    
    // Broadcast topics
    std::string topicUiAll;
    std::string topicImageAll;
    std::string topicCommandAll;
    std::string topicSync;
    std::string topicEmergency;
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

  struct LedConfig {
    std::uint8_t numStrips = 4;
    std::vector<std::uint16_t> ledsPerStrip;
    std::vector<std::uint8_t> stripGpios;
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

  struct SphereConfig {
    struct InstanceConfig {
      std::string id;
      std::string mac;
      std::string staticIp;
      std::string mqttPrefix;
      std::string friendlyName;
      std::string notes;
      struct Features {
        bool led = true;
        bool imu = true;
        bool ui = true;
      } features;
    };
    std::vector<InstanceConfig> instances;
  };

  struct JoystickConfig {
    struct UdpConfig {
      std::string targetIp = "192.168.100.100";
      std::uint16_t port = 8000;
      std::uint32_t updateIntervalMs = 33;
      std::uint32_t joystickReadIntervalMs = 16;
      std::uint32_t maxRetryCount = 3;
      std::uint32_t timeoutMs = 1000;
    } udp;
    
    struct SystemConfig {
      bool buzzerEnabled = true;
      std::uint8_t buzzerVolume = 50;
      bool openingAnimationEnabled = true;
      std::uint8_t lcdBrightness = 128;
      bool debugMode = false;
      std::string deviceName = "joystick-001";
    } system;
    
    struct InputConfig {
      float deadzone = 0.1f;
      bool invertLeftY = false;
      bool invertRightY = false;
      std::uint32_t timestampOffsetMs = 0;
      std::string sensitivityProfile = "normal";
    } input;
    
    struct UiConfig {
      bool useDualDial = true;
      std::string defaultMode = "sphere_control";
      std::uint32_t buttonDebounceMs = 50;
      bool ledFeedback = true;
    } ui;
    
    struct AudioConfig {
      bool enabled = true;
      std::uint8_t masterVolume = 75;
      
      struct Sounds {
        bool startup = true;
        bool click = true;
        bool error = true;
        bool test = true;
      } sounds;
      
      struct Volumes {
        std::uint8_t startup = 55;
        std::uint8_t click = 40;
        std::uint8_t error = 70;
        std::uint8_t test = 60;
      } volumes;
    } audio;
  };

  struct Config {
    SystemConfig system;
    DisplayConfig display;
    WifiConfig wifi;
    MqttConfig mqtt;
    BuzzerConfig buzzer;
    ImuConfig imu;
    LedConfig led;
    OtaConfig ota;
    UiConfig ui;
    SphereConfig sphere;
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
