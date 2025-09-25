#pragma once

#include "audio/BuzzerManager.h"

#include <functional>
#include <memory>

class BuzzerService {
 public:
  struct Hooks {
    std::function<buzzer::Result(gpio_num_t)> init;
    std::function<buzzer::Result(buzzer::Effect)> playEffect;
    std::function<buzzer::Result()> stop;
  };

  BuzzerService();
  explicit BuzzerService(Hooks hooks);
  ~BuzzerService();

  bool begin(gpio_num_t gpio = buzzer::kDefaultGpio);
  bool playEffect(buzzer::Effect effect);
  bool playStartupTone();
  void stop();

 private:
  void ensureDefaultHooks();

  Hooks hooks_{};
  std::unique_ptr<buzzer::Manager> manager_;
  bool initialized_ = false;
};

