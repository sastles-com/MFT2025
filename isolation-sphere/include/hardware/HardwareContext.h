#pragma once

#include <cstdint>

class HardwareContext {
 public:
  class DisplayDriver {
   public:
    virtual ~DisplayDriver() = default;
    virtual bool begin() = 0;
    virtual void setRotation(std::int8_t rotation) = 0;
    virtual void setBrightness(std::uint8_t brightness) = 0;
    virtual void fillScreen(std::uint16_t color) = 0;
  };

  virtual ~HardwareContext() = default;

  virtual DisplayDriver &display() = 0;
};
