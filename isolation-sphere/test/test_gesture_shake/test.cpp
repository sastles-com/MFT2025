#ifdef ARDUINO
void setup() {}
void loop() {}
#endif
#include <Arduino.h>
#include <unity.h>
#include "imu/ShakeDetector.h"


void test_shake_simple() {
    // threshold, triggerCount, windowMs, refractoryMs, cooldownMs
    ShakeDetector detector(2.0f, 2, 1000, 2000, 1000);
    uint32_t t = 1000;
    // 通常時
    TEST_ASSERT_FALSE(detector.update(0.0f, 0.0f, 9.8f, t));
    TEST_ASSERT_FALSE(detector.update(0.1f, 0.2f, 9.7f, t+100));
    // 1回目のshake
    TEST_ASSERT_FALSE(detector.update(5.0f, 0.0f, 9.8f, t+200));
    // 2回目のshake（1秒以内）
    TEST_ASSERT_TRUE(detector.update(-5.0f, 0.0f, 9.8f, t+500));
    // 直後の連続検出は抑制される
    TEST_ASSERT_FALSE(detector.update(5.0f, 0.0f, 9.8f, t+600));
    // 2秒後なら再検出可能
    TEST_ASSERT_FALSE(detector.update(0.0f, 0.0f, 9.8f, t+2500));
    TEST_ASSERT_FALSE(detector.update(5.0f, 0.0f, 9.8f, t+2600));
    TEST_ASSERT_TRUE(detector.update(-5.0f, 0.0f, 9.8f, t+2700));
}


void test_shake_window() {
    ShakeDetector detector(2.0f, 2, 300, 2000, 1000);
    uint32_t t = 2000;
    // 1回目
    TEST_ASSERT_FALSE(detector.update(5.0f, 0.0f, 9.8f, t));
    // 2回目（ウィンドウ外）
    TEST_ASSERT_FALSE(detector.update(-5.0f, 0.0f, 9.8f, t+400));
    // 2回目（ウィンドウ内）
    TEST_ASSERT_TRUE(detector.update(-5.0f, 0.0f, 9.8f, t+200));
}

