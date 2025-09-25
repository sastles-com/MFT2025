#include "imu/ImuService.h"

#include <Arduino.h>
#include <Wire.h>
#include <cmath>
#if defined(IMU_SENSOR_BMI270)
#include <M5Unified.h>
#include <MadgwickAHRS.h>
#else
#include <Adafruit_BNO055.h>
#endif

extern TwoWire Wire1;

ImuService::ImuService(Hooks hooks) : hooks_(std::move(hooks)) {}

ImuService::~ImuService() = default;

bool ImuService::begin() {
  if (initialized_) {
    return true;
  }
  ensureDefaultHooks();
  if (!hooks_.begin) {
    return false;
  }
  initialized_ = hooks_.begin();
#if defined(IMU_SENSOR_BMI270)
  if (initialized_ && usingDefaultHooks_ && !offsetLoadedFromNvs_) {
    startCalibration(10);
  }
#endif
  return initialized_;
}

bool ImuService::isInitialized() const {
  return initialized_;
}

bool ImuService::read(Reading &out) {
  ensureDefaultHooks();
  if (!initialized_ || !hooks_.read) {
    return false;
  }
  return hooks_.read(out);
}

#ifdef UNIT_TEST
void ImuService::setHooksForTest(Hooks hooks) {
  hooks_ = std::move(hooks);
  usingDefaultHooks_ = false;
  initialized_ = false;
}
#endif

void ImuService::markWireInitialized() {
  wireInitialized_ = true;
}

void ImuService::ensureDefaultHooks() {
  if (hooks_.begin && hooks_.read) {
    return;
  }
#if defined(IMU_SENSOR_BMI270)
  if (!filter_) {
    filter_.reset(new Madgwick());
  }
  usingDefaultHooks_ = true;
  if (!hooks_.begin) {
    hooks_.begin = [this]() {
      if (!M5.Imu.isEnabled() && !M5.Imu.begin(&M5.In_I2C, M5.getBoard())) {
        return false;
      }
      offsetLoadedFromNvs_ = M5.Imu.loadOffsetFromNVS();
      if (!offsetLoadedFromNvs_) {
        Serial.println("[IMU] No calibration data in NVS - using defaults");
      }
      filter_->begin(100.0f);
      auto data = M5.Imu.getImuData();
      lastUpdateUs_ = data.usec ? data.usec : micros();
      return true;
    };
  }
  if (!hooks_.read) {
    hooks_.read = [this](Reading &out) {
      if (M5.Imu.update() == m5::IMU_Class::sensor_mask_none) {
        processCalibrationTick();
        return false;
      }

      auto data = M5.Imu.getImuData();
      const float ax = data.accel.x;
      const float ay = data.accel.y;
      const float az = data.accel.z;
      const float gx = data.gyro.x;
      const float gy = data.gyro.y;
      const float gz = data.gyro.z;
      const float mx = data.mag.x;
      const float my = data.mag.y;
      const float mz = data.mag.z;

      if (std::isnan(ax) || std::isnan(gx)) {
        return false;
      }

      const uint32_t nowUs = data.usec ? data.usec : micros();
      float dt = (nowUs - lastUpdateUs_) / 1000000.0f;
      if (dt <= 0.0f) {
        dt = 0.001f;
      }
      lastUpdateUs_ = nowUs;
      filter_->begin(1.0f / dt);

      filter_->update(gx, gy, gz, ax, ay, az, mx, my, mz);

      const float roll = filter_->getRollRadians();
      const float pitch = filter_->getPitchRadians();
      const float yaw = filter_->getYawRadians();

      const float cy = cosf(yaw * 0.5f);
      const float sy = sinf(yaw * 0.5f);
      const float cp = cosf(pitch * 0.5f);
      const float sp = sinf(pitch * 0.5f);
      const float cr = cosf(roll * 0.5f);
      const float sr = sinf(roll * 0.5f);

      out.qw = cr * cp * cy + sr * sp * sy;
      out.qx = sr * cp * cy - cr * sp * sy;
      out.qy = cr * sp * cy + sr * cp * sy;
      out.qz = cr * cp * sy - sr * sp * cy;
      out.timestampMs = millis();
      processCalibrationTick();
      return true;
    };
  }
#else
  if (!wire_) {
    wire_ = &Wire1;
  }
  if (!bno_) {
    bno_.reset(new Adafruit_BNO055(-1, BNO055_ADDRESS_A, wire_));
  }
  usingDefaultHooks_ = true;
  if (!hooks_.begin) {
    hooks_.begin = [this]() {
      if (!wireInitialized_) {
        wire_->begin(2, 1);
        wire_->setClock(400000);
        wireInitialized_ = true;
      }
      if (!bno_->begin(OPERATION_MODE_NDOF)) {
        return false;
      }
      bno_->setExtCrystalUse(true);
      return true;
    };
  }
  if (!hooks_.read) {
    hooks_.read = [this](Reading &out) {
      imu::Quaternion quat = bno_->getQuat();
      out.qw = quat.w();
      out.qx = quat.x();
      out.qy = quat.y();
      out.qz = quat.z();
      out.timestampMs = millis();
      return true;
    };
  }
#endif
}

void ImuService::requestCalibration(std::uint8_t seconds) {
#if defined(IMU_SENSOR_BMI270)
  if (!usingDefaultHooks_ || !initialized_) {
    return;
  }
  startCalibration(seconds == 0 ? 10 : seconds);
#else
  (void)seconds;
#endif
}

#if defined(IMU_SENSOR_BMI270)
void ImuService::startCalibration(std::uint8_t seconds) {
  if (seconds == 0) {
    seconds = 1;
  }
  calibrationActive_ = true;
  calibrationCountdown_ = seconds;
  calibrationNextTickMs_ = millis() + 1000;
  Serial.printf("[IMU] Calibration started (%u s)\n", static_cast<unsigned>(seconds));
  M5.Imu.setCalibration(kCalibrationStrength_, kCalibrationStrength_, kCalibrationStrength_);
}

void ImuService::processCalibrationTick() {
  if (!calibrationActive_) {
    return;
  }
  const auto now = millis();
  if (now < calibrationNextTickMs_) {
    return;
  }
  calibrationNextTickMs_ = now + 1000;
  if (calibrationCountdown_ > 0) {
    --calibrationCountdown_;
    Serial.printf("[IMU] Calibration countdown: %u\n", static_cast<unsigned>(calibrationCountdown_));
  }
  if (calibrationCountdown_ == 0) {
    M5.Imu.setCalibration(0, 0, kCalibrationStrength_);
    if (M5.Imu.saveOffsetToNVS()) {
      offsetLoadedFromNvs_ = true;
      Serial.println("[IMU] Calibration saved to NVS");
    }
    calibrationActive_ = false;
  }
}
#endif
