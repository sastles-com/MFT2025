/**
 * @file led_controller.cpp
 * @brief LED制御・応答性最適化実装
 */

#include "led_controller.h"

/**
 * @brief コンストラクタ
 */
LEDController::LEDController()
  : leds_(nullptr)
  , led_count_(0)
  , led_pin_(35)
  , brightness_(128)
  , update_rate_(30)
  , current_mode_(LEDMode::INITIALIZATION)
  , last_update_time_(0)
  , last_joystick_time_(0)
  , animation_start_time_(0)
  , animation_phase_(0)
  , joystick_receive_time_(0)
  , led_update_start_time_(0)
  , stats_({0, 0, 0.0, 0.0, 0}) {
}

/**
 * @brief デストラクタ
 */
LEDController::~LEDController() {
  end();
}

/**
 * @brief LED制御システム初期化
 */
bool LEDController::begin(const ConfigManager& config) {
  Serial.println("LEDController: 初期化開始");
  
  const LEDConfig& led_config = config.getLEDConfig();
  
  led_pin_ = led_config.pin;
  led_count_ = led_config.count;
  brightness_ = led_config.brightness;
  update_rate_ = led_config.update_rate;
  
  Serial.printf("LED設定: Pin=%d, Count=%d, Brightness=%d\n", 
                led_pin_, led_count_, brightness_);
  
  // LED配列確保
  leds_ = new CRGB[led_count_];
  if (!leds_) {
    Serial.println("❌ LEDController: メモリ確保失敗");
    return false;
  }
  
  // FastLED初期化（GPIO35, WS2812）
  FastLED.addLeds<WS2812, 35, GRB>(leds_, led_count_);
  FastLED.setBrightness(brightness_);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setTemperature(DirectSunlight);
  
  // 初期色設定（消灯）
  fillSolid(CRGB::Black);
  FastLED.show();
  
  // 統計初期化
  resetStats();
  
  Serial.println("✅ LEDController: 初期化完了");
  return true;
}

/**
 * @brief 終了処理
 */
void LEDController::end() {
  if (leds_) {
    fillSolid(CRGB::Black);
    FastLED.show();
    delete[] leds_;
    leds_ = nullptr;
  }
  
  Serial.println("LEDController: 終了完了");
}

/**
 * @brief Joystickデータからの更新
 */
void LEDController::updateFromJoystick(const JoystickData& data) {
  joystick_receive_time_ = millis();
  last_joystick_time_ = joystick_receive_time_;
  
  if (current_mode_ != LEDMode::NORMAL) {
    setMode(LEDMode::NORMAL);
  }
  
  // LED更新開始時刻記録
  led_update_start_time_ = micros();
  
  updateNormalMode(data);
  
  // 応答性測定
  measureUpdateTime();
}

/**
 * @brief 定期更新（アニメーション等）
 */
void LEDController::update() {
  unsigned long now = millis();
  
  // アニメーションモードの更新
  if (current_mode_ != LEDMode::NORMAL) {
    updateAnimationMode();
  }
  
  // No Signal状態判定
  if (current_mode_ == LEDMode::NORMAL) {
    if (now - last_joystick_time_ > 3000) { // 3秒間未受信
      setMode(LEDMode::NO_SIGNAL);
    }
  }
  
  last_update_time_ = now;
}

/**
 * @brief 通常モード更新（Joystick制御）
 */
void LEDController::updateNormalMode(const JoystickData& data) {
  CRGB target_color;
  
  // ボタン押下判定・特殊色設定（新しいボタン構造対応）
  if (data.button_left) {
    target_color = getButtonAPattern();  // Lボタン → 旧A機能
  } else if (data.button_right) {
    target_color = getButtonBPattern();  // Rボタン → 旧B機能
  } else if (data.left_stick_button || data.right_stick_button) {
    target_color = getButtonCenterPattern();  // スティック押し込み → 旧Center機能
  } else {
    // スティック操作による色制御
    target_color = joystickToColor(data);
  }
  
  // LED更新
  fillSolid(target_color);
  applyBrightness();
  FastLED.show();
  
  // 統計更新
  updateStats();
  
  // デバッグ出力
  static unsigned long last_debug_time = 0;
  if (millis() - last_debug_time > 1000) { // 1秒間隔
    printColorInfo(target_color, data);
    last_debug_time = millis();
  }
}

/**
 * @brief アニメーションモード更新
 */
void LEDController::updateAnimationMode() {
  switch (current_mode_) {
    case LEDMode::INITIALIZATION:
      animateInitialization();
      break;
    case LEDMode::NO_SIGNAL:
      animateNoSignal();
      break;
    case LEDMode::WIFI_DISCONNECTED:
      animateWiFiDisconnected();
      break;
    case LEDMode::ERROR:
      animateError();
      break;
    case LEDMode::TEST_PATTERN:
      animateTestPattern();
      break;
    default:
      break;
  }
}

/**
 * @brief Joystickデータ→色変換
 */
CRGB LEDController::joystickToColor(const JoystickData& data) {
  HSVColor hsv = calculateJoystickHSV(data);
  return hsvToRgb(hsv);
}

/**
 * @brief JoystickデータからHSV計算
 */
HSVColor LEDController::calculateJoystickHSV(const JoystickData& data) {
  // 左スティック → 色相・彩度
  float hue_input = (data.left_x + 1.0f) * 0.5f; // 0.0-1.0変換
  float sat_input = (data.left_y + 1.0f) * 0.5f;
  
  // 右スティック → 明度
  float val_input = (data.right_y + 1.0f) * 0.5f;
  
  HSVColor hsv;
  hsv.hue = (uint8_t)(hue_input * 255.0f);
  hsv.saturation = (uint8_t)(sat_input * 255.0f);
  hsv.value = (uint8_t)(val_input * brightness_);
  
  return hsv;
}

/**
 * @brief HSV→RGB変換
 */
CRGB LEDController::hsvToRgb(const HSVColor& hsv) {
  return CHSV(hsv.hue, hsv.saturation, hsv.value);
}

/**
 * @brief ボタンAパターン（赤色系）
 */
CRGB LEDController::getButtonAPattern() {
  uint8_t intensity = (uint8_t)(brightness_ * (0.5f + 0.5f * sin(millis() * 0.01f)));
  return CRGB(intensity, 0, 0);
}

/**
 * @brief ボタンBパターン（青色系）
 */
CRGB LEDController::getButtonBPattern() {
  uint8_t intensity = (uint8_t)(brightness_ * (0.5f + 0.5f * sin(millis() * 0.01f)));
  return CRGB(0, 0, intensity);
}

/**
 * @brief センターボタンパターン（白色）
 */
CRGB LEDController::getButtonCenterPattern() {
  uint8_t intensity = brightness_;
  return CRGB(intensity, intensity, intensity);
}

/**
 * @brief モード設定
 */
void LEDController::setMode(LEDMode mode) {
  if (current_mode_ != mode) {
    current_mode_ = mode;
    animation_start_time_ = millis();
    animation_phase_ = 0;
    
    Serial.printf("LEDController: モード変更 -> %d\n", (int)mode);
  }
}

/**
 * @brief 初期化完了表示
 */
void LEDController::showInitializationComplete() {
  setMode(LEDMode::INITIALIZATION);
  
  // 緑色で2秒間点灯
  fillSolid(CRGB::Green);
  applyBrightness();
  FastLED.show();
  
  delay(2000);
  
  // 消灯して通常モード準備
  fillSolid(CRGB::Black);
  FastLED.show();
  
  setMode(LEDMode::NO_SIGNAL);
}

/**
 * @brief 信号なし表示
 */
void LEDController::showNoSignal() {
  setMode(LEDMode::NO_SIGNAL);
}

/**
 * @brief WiFi切断表示
 */
void LEDController::showWiFiDisconnected() {
  setMode(LEDMode::WIFI_DISCONNECTED);
}

/**
 * @brief エラー表示
 */
void LEDController::showError() {
  setMode(LEDMode::ERROR);
}

/**
 * @brief 信号なしアニメーション
 */
void LEDController::animateNoSignal() {
  // ゆっくりとした青色呼吸
  unsigned long elapsed = millis() - animation_start_time_;
  float breath = 0.5f + 0.5f * sin(elapsed * 0.001f); // 1秒周期
  
  uint8_t blue_intensity = (uint8_t)(brightness_ * 0.3f * breath);
  fillSolid(CRGB(0, 0, blue_intensity));
  FastLED.show();
}

/**
 * @brief WiFi切断アニメーション
 */
void LEDController::animateWiFiDisconnected() {
  // 速い赤色点滅
  unsigned long elapsed = millis() - animation_start_time_;
  bool blink = (elapsed % 500) < 250; // 0.5秒周期
  
  if (blink) {
    fillSolid(CRGB(brightness_, 0, 0));
  } else {
    fillSolid(CRGB::Black);
  }
  FastLED.show();
}

/**
 * @brief エラーアニメーション
 */
void LEDController::animateError() {
  // 非常に速い赤色点滅
  unsigned long elapsed = millis() - animation_start_time_;
  bool blink = (elapsed % 200) < 100; // 0.2秒周期
  
  if (blink) {
    fillSolid(CRGB(brightness_, 0, 0));
  } else {
    fillSolid(CRGB::Black);
  }
  FastLED.show();
}

/**
 * @brief 単色塗りつぶし
 */
void LEDController::fillSolid(CRGB color) {
  for (int i = 0; i < led_count_; i++) {
    leds_[i] = color;
  }
}

/**
 * @brief 明度適用
 */
void LEDController::applyBrightness() {
  // FastLEDの明度設定を使用
  FastLED.setBrightness(brightness_);
}

/**
 * @brief 更新時間測定
 */
void LEDController::measureUpdateTime() {
  unsigned long update_time = micros() - led_update_start_time_;
  float update_time_ms = update_time / 1000.0f;
  
  // 平均更新時間計算
  static const int SAMPLES = 10;
  static float samples[SAMPLES] = {0};
  static int sample_index = 0;
  
  samples[sample_index] = update_time_ms;
  sample_index = (sample_index + 1) % SAMPLES;
  
  float sum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    sum += samples[i];
  }
  stats_.avg_update_time = sum / SAMPLES;
  
  // 最大更新時間更新
  if (update_time_ms > stats_.max_update_time) {
    stats_.max_update_time = update_time_ms;
  }
  
  // 応答性警告
  if (update_time_ms > 10.0f) { // 10ms超過警告
    Serial.printf("⚠️  LED更新時間警告: %.2fms\n", update_time_ms);
  }
}

/**
 * @brief 統計更新
 */
void LEDController::updateStats() {
  stats_.updates_count++;
  stats_.last_update_time = millis();
}

/**
 * @brief 統計出力
 */
void LEDController::printStats() const {
  Serial.println("\n========== LED制御統計 ==========");
  Serial.printf("更新回数: %lu\n", stats_.updates_count);
  Serial.printf("平均更新時間: %.2fms\n", stats_.avg_update_time);
  Serial.printf("最大更新時間: %.2fms\n", stats_.max_update_time);
  Serial.printf("色変更回数: %lu\n", stats_.color_changes);
  
  if (stats_.last_update_time > 0) {
    unsigned long since_last = millis() - stats_.last_update_time;
    Serial.printf("最終更新: %lums前\n", since_last);
  }
  
  Serial.println("==================================\n");
}

/**
 * @brief カラー情報出力（デバッグ）
 */
void LEDController::printColorInfo(CRGB color, const JoystickData& data) const {
  Serial.printf("LED: RGB(%d,%d,%d) <- Joy(%.2f,%.2f,%.2f,%.2f) Btn:L%d R%d LS%d RS%d\n",
                color.r, color.g, color.b,
                data.left_x, data.left_y, data.right_x, data.right_y,
                data.button_left, data.button_right, data.left_stick_button, data.right_stick_button);
}

/**
 * @brief 初期化アニメーション
 */
void LEDController::animateInitialization() {
  // 緑色で3回点滅
  unsigned long elapsed = millis() - animation_start_time_;
  bool blink = (elapsed % 400) < 200; // 0.4秒周期
  
  if (blink) {
    fillSolid(CRGB(0, brightness_, 0));
  } else {
    fillSolid(CRGB::Black);
  }
  FastLED.show();
  
  // 3秒後にNO_SIGNALに移行
  if (elapsed > 3000) {
    setMode(LEDMode::NO_SIGNAL);
  }
}

/**
 * @brief テストパターンアニメーション
 */
void LEDController::animateTestPattern() {
  // レインボーサイクル
  unsigned long elapsed = millis() - animation_start_time_;
  uint8_t hue = (elapsed / 10) % 256; // 2.56秒で1周
  
  fillSolid(CHSV(hue, 255, brightness_));
  FastLED.show();
}

/**
 * @brief 統計リセット
 */
void LEDController::resetStats() {
  stats_.updates_count = 0;
  stats_.avg_update_time = 0.0f;
  stats_.max_update_time = 0.0f;
  stats_.color_changes = 0;
  stats_.last_update_time = 0;
  
  Serial.println("LEDController: 統計リセット完了");
}