/**
 * @file JoystickBuzzer.cpp
 * @brief Atom-JoyStick ブザー制御システム実装
 */

#include "JoystickBuzzer.h"

// プリセットメロディデータ
// 起動音メロディ
const int startup_notes[] = {NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5};
const int startup_durations[] = {200, 200, 200, 400};

// 完了音メロディ
const int completion_notes[] = {NOTE_G4, NOTE_C5, NOTE_E4, NOTE_C4};
const int completion_durations[] = {150, 150, 150, 300};

// オープニング開始メロディ
const int opening_startup_notes[] = {NOTE_C4, NOTE_D3, NOTE_E4, NOTE_G4, NOTE_C5};
const int opening_startup_durations[] = {120, 120, 120, 120, 200};

// オープニング完了メロディ
const int opening_completion_notes[] = {NOTE_C5, NOTE_G4, NOTE_E4, NOTE_C4, NOTE_G4, NOTE_C5};
const int opening_completion_durations[] = {100, 100, 100, 100, 150, 300};

/**
 * @brief コンストラクタ
 */
JoystickBuzzer::JoystickBuzzer() 
  : initialized_(false)
  , enabled_(true)  // デフォルト有効
  , stats_({0, 0, 0, 0, 0}) {
}

/**
 * @brief デストラクタ
 */
JoystickBuzzer::~JoystickBuzzer() {
  end();
}

/**
 * @brief 初期化（デフォルト設定）
 */
bool JoystickBuzzer::begin() {
  return begin(BuzzerConfig());
}

/**
 * @brief 初期化（カスタム設定）
 */
bool JoystickBuzzer::begin(const BuzzerConfig& config) {
  config_ = config;
  
  Serial.println("🎵 JoystickBuzzer: 初期化開始");
  Serial.printf("  ブザーピン: GPIO%d\n", config_.pin);
  Serial.printf("  PWMチャンネル: %d\n", config_.pwm_channel);
  Serial.printf("  音量: %d/255\n", config_.volume);
  
  // PWMセットアップ
  setupPWM();
  
  initialized_ = true;
  Serial.println("✅ JoystickBuzzer: 初期化完了");
  
  // 初期化完了音
  if (enabled_) {
    beep();
  }
  
  return true;
}

/**
 * @brief 終了処理
 */
void JoystickBuzzer::end() {
  if (initialized_) {
    stop();
    initialized_ = false;
    Serial.println("JoystickBuzzer: 終了完了");
  }
}

/**
 * @brief PWMセットアップ（StampFlyController準拠）
 */
void JoystickBuzzer::setupPWM() {
  // 新しいESP32 LEDC API使用
  ledcAttach(config_.pin, config_.default_frequency, config_.resolution);
  
  Serial.printf("✅ PWM初期化完了: GPIO%d -> %dHz, %dbit\n", config_.pin, config_.default_frequency, config_.resolution);
}

/**
 * @brief ブザー音生成（基本機能）
 */
void JoystickBuzzer::buzzer_sound(uint32_t frequency, uint32_t duration_ms) {
  if (!initialized_ || !enabled_) {
    return;
  }
  
  playTone(frequency, duration_ms);
  updateStats(frequency, duration_ms);
}

/**
 * @brief 基本ビープ音（StampFlyController準拠）
 */
void JoystickBuzzer::beep() {
  buzzer_sound(config_.default_frequency, 100);
  logSound("beep", config_.default_frequency, 100);
}

/**
 * @brief 音停止
 */
void JoystickBuzzer::stop() {
  if (initialized_) {
    stopPWM();
  }
}

/**
 * @brief 起動音（StampFlyController準拠）
 */
void JoystickBuzzer::start_tone() {
  if (!enabled_) return;
  
  Serial.println("🎵 起動音再生中...");
  playMelody(startup_notes, startup_durations, 4);
  logSound("start_tone", 0, 800);
}

/**
 * @brief 3音下降メロディー（Atom-JoyStick専用起動音）
 * @description C5(ド) → G4(ソ) → F4(ファ) 下降音階
 */
void JoystickBuzzer::startup_melody() {
  if (!enabled_) return;
  
  Serial.println("🎵 起動メロディー再生中（下降音階）...");
  
  // C5 (ド) - 高音
  playTone(STARTUP_NOTE_HIGH, STARTUP_NOTE_DURATION);
  delay(STARTUP_NOTE_PAUSE);
  
  // G4 (ソ) - 中音  
  playTone(STARTUP_NOTE_MID, STARTUP_NOTE_DURATION);
  delay(STARTUP_NOTE_PAUSE);
  
  // F4 (ファ) - 低音
  playTone(STARTUP_NOTE_LOW, STARTUP_NOTE_DURATION);
  
  logSound("startup_melody_descending", 0, (STARTUP_NOTE_DURATION * 3) + (STARTUP_NOTE_PAUSE * 2));
}

/**
 * @brief 正常動作音（StampFlyController準拠）
 */
void JoystickBuzzer::good_voltage_tone() {
  if (!enabled_) return;
  
  Serial.println("🎵 正常動作音再生中...");
  
  // // 上昇音階パターン
  // playNote(NOTE_C4, 150);
  // delay(50);
  // playNote(NOTE_E4, 150);
  // delay(50);
  // playNote(NOTE_G4, 200);
  
  logSound("good_voltage_tone", 0, 550);
}

/**
 * @brief エラー音
 */
void JoystickBuzzer::error_tone() {
  if (!enabled_) return;
  
  Serial.println("🎵 エラー音再生中...");
  
  // 警告音パターン
  for (int i = 0; i < 3; i++) {
    playTone(800, 100);
    delay(100);
    playTone(400, 100);
    delay(100);
  }
  
  logSound("error_tone", 0, 600);
}

/**
 * @brief 完了音
 */
void JoystickBuzzer::completion_tone() {
  if (!enabled_) return;
  
  Serial.println("🎵 完了音再生中...");
  playMelody(completion_notes, completion_durations, 4);
  logSound("completion_tone", 0, 750);
}

/**
 * @brief オープニング開始メロディ
 */
void JoystickBuzzer::opening_startup_melody() {
  if (!enabled_) return;
  
  Serial.println("🎬🎵 オープニング開始メロディ再生中...");
  playMelody(opening_startup_notes, opening_startup_durations, 5);
  logSound("opening_startup", 0, 680);
}

/**
 * @brief オープニング完了メロディ
 */
void JoystickBuzzer::opening_completion_melody() {
  if (!enabled_) return;
  
  Serial.println("🎬🎵 オープニング完了メロディ再生中...");
  playMelody(opening_completion_notes, opening_completion_durations, 6);
  logSound("opening_completion", 0, 850);
}

/**
 * @brief フレーム進行音
 */
void JoystickBuzzer::frame_advance_beep() {
  if (!enabled_) return;
  
  // 短い上昇音
  playTone(600, 50);
  delay(10);
  playTone(800, 30);
}

/**
 * @brief WiFi接続音
 */
void JoystickBuzzer::wifi_connected_tone() {
  if (!enabled_) return;
  
  Serial.println("🎵 WiFi接続音再生中...");
  
  // 接続成功パターン
  playTone(400, 100);
  delay(50);
  playTone(600, 100);
  delay(50);
  playTone(800, 200);
  
  logSound("wifi_connected", 0, 450);
}

/**
 * @brief UDP接続音
 */
void JoystickBuzzer::udp_connected_tone() {
  if (!enabled_) return;
  
  Serial.println("🎵 UDP接続音再生中...");
  
  // 短い接続確認音
  playTone(1000, 80);
  delay(40);
  playTone(1200, 80);
  
  logSound("udp_connected", 0, 200);
}

/**
 * @brief ボタンクリック音
 */
void JoystickBuzzer::button_click() {
  if (!enabled_) return;
  
  // 短いクリック音
  playTone(1500, 30);
}

/**
 * @brief 音量設定
 */
void JoystickBuzzer::setVolume(int volume) {
  config_.volume = constrain(volume, 0, 255);
  Serial.printf("🎵 音量設定: %d/255\n", config_.volume);
}

/**
 * @brief ブザー有効/無効設定
 */
void JoystickBuzzer::setEnabled(bool enabled) {
  enabled_ = enabled;
  if (!enabled) {
    stop();
  }
  Serial.printf("🎵 ブザー: %s\n", enabled ? "有効" : "無効");
}

/**
 * @brief 音色再生（内部メソッド）
 */
void JoystickBuzzer::playTone(uint32_t frequency, uint32_t duration_ms) {
  if (!initialized_) return;
  
  startPWM(frequency);
  delay(duration_ms);
  stopPWM();
}

/**
 * @brief 音符再生（内部メソッド）
 */
void JoystickBuzzer::playNote(int note_frequency, uint32_t duration_ms) {
  playTone(note_frequency, duration_ms);
}

/**
 * @brief メロディ再生（内部メソッド）
 */
void JoystickBuzzer::playMelody(const int* notes, const int* durations, int note_count) {
  for (int i = 0; i < note_count; i++) {
    playNote(notes[i], durations[i]);
    delay(50);  // 音符間の間隔
  }
}

/**
 * @brief PWM開始
 */
void JoystickBuzzer::startPWM(uint32_t frequency) {
  ledcWriteTone(config_.pin, frequency);
  ledcWrite(config_.pin, config_.volume);
}

/**
 * @brief PWM停止
 */
void JoystickBuzzer::stopPWM() {
  ledcWrite(config_.pin, 0);
}

/**
 * @brief 統計更新
 */
void JoystickBuzzer::updateStats(uint32_t frequency, uint32_t duration) {
  stats_.total_beeps++;
  stats_.total_play_time += duration;
  stats_.last_beep_time = millis();
  stats_.last_frequency = frequency;
  stats_.last_duration = duration;
}

/**
 * @brief 音色ログ出力
 */
void JoystickBuzzer::logSound(const char* sound_name, uint32_t frequency, uint32_t duration) {
  Serial.printf("🎵 音再生: %s", sound_name);
  if (frequency > 0) {
    Serial.printf(" | %dHz, %dms", frequency, duration);
  }
  Serial.println();
}

/**
 * @brief エラー出力
 */
void JoystickBuzzer::printError(const char* message, const char* detail) {
  Serial.printf("❌ JoystickBuzzer: %s", message);
  if (detail != nullptr) {
    Serial.printf(" - %s", detail);
  }
  Serial.println();
}

/**
 * @brief 統計情報出力
 */
void JoystickBuzzer::printStats() const {
  Serial.println();
  Serial.println("========== Joystick ブザー統計 ==========");
  Serial.printf("総ビープ回数: %lu\n", stats_.total_beeps);
  Serial.printf("総再生時間: %lums\n", stats_.total_play_time);
  Serial.printf("最終ビープ: %lums前\n", millis() - stats_.last_beep_time);
  Serial.printf("最終周波数: %dHz\n", stats_.last_frequency);
  Serial.printf("最終再生時間: %dms\n", stats_.last_duration);
  Serial.printf("ブザー状態: %s\n", enabled_ ? "有効" : "無効");
  Serial.println("========================================");
  Serial.println();
}

/**
 * @brief 統計リセット
 */
void JoystickBuzzer::resetStats() {
  stats_ = {0, 0, 0, 0, 0};
  Serial.println("JoystickBuzzer: 統計リセット完了");
}