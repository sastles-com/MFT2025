#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#if defined(IMU_SENSOR_BMI270)
class Madgwick;
#else
class Adafruit_BNO055;
#endif

class ImuService {
 public:
  struct Reading {
    float qw = 0.0f;
    float qx = 0.0f;
    float qy = 0.0f;
    float qz = 0.0f;
    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    float accelMagnitudeMps2 = 0.0f;
    std::uint32_t timestampMs = 0;
  };

  struct Hooks {
    std::function<bool()> begin;
    std::function<bool(Reading &)> read;
  };

  explicit ImuService(Hooks hooks = Hooks{});
  ~ImuService();

  bool begin();
  bool isInitialized() const;
  bool read(Reading &out);

  void requestCalibration(std::uint8_t seconds = 10);

#ifdef UNIT_TEST
  void setHooksForTest(Hooks hooks);
#endif

  void markWireInitialized();

 private:
  void ensureDefaultHooks();

  Hooks hooks_;
  bool initialized_ = false;
  bool usingDefaultHooks_ = false;
  bool wireInitialized_ = false;
  class TwoWire *wire_ = nullptr;
#if defined(IMU_SENSOR_BMI270)
  std::unique_ptr<class Madgwick> filter_;
  uint32_t lastUpdateUs_ = 0;
  bool offsetLoadedFromNvs_ = false;
  bool calibrationActive_ = false;
  std::uint8_t calibrationCountdown_ = 0;
  std::uint32_t calibrationNextTickMs_ = 0;
  static constexpr std::uint8_t kCalibrationStrength_ = 64;
  void startCalibration(std::uint8_t seconds);
  void processCalibrationTick();
#else
  std::unique_ptr<class Adafruit_BNO055> bno_;
#endif
};
