#include "display/DisplayController.h"

namespace {
constexpr std::uint8_t kDefaultBrightness = 128;
constexpr std::uint16_t kClearColor = 0x0000;
}

DisplayController::DisplayController(HardwareContext::DisplayDriver &displayDriver)
    : displayDriver_(displayDriver) {}

bool DisplayController::initialize(const ConfigManager::DisplayConfig &config) {
  enabled_ = false;

  if (!config.displaySwitch) {
    return true;
  }

  if (!displayDriver_.begin()) {
    return false;
  }

  displayDriver_.setRotation(config.rotation);
  displayDriver_.setBrightness(kDefaultBrightness);
  displayDriver_.fillScreen(kClearColor);

  enabled_ = true;
  return true;
}

bool DisplayController::isEnabled() const {
  return enabled_;
}

