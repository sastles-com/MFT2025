#pragma once

#include "config/ConfigManager.h"
#include "hardware/HardwareContext.h"

class DisplayController {
 public:
  explicit DisplayController(HardwareContext::DisplayDriver &displayDriver);

  bool initialize(const ConfigManager::DisplayConfig &config);

  bool isEnabled() const;

 private:
  HardwareContext::DisplayDriver &displayDriver_;
  bool enabled_ = false;
};

