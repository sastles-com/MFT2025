#include "buzzer/M5SpeakerBuzzer.h"
#include <M5Unified.h>

M5SpeakerBuzzer::M5SpeakerBuzzer(const ConfigManager::BuzzerConfig& config)
    : config_(config), initialized_(false) {
}

M5SpeakerBuzzer::~M5SpeakerBuzzer() {
    // M5Unifiedの解放は不要
}

bool M5SpeakerBuzzer::initialize() {
    if (initialized_) {
        return true;
    }

    // M5Unified Speakerの初期化確認
    Serial.println("[M5SpeakerBuzzer] Checking M5 Speaker availability...");
    
    // スピーカーが利用可能かチェック
    auto spk_cfg = M5.Speaker.config();
    Serial.printf("[M5SpeakerBuzzer] Speaker config - buzzer: %s\n", spk_cfg.buzzer ? "true" : "false");
    
    if (!M5.Speaker.isEnabled()) {
        Serial.println("[M5SpeakerBuzzer] M5 Speaker not available on this device");
        return false;
    }
    
    // 音量設定
    float volumeFloat = static_cast<float>(config_.volume) / 100.0f;
    M5.Speaker.setVolume(static_cast<uint8_t>(volumeFloat * 255));
    
    initialized_ = true;
    Serial.printf("[M5SpeakerBuzzer] M5 Speaker initialized (volume: %d%%)\n", config_.volume);
    
    // 初期化完了音（設定が有効な場合）
    if (shouldPlaySound()) {
        playStartupMelody();
    }
    
    return true;
}

void M5SpeakerBuzzer::playStartupMelody() {
    if (!shouldPlaySound()) return;
    
    Serial.println("[M5SpeakerBuzzer] Playing startup melody...");
    // 起動メロディ: C-E-G-C (ドミソド)
    playToneInternal(523, 200);  // C5
    delay(50);
    playToneInternal(659, 200);  // E5
    delay(50);
    playToneInternal(784, 200);  // G5
    delay(50);
    playToneInternal(1047, 300); // C6
}

void M5SpeakerBuzzer::playClickTone() {
    if (!shouldPlaySound()) return;
    
    Serial.println("[M5SpeakerBuzzer] Playing click tone...");
    // 短いクリック音
    playToneInternal(1000, 80);
}

void M5SpeakerBuzzer::playErrorTone() {
    if (!shouldPlaySound()) return;
    
    Serial.println("[M5SpeakerBuzzer] Playing error tone...");
    // エラー音: 低い音を3回
    for (int i = 0; i < 3; i++) {
        playToneInternal(200, 150);
        delay(100);
        delay(50);
    }
}

void M5SpeakerBuzzer::playCompletionTone() {
    if (!shouldPlaySound()) return;
    
    Serial.println("[M5SpeakerBuzzer] Playing completion tone...");
    // 完了音: 上昇音程
    playToneInternal(400, 150);
    delay(50);
    playToneInternal(600, 150);
    delay(50);
    playToneInternal(800, 200);
}

void M5SpeakerBuzzer::playConnectTone() {
    if (!shouldPlaySound()) return;
    
    Serial.println("[M5SpeakerBuzzer] Playing connect tone...");
    // 接続音: 2音上昇
    playToneInternal(600, 120);
    delay(30);
    playToneInternal(900, 180);
}

void M5SpeakerBuzzer::playDisconnectTone() {
    if (!shouldPlaySound()) return;
    
    Serial.println("[M5SpeakerBuzzer] Playing disconnect tone...");
    // 切断音: 2音下降
    playToneInternal(900, 120);
    delay(30);
    playToneInternal(600, 180);
}

void M5SpeakerBuzzer::playWarningTone() {
    if (!shouldPlaySound()) return;
    
    Serial.println("[M5SpeakerBuzzer] Playing warning tone...");
    // 警告音: 高い音を2回
    for (int i = 0; i < 2; i++) {
        playToneInternal(1500, 200);
        delay(150);
        delay(100);
    }
}

void M5SpeakerBuzzer::playTone(int frequency, int duration) {
    if (!shouldPlaySound()) return;
    
    Serial.printf("[M5SpeakerBuzzer] Playing custom tone: %dHz, %dms\n", frequency, duration);
    playToneInternal(frequency, duration);
}

bool M5SpeakerBuzzer::isEnabled() const {
    return config_.enabled;
}

uint8_t M5SpeakerBuzzer::getVolume() const {
    return config_.volume;
}

void M5SpeakerBuzzer::updateConfig(const ConfigManager::BuzzerConfig& config) {
    config_ = config;
    
    if (initialized_ && config_.enabled) {
        // M5Unified Speakerの音量更新
        float volumeFloat = static_cast<float>(config_.volume) / 100.0f;
        M5.Speaker.setVolume(static_cast<uint8_t>(volumeFloat * 255));
        Serial.printf("[M5SpeakerBuzzer] Volume updated to %d%%\n", config_.volume);
    }
}

void M5SpeakerBuzzer::playToneInternal(int frequency, int duration) {
    // 開発時強制無効化
    Serial.println("[M5SpeakerBuzzer] playToneInternal: FORCED DISABLED FOR DEVELOPMENT");
    return;
    
    if (!initialized_ || !shouldPlaySound()) {
        return;
    }
    
    // M5Unified Speakerを使用してトーンを再生
    Serial.printf("[M5SpeakerBuzzer] M5.Speaker.tone(%d, %d)\n", frequency, duration);
    M5.Speaker.tone(frequency, duration);
    delay(duration);
    M5.Speaker.stop();
}

bool M5SpeakerBuzzer::shouldPlaySound() const {
    return config_.enabled && config_.volume > 0 && initialized_;
}