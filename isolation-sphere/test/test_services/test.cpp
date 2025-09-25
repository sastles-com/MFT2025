#include <unity.h>

#include <vector>

#include "audio/BuzzerService.h"
#include "display/DisplayController.h"

namespace {

struct FakeDisplay : public HardwareContext::DisplayDriver {
  bool beginResult = true;
  bool beginCalled = false;
  std::int8_t rotationArg = 0;
  std::uint8_t brightnessArg = 0;
  std::uint16_t fillColor = 0xFFFF;

  bool begin() override {
    beginCalled = true;
    return beginResult;
  }

  void setRotation(std::int8_t rotation) override { rotationArg = rotation; }

  void setBrightness(std::uint8_t brightness) override { brightnessArg = brightness; }

  void fillScreen(std::uint16_t color) override { fillColor = color; }
};

}  // namespace

void test_display_controller_initializes_enabled_display() {
  FakeDisplay fakeDisplay;
  DisplayController controller(fakeDisplay);

  ConfigManager::DisplayConfig cfg;
  cfg.displaySwitch = true;
  cfg.rotation = 1;

  TEST_ASSERT_TRUE(controller.initialize(cfg));
  TEST_ASSERT_TRUE(controller.isEnabled());
  TEST_ASSERT_TRUE(fakeDisplay.beginCalled);
  TEST_ASSERT_EQUAL_INT8(1, fakeDisplay.rotationArg);
  TEST_ASSERT_EQUAL_UINT16(0, fakeDisplay.fillColor);
}

void test_display_controller_skips_when_switch_off() {
  FakeDisplay fakeDisplay;
  DisplayController controller(fakeDisplay);

  ConfigManager::DisplayConfig cfg;
  cfg.displaySwitch = false;
  cfg.rotation = 0;

  TEST_ASSERT_TRUE(controller.initialize(cfg));
  TEST_ASSERT_FALSE(controller.isEnabled());
  TEST_ASSERT_FALSE(fakeDisplay.beginCalled);
}

void test_buzzer_service_hooks_are_invoked() {
  bool initCalled = false;
  bool playCalled = false;

  BuzzerService::Hooks hooks;
  hooks.init = [&](gpio_num_t gpio) {
    initCalled = (gpio == buzzer::kDefaultGpio);
    return buzzer::Result::kOk;
  };
  hooks.playEffect = [&](buzzer::Effect effect) {
    playCalled = (effect == buzzer::Effect::kStartup);
    return buzzer::Result::kOk;
  };
  hooks.stop = []() { return buzzer::Result::kOk; };

  BuzzerService service(hooks);
  TEST_ASSERT_TRUE(service.begin());
  TEST_ASSERT_TRUE(initCalled);
  TEST_ASSERT_TRUE(service.playStartupTone());
  TEST_ASSERT_TRUE(playCalled);
}

void test_buzzer_service_rejects_play_before_init() {
  bool playCalled = false;
  BuzzerService::Hooks hooks;
  hooks.playEffect = [&](buzzer::Effect) {
    playCalled = true;
    return buzzer::Result::kOk;
  };

  BuzzerService service(hooks);
  TEST_ASSERT_FALSE(service.playStartupTone());
  TEST_ASSERT_FALSE(playCalled);
}

void setUp() {}
void tearDown() {}

int runUnityTests() {
  UNITY_BEGIN();
  RUN_TEST(test_display_controller_initializes_enabled_display);
  RUN_TEST(test_display_controller_skips_when_switch_off);
  RUN_TEST(test_buzzer_service_hooks_are_invoked);
  RUN_TEST(test_buzzer_service_rejects_play_before_init);
  return UNITY_END();
}

#include <Arduino.h>

void setup() {
  delay(200);
  runUnityTests();
}

void loop() {}
