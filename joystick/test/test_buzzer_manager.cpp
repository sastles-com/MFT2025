#include <unity.h>
#include "buzzer/JoystickBuzzer.h"
#include "config/ConfigManager.h"

// テスト用モッククラス
class MockJoystickBuzzer : public JoystickBuzzer {
private:
    bool mockEnabled_;
    uint8_t mockVolume_;
    int lastFrequency_;
    int lastDuration_;
    bool soundPlayed_;

public:
    MockJoystickBuzzer(const ConfigManager::BuzzerConfig& config) 
        : JoystickBuzzer(config), mockEnabled_(config.enabled), mockVolume_(config.volume),
          lastFrequency_(0), lastDuration_(0), soundPlayed_(false) {}
    
    // テスト用のパブリックメソッド
    void mockSetEnabled(bool enabled) { mockEnabled_ = enabled; }
    void mockSetVolume(uint8_t volume) { mockVolume_ = volume; }
    
    // テスト検証用ゲッター
    int getLastFrequency() const { return lastFrequency_; }
    int getLastDuration() const { return lastDuration_; }
    bool wasSoundPlayed() const { return soundPlayed_; }
    
    void resetMockState() { 
        lastFrequency_ = 0; 
        lastDuration_ = 0; 
        soundPlayed_ = false; 
    }

protected:
    // 実際のハードウェア呼び出しをモックに置き換え
    virtual void playCompletionTone(int frequency, int duration) override {
        if (mockEnabled_) {
            lastFrequency_ = frequency;
            lastDuration_ = duration;
            soundPlayed_ = true;
        }
    }
};

MockJoystickBuzzer* testBuzzer = nullptr;

void setUp(void) {
    // 各テスト前に実行される初期化
    ConfigManager::BuzzerConfig config;
    config.enabled = true;
    config.volume = 50;
    
    testBuzzer = new MockJoystickBuzzer(config);
}

void tearDown(void) {
    // 各テスト後にクリーンアップ
    if (testBuzzer) {
        delete testBuzzer;
        testBuzzer = nullptr;
    }
}

// テスト1: 初期化テスト
void test_buzzer_manager_initialization() {
    TEST_ASSERT_NOT_NULL(testBuzzer);
    TEST_ASSERT_TRUE(testBuzzer->isEnabled());
    TEST_ASSERT_EQUAL(50, testBuzzer->getVolume());
}

// テスト2: 起動音テスト
void test_startup_sound() {
    testBuzzer->resetMockState();
    
    testBuzzer->playStartupMelody();
    
    TEST_ASSERT_TRUE(testBuzzer->wasSoundPlayed());
    // 起動音は通常高い周波数で短時間
    TEST_ASSERT_GREATER_THAN(500, testBuzzer->getLastFrequency());
    TEST_ASSERT_GREATER_THAN(0, testBuzzer->getLastDuration());
    TEST_ASSERT_LESS_OR_EQUAL(1000, testBuzzer->getLastDuration());
}

// テスト3: ボタン音テスト  
void test_button_sound() {
    testBuzzer->resetMockState();
    
    testBuzzer->playClickTone();
    
    TEST_ASSERT_TRUE(testBuzzer->wasSoundPlayed());
    // ボタン音は中程度の周波数で短時間
    TEST_ASSERT_GREATER_THAN(200, testBuzzer->getLastFrequency());
    TEST_ASSERT_LESS_OR_EQUAL(2000, testBuzzer->getLastFrequency());
    TEST_ASSERT_GREATER_THAN(0, testBuzzer->getLastDuration());
    TEST_ASSERT_LESS_OR_EQUAL(300, testBuzzer->getLastDuration());
}

// テスト4: エラー音テスト
void test_error_sound() {
    testBuzzer->resetMockState();
    
    testBuzzer->playErrorTone();
    
    TEST_ASSERT_TRUE(testBuzzer->wasSoundPlayed());
    // エラー音は低い周波数で長めの時間
    TEST_ASSERT_GREATER_THAN(100, testBuzzer->getLastFrequency());
    TEST_ASSERT_LESS_OR_EQUAL(500, testBuzzer->getLastFrequency());
    TEST_ASSERT_GREATER_THAN(200, testBuzzer->getLastDuration());
}

// テスト5: 無効化時の動作テスト
void test_disabled_buzzer() {
    testBuzzer->mockSetEnabled(false);
    testBuzzer->resetMockState();
    
    testBuzzer->playStartupMelody();
    testBuzzer->playClickTone();
    testBuzzer->playErrorTone();
    
    TEST_ASSERT_FALSE(testBuzzer->wasSoundPlayed());
    TEST_ASSERT_EQUAL(0, testBuzzer->getLastFrequency());
    TEST_ASSERT_EQUAL(0, testBuzzer->getLastDuration());
}

// テスト6: 音量設定テスト
void test_volume_control() {
    // 音量0（無音）
    testBuzzer->mockSetVolume(0);
    testBuzzer->resetMockState();
    testBuzzer->playClickTone();
    TEST_ASSERT_FALSE(testBuzzer->wasSoundPlayed());
    
    // 音量最大
    testBuzzer->mockSetVolume(100);
    testBuzzer->resetMockState();
    testBuzzer->playClickTone();
    TEST_ASSERT_TRUE(testBuzzer->wasSoundPlayed());
}

// テスト7: カスタム音程・時間テスト
void test_custom_tone() {
    testBuzzer->resetMockState();
    
    int testFreq = 880; // A5音程
    int testDuration = 250;
    
    testBuzzer->playTone(testFreq, testDuration);
    
    TEST_ASSERT_TRUE(testBuzzer->wasSoundPlayed());
    TEST_ASSERT_EQUAL(testFreq, testBuzzer->getLastFrequency());
    TEST_ASSERT_EQUAL(testDuration, testBuzzer->getLastDuration());
}

// テスト8: 設定更新テスト
void test_config_update() {
    ConfigManager::BuzzerConfig newConfig;
    newConfig.enabled = false;
    newConfig.volume = 25;
    
    testBuzzer->updateConfig(newConfig);
    
    TEST_ASSERT_FALSE(testBuzzer->isEnabled());
    TEST_ASSERT_EQUAL(25, testBuzzer->getVolume());
}

void setup() {
    UNITY_BEGIN();
    
    RUN_TEST(test_buzzer_manager_initialization);
    RUN_TEST(test_startup_sound);
    RUN_TEST(test_button_sound);
    RUN_TEST(test_error_sound);
    RUN_TEST(test_disabled_buzzer);
    RUN_TEST(test_volume_control);
    RUN_TEST(test_custom_tone);
    RUN_TEST(test_config_update);
    
    UNITY_END();
}

void loop() {
    // Unityテストでは不要
}