/*
 * Joystick入力処理
 * isolation-sphere分散MQTT制御システム
 */

#ifndef JOYSTICK_INPUT_H
#define JOYSTICK_INPUT_H

#include <stdint.h>
#include <stdbool.h>

// Joystick状態構造体
struct JoystickState {
  // 左スティック
  int16_t left_x;    // -512 to +512
  int16_t left_y;    // -512 to +512
  bool left_pressed; // スティック押し込み
  
  // 右スティック  
  int16_t right_x;   // -512 to +512
  int16_t right_y;   // -512 to +512
  bool right_pressed; // スティック押し込み
  
  // ボタン
  bool button_a;
  bool button_b;
  
  // システム
  unsigned long timestamp;
  bool valid;
};

// 入力イベント種別
enum JoystickEvent {
  JOYSTICK_EVENT_NONE,
  JOYSTICK_EVENT_LEFT_MOVE,
  JOYSTICK_EVENT_RIGHT_MOVE,
  JOYSTICK_EVENT_LEFT_CLICK,
  JOYSTICK_EVENT_RIGHT_CLICK,
  JOYSTICK_EVENT_BUTTON_A_PRESS,
  JOYSTICK_EVENT_BUTTON_B_PRESS,
  JOYSTICK_EVENT_BUTTON_A_RELEASE,
  JOYSTICK_EVENT_BUTTON_B_RELEASE
};

// 関数プロトタイプ
bool joystick_init();
void joystick_update();
JoystickState joystick_get_state();
bool joystick_state_changed(const JoystickState* prev, const JoystickState* current);
JoystickEvent joystick_get_last_event();
void joystick_calibrate();
String joystick_state_to_json(const JoystickState* state);
void joystick_set_deadzone(int deadzone);

// ユーティリティ関数
int16_t joystick_apply_deadzone(int16_t raw_value, int deadzone);
bool joystick_is_center_position(const JoystickState* state);
float joystick_get_left_magnitude(const JoystickState* state);
float joystick_get_right_magnitude(const JoystickState* state);
float joystick_get_left_angle(const JoystickState* state);
float joystick_get_right_angle(const JoystickState* state);

#endif // JOYSTICK_INPUT_H