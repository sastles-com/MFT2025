/**
 * @file JoystickBuzzer.h
 * @brief Atom-JoyStick ブザー制御システム
 * @description StampFlyController参考のGPIO5 PWMブザー制御
 */

#pragma once
#include <Arduino.h>

/**
 * @brief ブザー制御設定
 */
struct BuzzerConfig {
  int pin;                         // ブザーピン（GPIO5）
  int pwm_channel;                 // PWMチャンネル
  int resolution;                  // PWM分解能（bit）
  int default_frequency;           // デフォルト周波数（Hz）
  int volume;                      // 音量（0-255）
  
  // デフォルト設定
  BuzzerConfig() 
    : pin(5)                      // Atom-JoyStick GPIO5
    , pwm_channel(0)              // PWMチャンネル0
    , resolution(8)               // 8bit分解能
    , default_frequency(4000)     // 4kHz
    , volume(51) {}               // 音量20%
};

/**
 * @brief 音符周波数定義（StampFlyController準拠）
 */
#define NOTE_D1    294
#define NOTE_D2    330
#define NOTE_D3    350
#define NOTE_D4    393
#define NOTE_D5    441
#define NOTE_D6    495
#define NOTE_D7    556
#define NOTE_C4    262
#define NOTE_E4    330
#define NOTE_F4    349  // ファ（起動音用）
#define NOTE_G4    392
#define NOTE_C5    523

// 起動音メロディー定数
#define STARTUP_NOTE_HIGH    NOTE_C5    // C5 (ド) - 高音
#define STARTUP_NOTE_MID     NOTE_G4    // G4 (ソ) - 中音  
#define STARTUP_NOTE_LOW     NOTE_F4    // F4 (ファ) - 低音
#define STARTUP_NOTE_DURATION  300      // 各音300ms
#define STARTUP_NOTE_PAUSE     100      // 音間100ms

/**
 * @brief ブザー統計
 */
struct BuzzerStats {
  unsigned long total_beeps;       // 総ビープ回数
  unsigned long total_play_time;   // 総再生時間
  unsigned long last_beep_time;    // 最終ビープ時刻
  int last_frequency;              // 最終周波数
  int last_duration;               // 最終再生時間
};

/**
 * @brief Atom-JoyStick ブザー制御クラス
 */
class JoystickBuzzer {
public:
  JoystickBuzzer();
  ~JoystickBuzzer();
  
  // 初期化・終了
  bool begin();
  bool begin(const BuzzerConfig& config);
  void end();
  
  // 基本ブザー制御
  void buzzer_sound(uint32_t frequency, uint32_t duration_ms);
  void beep();
  void stop();
  
  // プリセット音色（StampFlyController準拠）
  void start_tone();               // 起動音
  void startup_melody();           // 起動メロディー（3音下降）
  void good_voltage_tone();        // 正常動作音
  void error_tone();               // エラー音
  void completion_tone();          // 完了音
  
  // オープニング専用音色
  void opening_startup_melody();   // オープニング開始メロディ
  void opening_completion_melody();// オープニング完了メロディ
  void frame_advance_beep();       // フレーム進行音
  
  // システム音色
  void wifi_connected_tone();      // WiFi接続音
  void udp_connected_tone();       // UDP接続音
  void button_click();             // ボタンクリック音
  
  // 設定・統計
  void setVolume(int volume);      // 音量設定（0-255）
  void setEnabled(bool enabled);   // ブザー有効/無効
  bool isEnabled() const { return enabled_; }
  
  const BuzzerStats& getStats() const { return stats_; }
  void printStats() const;
  void resetStats();

private:
  BuzzerConfig config_;
  BuzzerStats stats_;
  bool initialized_;
  bool enabled_;
  
  // 内部制御メソッド
  void playTone(uint32_t frequency, uint32_t duration_ms);
  void playNote(int note_frequency, uint32_t duration_ms);
  void playMelody(const int* notes, const int* durations, int note_count);
  
  // PWM制御
  void setupPWM();
  void startPWM(uint32_t frequency);
  void stopPWM();
  
  // 統計更新
  void updateStats(uint32_t frequency, uint32_t duration);
  
  // デバッグ・ログ
  void logSound(const char* sound_name, uint32_t frequency, uint32_t duration);
  void printError(const char* message, const char* detail = nullptr);
};

// プリセットメロディデータ
struct MelodyData {
  const int* notes;
  const int* durations;
  int note_count;
};