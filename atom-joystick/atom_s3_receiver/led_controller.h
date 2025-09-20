/**
 * @file led_controller.h
 * @brief LED制御・応答性最適化システム
 * @description Joystickデータ→WS2812 LED制御・15-30ms応答性実現
 */

#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include "config_manager.h"
#include "udp_receiver.h"

/**
 * @brief LED表示モード
 */
enum class LEDMode {
  INITIALIZATION,     // 初期化完了表示
  NORMAL,            // 通常制御（Joystick連動）
  NO_SIGNAL,         // 信号なし（待機状態）
  WIFI_DISCONNECTED, // WiFi切断中
  ERROR,             // エラー状態
  TEST_PATTERN       // テストパターン
};

/**
 * @brief LED制御統計
 */
struct LEDControlStats {
  unsigned long updates_count;
  unsigned long last_update_time;
  float avg_update_time;
  float max_update_time;
  unsigned long color_changes;
};

/**
 * @brief HSVカラー構造体
 */
struct HSVColor {
  uint8_t hue;        // 色相 (0-255)
  uint8_t saturation; // 彩度 (0-255)
  uint8_t value;      // 明度 (0-255)
  
  HSVColor(uint8_t h = 0, uint8_t s = 255, uint8_t v = 128) 
    : hue(h), saturation(s), value(v) {}
};

/**
 * @brief LED制御クラス
 */
class LEDController {
public:
  LEDController();
  ~LEDController();
  
  // 初期化・終了処理
  bool begin(const ConfigManager& config);
  void end();
  
  // 制御メソッド
  void updateFromJoystick(const JoystickData& data);
  void update(); // 定期実行（アニメーション等）
  
  // 表示モード制御
  void setMode(LEDMode mode);
  LEDMode getMode() const { return current_mode_; }
  
  // 状態表示メソッド
  void showInitializationComplete();
  void showNoSignal();
  void showWiFiDisconnected();
  void showError();
  void showTestPattern();
  
  // 設定変更
  void setBrightness(uint8_t brightness);
  void setUpdateRate(uint8_t rate_hz);
  
  // 統計情報
  const LEDControlStats& getStats() const { return stats_; }
  void printStats() const;
  void resetStats();

private:
  CRGB* leds_;
  int led_count_;
  int led_pin_;
  uint8_t brightness_;
  uint8_t update_rate_;
  
  LEDMode current_mode_;
  unsigned long last_update_time_;
  unsigned long last_joystick_time_;
  
  // アニメーション用
  unsigned long animation_start_time_;
  uint8_t animation_phase_;
  
  // 応答性測定
  unsigned long joystick_receive_time_;
  unsigned long led_update_start_time_;
  
  // 統計情報
  LEDControlStats stats_;
  
  // 内部制御メソッド
  void updateNormalMode(const JoystickData& data);
  void updateAnimationMode();
  
  // カラー変換
  CRGB joystickToColor(const JoystickData& data);
  CRGB hsvToRgb(const HSVColor& hsv);
  HSVColor calculateJoystickHSV(const JoystickData& data);
  
  // ボタン別パターン
  CRGB getButtonAPattern();
  CRGB getButtonBPattern();
  CRGB getButtonCenterPattern();
  
  // アニメーション
  void animateInitialization();
  void animateNoSignal();
  void animateWiFiDisconnected();
  void animateError();
  void animateTestPattern();
  
  // ユーティリティ
  void fillSolid(CRGB color);
  void applyBrightness();
  void measureUpdateTime();
  void updateStats();
  
  // デバッグ出力
  void printColorInfo(CRGB color, const JoystickData& data) const;
};