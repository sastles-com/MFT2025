#include "buzzer/JoystickBuzzer.h"
#include <Arduino.h>

JoystickBuzzer::JoystickBuzzer(const ConfigManager::BuzzerConfig& config)
    : config_(config), hasAudioConfig_(false), initialized_(false) {
}

JoystickBuzzer::JoystickBuzzer(const ConfigManager::JoystickConfig::AudioConfig& audioConfig)
    : audioConfig_(audioConfig), hasAudioConfig_(true), initialized_(false) {
    // 従来のconfig_も互換性のため設定
    config_.enabled = audioConfig.enabled;
    config_.volume = audioConfig.masterVolume;
    Serial.printf("[JoystickBuzzer] Constructor: Audio config enabled=%s, masterVolume=%d\n",
                 audioConfig.enabled ? "true" : "false", audioConfig.masterVolume);
}

JoystickBuzzer::~JoystickBuzzer() {
    if (initialized_) {
        stopTone();
        ledcDetachPin(BUZZER_PIN);
    }
}

bool JoystickBuzzer::initialize() {
    if (initialized_) {
        return true;
    }

    // GPIO5をPWM出力として設定
    if (!ledcSetup(PWM_CHANNEL, 1000, PWM_RESOLUTION)) {
        Serial.println("[JoystickBuzzer] PWM setup failed");
        return false;
    }
    ledcAttachPin(BUZZER_PIN, PWM_CHANNEL);

    // 初期化時は音を停止
    stopTone();
    
    Serial.printf("[JoystickBuzzer] PWM initialized on GPIO%d, channel %d\n", BUZZER_PIN, PWM_CHANNEL);
    
    initialized_ = true;
    
    // 初期化完了音（設定が有効な場合）
    if (shouldPlaySound()) {
        playStartupMelody();
    }
    
    return true;
}

void JoystickBuzzer::playStartupMelody() {
    if (!shouldPlaySound()) return;
    
    // 起動メロディ: C-E-G-C (ドミソド)
    playToneWithVolume(523, 200, "startup");  // C5
    delay(50);
    playToneWithVolume(659, 200, "startup");  // E5
    delay(50);
    playToneWithVolume(784, 200, "startup");  // G5
    delay(50);
    playToneWithVolume(1047, 300, "startup"); // C6
    stopTone();
}

void JoystickBuzzer::playClickTone() {
    if (!shouldPlaySound()) return;
    
    // 短いクリック音
    playToneWithVolume(1000, 80, "click");
    stopTone();
}

void JoystickBuzzer::playErrorTone() {
    if (!shouldPlaySound()) return;
    
    // エラー音: 低い音を3回
    for (int i = 0; i < 3; i++) {
        playToneWithVolume(200, 150, "error");
        delay(100);
        stopTone();
        delay(50);
    }
}

void JoystickBuzzer::playCompletionTone() {
    if (!shouldPlaySound()) return;
    
    // 完了音: 上昇音程
    playToneInternal(400, 150);
    delay(50);
    playToneInternal(600, 150);
    delay(50);
    playToneInternal(800, 200);
    stopTone();
}

void JoystickBuzzer::playConnectTone() {
    if (!shouldPlaySound()) return;
    
    // 接続音: 2音上昇
    playToneInternal(600, 120);
    delay(30);
    playToneInternal(900, 180);
    stopTone();
}

void JoystickBuzzer::playDisconnectTone() {
    if (!shouldPlaySound()) return;
    
    // 切断音: 2音下降
    playToneInternal(900, 120);
    delay(30);
    playToneInternal(600, 180);
    stopTone();
}

void JoystickBuzzer::playWarningTone() {
    if (!shouldPlaySound()) return;
    
    // 警告音: 高い音を2回
    for (int i = 0; i < 2; i++) {
        playToneInternal(1500, 200);
        delay(150);
        stopTone();
        delay(100);
    }
}

void JoystickBuzzer::playTone(int frequency, int duration) {
    if (!shouldPlaySound()) return;
    
    playToneInternal(frequency, duration);
    stopTone();
}

void JoystickBuzzer::playFrequencySweep() {
    if (!shouldPlaySound()) return;
    
    Serial.println("[JoystickBuzzer] Playing frequency sweep test (passive buzzer)");
    
    // 低周波から高周波へスイープ（パッシブブザーテスト）
    for (int freq = 200; freq <= 2000; freq += 100) {
        playToneInternal(freq, 100);
        delay(10);
    }
    stopTone();
}

void JoystickBuzzer::playScaleTest() {
    if (!shouldPlaySound()) return;
    
    Serial.println("[JoystickBuzzer] Playing musical scale test (passive buzzer)");
    
    // ドレミファソラシド（C4-C5）
    int scale[] = {262, 294, 330, 349, 392, 440, 494, 523};
    const char* notes[] = {"C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5"};
    
    for (int i = 0; i < 8; i++) {
        Serial.printf("[JoystickBuzzer] Playing %s (%dHz)\n", notes[i], scale[i]);
        playToneInternal(scale[i], 300);
        delay(100);
    }
    stopTone();
}

bool JoystickBuzzer::isEnabled() const {
    return config_.enabled;
}

uint8_t JoystickBuzzer::getVolume() const {
    return config_.volume;
}

void JoystickBuzzer::updateConfig(const ConfigManager::BuzzerConfig& config) {
    config_ = config;
    
    // ハードウェア設定の更新が必要な場合はここで実行
    // 現在のPWM実装では音量はデューティ比で制御可能
}

void JoystickBuzzer::playToneInternal(int frequency, int duration) {
    // 開発時強制無効化
    Serial.println("[JoystickBuzzer] playToneInternal: FORCED DISABLED FOR DEVELOPMENT");
    return;
    
    if (!initialized_ || !shouldPlaySound()) {
        return;
    }
    
    // PWM周波数を設定
    ledcChangeFrequency(PWM_CHANNEL, frequency, PWM_RESOLUTION);
    
    // 音量を考慮したデューティ比を計算 (0-100% → 0-255)
    uint8_t effectiveVolume = calculateEffectiveVolume();
    int dutyCycle = (DEFAULT_DUTY_CYCLE * effectiveVolume) / 100;
    ledcWrite(PWM_CHANNEL, dutyCycle);
    
    Serial.printf("[JoystickBuzzer] Playing tone: %dHz, duty: %d, duration: %dms\n", frequency, dutyCycle, duration);
    
    // 指定された時間だけ音を出力
    delay(duration);
}

void JoystickBuzzer::playToneWithVolume(int frequency, int duration, const char* soundType) {
    if (!initialized_ || !shouldPlaySound()) {
        return;
    }
    
    // PWM周波数を設定
    ledcChangeFrequency(PWM_CHANNEL, frequency, PWM_RESOLUTION);
    
    // 音種別音量を計算
    uint8_t soundVolume = calculateSoundVolume(soundType);
    int dutyCycle = (DEFAULT_DUTY_CYCLE * soundVolume) / 100;
    ledcWrite(PWM_CHANNEL, dutyCycle);
    
    Serial.printf("[JoystickBuzzer] Playing tone: %dHz, duty: %d, duration: %dms, type: %s, volume: %d%%\n", 
                  frequency, dutyCycle, duration, soundType, soundVolume);
    
    // 指定された時間だけ音を出力
    delay(duration);
}

void JoystickBuzzer::stopTone() {
    if (!initialized_) return;
    
    // PWM出力を停止
    ledcWrite(PWM_CHANNEL, 0);
    Serial.println("[JoystickBuzzer] Tone stopped");
}

bool JoystickBuzzer::shouldPlaySound() const {
    if (!initialized_) {
        Serial.println("[JoystickBuzzer] shouldPlaySound: Not initialized");
        return false;
    }
    
    bool result;
    if (hasAudioConfig_) {
        result = audioConfig_.enabled && calculateEffectiveVolume() > 0;
        Serial.printf("[JoystickBuzzer] shouldPlaySound: AudioConfig enabled=%s, volume=%d, result=%s\n",
                     audioConfig_.enabled ? "true" : "false", calculateEffectiveVolume(),
                     result ? "true" : "false");
    } else {
        result = config_.enabled && config_.volume > 0;
        Serial.printf("[JoystickBuzzer] shouldPlaySound: Basic config enabled=%s, volume=%d, result=%s\n",
                     config_.enabled ? "true" : "false", config_.volume,
                     result ? "true" : "false");
    }
    return result;
}

uint8_t JoystickBuzzer::calculateEffectiveVolume() const {
    if (hasAudioConfig_) {
        // マスター音量をそのまま使用（簡素化）
        return audioConfig_.masterVolume;
    } else {
        return config_.volume;
    }
}

uint8_t JoystickBuzzer::calculateSoundVolume(const char* soundType) const {
    if (!hasAudioConfig_) {
        return config_.volume;
    }
    
    uint8_t soundVolume = 50; // デフォルト
    if (strcmp(soundType, "startup") == 0) {
        soundVolume = audioConfig_.volumes.startup;
    } else if (strcmp(soundType, "click") == 0) {
        soundVolume = audioConfig_.volumes.click;
    } else if (strcmp(soundType, "error") == 0) {
        soundVolume = audioConfig_.volumes.error;
    } else if (strcmp(soundType, "test") == 0) {
        soundVolume = audioConfig_.volumes.test;
    }
    
    // マスター音量と音種別音量を掛け合わせる
    uint8_t finalVolume = static_cast<uint8_t>((audioConfig_.masterVolume * soundVolume) / 100);
    
    // デバッグ出力
    Serial.printf("[JoystickBuzzer] Volume calc: %s -> type:%d%% × master:%d%% = %d%%\n", 
                  soundType, soundVolume, audioConfig_.masterVolume, finalVolume);
    
    return finalVolume;
}

void JoystickBuzzer::initializeHardware() {
    // PWMの初期化はinitialize()メソッドで実行
}