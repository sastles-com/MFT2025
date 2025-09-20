/*
 * Joystick入力処理実装
 * isolation-sphere分散MQTT制御システム
 */

#include "joystick_input.h"
#include <M5Unified.h>
#include <ArduinoJson.h>
#include <math.h>

// M5Stack Atom-JoyStick ピン定義
#define LEFT_STICK_X_PIN    33  // 左スティックX軸
#define LEFT_STICK_Y_PIN    32  // 左スティックY軸
#define LEFT_STICK_BTN_PIN  25  // 左スティック押し込み

#define RIGHT_STICK_X_PIN   35  // 右スティックX軸
#define RIGHT_STICK_Y_PIN   34  // 右スティックY軸
#define RIGHT_STICK_BTN_PIN 26  // 右スティック押し込み

// 設定値
#define ADC_MAX_VALUE       4095
#define ADC_CENTER_VALUE    2047
#define JOYSTICK_RANGE      512
#define DEFAULT_DEADZONE    20
#define SAMPLE_COUNT        5

// グローバル変数
static JoystickState current_state = {0};
static JoystickState previous_state = {0};
static JoystickEvent last_event = JOYSTICK_EVENT_NONE;
static int deadzone = DEFAULT_DEADZONE;
static bool initialized = false;

// キャリブレーション値
static int16_t left_x_center = ADC_CENTER_VALUE;
static int16_t left_y_center = ADC_CENTER_VALUE;
static int16_t right_x_center = ADC_CENTER_VALUE;
static int16_t right_y_center = ADC_CENTER_VALUE;

// 内部関数プロトタイプ
static int16_t read_analog_averaged(int pin);
static int16_t map_analog_to_joystick(int16_t raw_value, int16_t center_value);
static void update_joystick_events();

bool joystick_init() {
  Serial.println("🎮 Initializing Joystick Input System...");
  
  // アナログピン設定
  pinMode(LEFT_STICK_X_PIN, INPUT);
  pinMode(LEFT_STICK_Y_PIN, INPUT);
  pinMode(RIGHT_STICK_X_PIN, INPUT);
  pinMode(RIGHT_STICK_Y_PIN, INPUT);
  
  // ボタンピン設定（内部プルアップ）
  pinMode(LEFT_STICK_BTN_PIN, INPUT_PULLUP);
  pinMode(RIGHT_STICK_BTN_PIN, INPUT_PULLUP);
  
  // ADC設定
  analogReadResolution(12); // 12bit ADC (0-4095)
  analogSetAttenuation(ADC_11db); // 3.3V full scale
  
  // 初期キャリブレーション（センター値読み取り）
  delay(100); // 安定化待ち
  
  int32_t left_x_sum = 0, left_y_sum = 0;
  int32_t right_x_sum = 0, right_y_sum = 0;
  
  for (int i = 0; i < 50; i++) {
    left_x_sum += analogRead(LEFT_STICK_X_PIN);
    left_y_sum += analogRead(LEFT_STICK_Y_PIN);
    right_x_sum += analogRead(RIGHT_STICK_X_PIN);
    right_y_sum += analogRead(RIGHT_STICK_Y_PIN);
    delay(10);
  }
  
  left_x_center = left_x_sum / 50;
  left_y_center = left_y_sum / 50;
  right_x_center = right_x_sum / 50;
  right_y_center = right_y_sum / 50;
  
  Serial.printf("✅ Joystick calibration completed\n");
  Serial.printf("   Left center: (%d, %d)\n", left_x_center, left_y_center);
  Serial.printf("   Right center: (%d, %d)\n", right_x_center, right_y_center);
  Serial.printf("   Deadzone: %d\n", deadzone);
  
  // 初期状態設定
  current_state.timestamp = millis();
  current_state.valid = true;
  
  initialized = true;
  return true;
}

void joystick_update() {
  if (!initialized) return;
  
  // 前回の状態を保存
  previous_state = current_state;
  
  // アナログ値読み取り（平均化）
  int16_t raw_left_x = read_analog_averaged(LEFT_STICK_X_PIN);
  int16_t raw_left_y = read_analog_averaged(LEFT_STICK_Y_PIN);
  int16_t raw_right_x = read_analog_averaged(RIGHT_STICK_X_PIN);
  int16_t raw_right_y = read_analog_averaged(RIGHT_STICK_Y_PIN);
  
  // Joystick座標に変換（-512 to +512）
  current_state.left_x = joystick_apply_deadzone(map_analog_to_joystick(raw_left_x, left_x_center), deadzone);
  current_state.left_y = joystick_apply_deadzone(map_analog_to_joystick(raw_left_y, left_y_center), deadzone);
  current_state.right_x = joystick_apply_deadzone(map_analog_to_joystick(raw_right_x, right_x_center), deadzone);
  current_state.right_y = joystick_apply_deadzone(map_analog_to_joystick(raw_right_y, right_y_center), deadzone);
  
  // ボタン状態読み取り（LOW=押下）
  current_state.left_pressed = !digitalRead(LEFT_STICK_BTN_PIN);
  current_state.right_pressed = !digitalRead(RIGHT_STICK_BTN_PIN);
  current_state.button_a = M5.BtnA.isPressed();
  current_state.button_b = M5.BtnB.isPressed();
  
  // タイムスタンプ更新
  current_state.timestamp = millis();
  current_state.valid = true;
  
  // イベント検出
  update_joystick_events();
}

static int16_t read_analog_averaged(int pin) {
  int32_t sum = 0;
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    sum += analogRead(pin);
  }
  return sum / SAMPLE_COUNT;
}

static int16_t map_analog_to_joystick(int16_t raw_value, int16_t center_value) {
  // センター値を基準に-512〜+512にマッピング
  int16_t offset = raw_value - center_value;
  
  if (offset > 0) {
    // 正の方向（センター→最大値）
    return map(offset, 0, ADC_MAX_VALUE - center_value, 0, JOYSTICK_RANGE);
  } else {
    // 負の方向（最小値→センター）
    return map(offset, center_value - 0, 0, -JOYSTICK_RANGE, 0);
  }
}

int16_t joystick_apply_deadzone(int16_t raw_value, int deadzone) {
  if (abs(raw_value) < deadzone) {
    return 0;
  }
  
  // デッドゾーン適用後の値を再マッピング
  if (raw_value > 0) {
    return map(raw_value, deadzone, JOYSTICK_RANGE, 1, JOYSTICK_RANGE);
  } else {
    return map(raw_value, -JOYSTICK_RANGE, -deadzone, -JOYSTICK_RANGE, -1);
  }
}

static void update_joystick_events() {
  last_event = JOYSTICK_EVENT_NONE;
  
  // スティック移動イベント
  if (current_state.left_x != previous_state.left_x || current_state.left_y != previous_state.left_y) {
    if (joystick_get_left_magnitude(&current_state) > 10) { // 最小移動量
      last_event = JOYSTICK_EVENT_LEFT_MOVE;
    }
  }
  
  if (current_state.right_x != previous_state.right_x || current_state.right_y != previous_state.right_y) {
    if (joystick_get_right_magnitude(&current_state) > 10) { // 最小移動量
      last_event = JOYSTICK_EVENT_RIGHT_MOVE;
    }
  }
  
  // ボタンイベント
  if (current_state.left_pressed && !previous_state.left_pressed) {
    last_event = JOYSTICK_EVENT_LEFT_CLICK;
  }
  
  if (current_state.right_pressed && !previous_state.right_pressed) {
    last_event = JOYSTICK_EVENT_RIGHT_CLICK;
  }
  
  if (current_state.button_a && !previous_state.button_a) {
    last_event = JOYSTICK_EVENT_BUTTON_A_PRESS;
  }
  
  if (!current_state.button_a && previous_state.button_a) {
    last_event = JOYSTICK_EVENT_BUTTON_A_RELEASE;
  }
  
  if (current_state.button_b && !previous_state.button_b) {
    last_event = JOYSTICK_EVENT_BUTTON_B_PRESS;
  }
  
  if (!current_state.button_b && previous_state.button_b) {
    last_event = JOYSTICK_EVENT_BUTTON_B_RELEASE;
  }
}

JoystickState joystick_get_state() {
  return current_state;
}

bool joystick_state_changed(const JoystickState* prev, const JoystickState* current) {
  if (prev == nullptr || current == nullptr) return false;
  
  return (prev->left_x != current->left_x ||
          prev->left_y != current->left_y ||
          prev->right_x != current->right_x ||
          prev->right_y != current->right_y ||
          prev->left_pressed != current->left_pressed ||
          prev->right_pressed != current->right_pressed ||
          prev->button_a != current->button_a ||
          prev->button_b != current->button_b);
}

JoystickEvent joystick_get_last_event() {
  return last_event;
}

String joystick_state_to_json(const JoystickState* state) {
  if (state == nullptr) return "{}";
  
  StaticJsonDocument<512> doc;
  doc["timestamp"] = state->timestamp;
  doc["valid"] = state->valid;
  
  JsonObject left = doc.createNestedObject("left");
  left["x"] = state->left_x;
  left["y"] = state->left_y;
  left["pressed"] = state->left_pressed;
  left["magnitude"] = joystick_get_left_magnitude(state);
  left["angle"] = joystick_get_left_angle(state);
  
  JsonObject right = doc.createNestedObject("right");
  right["x"] = state->right_x;
  right["y"] = state->right_y;
  right["pressed"] = state->right_pressed;
  right["magnitude"] = joystick_get_right_magnitude(state);
  right["angle"] = joystick_get_right_angle(state);
  
  JsonObject buttons = doc.createNestedObject("buttons");
  buttons["a"] = state->button_a;
  buttons["b"] = state->button_b;
  
  String result;
  serializeJson(doc, result);
  return result;
}

float joystick_get_left_magnitude(const JoystickState* state) {
  if (state == nullptr) return 0.0;
  return sqrt(state->left_x * state->left_x + state->left_y * state->left_y);
}

float joystick_get_right_magnitude(const JoystickState* state) {
  if (state == nullptr) return 0.0;
  return sqrt(state->right_x * state->right_x + state->right_y * state->right_y);
}

float joystick_get_left_angle(const JoystickState* state) {
  if (state == nullptr) return 0.0;
  return atan2(state->left_y, state->left_x) * 180.0 / PI;
}

float joystick_get_right_angle(const JoystickState* state) {
  if (state == nullptr) return 0.0;
  return atan2(state->right_y, state->right_x) * 180.0 / PI;
}

bool joystick_is_center_position(const JoystickState* state) {
  if (state == nullptr) return true;
  return (abs(state->left_x) <= deadzone && abs(state->left_y) <= deadzone &&
          abs(state->right_x) <= deadzone && abs(state->right_y) <= deadzone);
}

void joystick_set_deadzone(int new_deadzone) {
  deadzone = new_deadzone;
  Serial.printf("🎮 Joystick deadzone set to: %d\n", deadzone);
}

void joystick_calibrate() {
  Serial.println("🎮 Starting joystick calibration...");
  Serial.println("   Please center both joysticks and wait...");
  
  delay(3000); // 3秒待機
  
  int32_t left_x_sum = 0, left_y_sum = 0;
  int32_t right_x_sum = 0, right_y_sum = 0;
  
  for (int i = 0; i < 100; i++) {
    left_x_sum += analogRead(LEFT_STICK_X_PIN);
    left_y_sum += analogRead(LEFT_STICK_Y_PIN);
    right_x_sum += analogRead(RIGHT_STICK_X_PIN);
    right_y_sum += analogRead(RIGHT_STICK_Y_PIN);
    delay(20);
  }
  
  left_x_center = left_x_sum / 100;
  left_y_center = left_y_sum / 100;
  right_x_center = right_x_sum / 100;
  right_y_center = right_y_sum / 100;
  
  Serial.println("✅ Calibration completed");
  Serial.printf("   New left center: (%d, %d)\n", left_x_center, left_y_center);
  Serial.printf("   New right center: (%d, %d)\n", right_x_center, right_y_center);
}