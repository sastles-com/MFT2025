#include "audio/BuzzerService.h"

#include "esp_log.h"

namespace {
constexpr const char *kTag = "BuzzerService";
}

BuzzerService::BuzzerService() {
  manager_.reset(new buzzer::Manager());
  ensureDefaultHooks();
}

BuzzerService::BuzzerService(Hooks hooks) : hooks_(std::move(hooks)) {
  if (!hooks_.init || !hooks_.playEffect) {
    manager_.reset(new buzzer::Manager());
    ensureDefaultHooks();
  }
}

BuzzerService::~BuzzerService() {
  stop();
  if (manager_) {
    manager_->Deinit();
  }
}

void BuzzerService::ensureDefaultHooks() {
  if (!manager_) {
    return;
  }

  hooks_.init = [this](gpio_num_t gpio) { return manager_->Init(gpio); };
  hooks_.playEffect = [this](buzzer::Effect effect) { return manager_->PlayEffect(effect); };
  hooks_.stop = [this]() { return manager_->Stop(); };
}

bool BuzzerService::begin(gpio_num_t gpio) {
  if (initialized_) {
    return true;
  }
  if (!hooks_.init) {
    ESP_LOGE(kTag, "Init hook not provided");
    return false;
  }
  const auto result = hooks_.init(gpio);
  initialized_ = (result == buzzer::Result::kOk);
  if (!initialized_) {
    ESP_LOGE(kTag, "Buzzer init failed: %s", buzzer::Manager::ResultToString(result));
  }
  return initialized_;
}

bool BuzzerService::playEffect(buzzer::Effect effect) {
  if (!initialized_ || !hooks_.playEffect) {
    return false;
  }
  return hooks_.playEffect(effect) == buzzer::Result::kOk;
}

bool BuzzerService::playStartupTone() {
  return playEffect(buzzer::Effect::kStartup);
}

void BuzzerService::stop() {
  if (!initialized_ || !hooks_.stop) {
    return;
  }
  hooks_.stop();
}
