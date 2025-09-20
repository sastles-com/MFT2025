/**
 * @file JoystickMQTTManager.h
 * @brief Atom-JoyStick MQTT統合管理システム
 * @description 項目別Retain配信・軽量MQTTブローカー統合
 * 
 * 機能:
 * - EmbeddedMqttBroker統合・WiFi AP同時動作
 * - 項目別Topic配信・Retain機能
 * - UI状態→MQTT配信統合システム
 * - 変更検出・効率配信・統計管理
 * 
 * @target M5Stack Atom-JoyStick (ESP32-S3)
 * @author Claude Code Assistant  
 * @date 2025年9月4日
 */

#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <EmbeddedMqttBroker.h>
#include "JoystickConfig.h"

using namespace mqttBrokerName;

// ========== MQTT設定定数 ==========
#define MQTT_DEFAULT_PORT 1884  // config.json準拠
#define MQTT_MAX_CLIENTS 8
#define MQTT_KEEPALIVE_SEC 60
#define MQTT_MAX_TOPIC_LENGTH 100
#define MQTT_MAX_PAYLOAD_LENGTH 50

// ========== Topic定義 ==========
// Control モード
#define TOPIC_CONTROL_BRIGHTNESS    "control/brightness"
#define TOPIC_CONTROL_COLOR_TEMP    "control/color_temp" 
#define TOPIC_CONTROL_PLAYBACK      "control/playback"
#define TOPIC_CONTROL_ROTATION_X    "control/rotation_x"
#define TOPIC_CONTROL_ROTATION_Y    "control/rotation_y"

// Video モード
#define TOPIC_VIDEO_SELECTED_ID     "video/selected_id"
#define TOPIC_VIDEO_VOLUME          "video/volume"
#define TOPIC_VIDEO_SEEK_POSITION   "video/seek_position"
#define TOPIC_VIDEO_PLAYBACK_SPEED  "video/playback_speed"

// Adjust モード  
#define TOPIC_ADJUST_SELECTED_PARAM "adjust/selected_param"
#define TOPIC_ADJUST_PARAM_0        "adjust/param_0"
#define TOPIC_ADJUST_PARAM_1        "adjust/param_1"
#define TOPIC_ADJUST_PARAM_2        "adjust/param_2"
#define TOPIC_ADJUST_PARAM_3        "adjust/param_3"
#define TOPIC_ADJUST_PARAM_4        "adjust/param_4"

// System モード
#define TOPIC_SYSTEM_CURRENT_MODE   "system/current_mode"
#define TOPIC_SYSTEM_WIFI_CLIENTS   "system/wifi_clients"
#define TOPIC_SYSTEM_CPU_TEMP       "system/cpu_temp"
#define TOPIC_SYSTEM_UPTIME         "system/uptime"

/**
 * @brief MQTT統計情報
 */
struct MQTTStats {
  unsigned long total_messages_published;  // 総配信メッセージ数
  unsigned long total_messages_received;   // 総受信メッセージ数
  unsigned long total_clients_connected;   // 総接続クライアント数
  unsigned long total_connection_errors;   // 総接続エラー数
  unsigned long last_publish_time;         // 最終配信時刻
  unsigned long broker_start_time;         // ブローカー開始時刻
  int current_connected_clients;           // 現在接続中クライアント数
  
  // 統計リセット
  void reset() {
    total_messages_published = 0;
    total_messages_received = 0;
    total_clients_connected = 0;
    total_connection_errors = 0;
    last_publish_time = 0;
    broker_start_time = millis();
    current_connected_clients = 0;
  }
};

/**
 * @brief UI状態管理（前回値との比較用）
 */
struct UIStateCache {
  // Control モード
  int brightness = -1;
  int color_temp = -1;  
  bool playback_playing = false;
  float rotation_x = 0.0f;
  float rotation_y = 0.0f;
  
  // Video モード
  int selected_video_id = -1;
  int volume = -1;
  int seek_position = -1;
  float playback_speed = -1.0f;
  
  // Adjust モード
  int selected_parameter = -1;
  int parameter_values[5] = {-1, -1, -1, -1, -1};
  
  // System モード
  String current_mode = "";
  int wifi_clients = -1;
  float cpu_temp = -1.0f;
  unsigned long uptime = 0;
};

/**
 * @brief Joystick MQTT統合管理クラス
 */
class JoystickMQTTManager {
public:
  JoystickMQTTManager();
  ~JoystickMQTTManager();
  
  // 初期化・終了
  bool begin(const JoystickConfig& config);
  void end();
  bool isRunning() const { return mqtt_broker_running_; }
  
  // Control モード配信
  void publishBrightness(int brightness);
  void publishColorTemp(int color_temp);
  void publishPlayback(bool playing);
  void publishRotationX(float rotation_x);
  void publishRotationY(float rotation_y);
  
  // Video モード配信
  void publishSelectedVideoId(int video_id);
  void publishVolume(int volume);
  void publishSeekPosition(int position);
  void publishPlaybackSpeed(float speed);
  
  // Adjust モード配信
  void publishSelectedParameter(int param_index);
  void publishParameterValue(int param_index, int value);
  
  // System モード配信
  void publishCurrentMode(const String& mode);
  void publishWiFiClients(int client_count);
  void publishCPUTemp(float temperature);
  void publishUptime(unsigned long uptime_ms);
  
  // システム管理
  void update();
  int getConnectedClientsCount();
  void printStats() const;
  void resetStats();
  
  // コールバック設定
  void setMessageCallback(void (*callback)(const char* topic, const char* payload));
  
  const MQTTStats& getStats() const { return stats_; }

private:
  // MQTTブローカー
  mqttBrokerName::MqttBroker* mqtt_broker_;
  bool mqtt_broker_running_;
  int mqtt_port_;
  
  // 統計・状態管理
  MQTTStats stats_;
  UIStateCache ui_cache_;
  
  // コールバック
  void (*message_callback_)(const char* topic, const char* payload);
  
  // 内部制御メソッド
  bool publishWithRetain(const char* topic, const char* payload);
  bool publishIntValue(const char* topic, int value, int& cache_value);
  bool publishFloatValue(const char* topic, float value, float& cache_value);
  bool publishBoolValue(const char* topic, bool value, bool& cache_value);
  bool publishStringValue(const char* topic, const String& value, String& cache_value);
  
  void updateStats(bool publish_success);
  void logPublish(const char* topic, const char* payload, bool success);
  void printError(const char* message, const char* detail = nullptr);
  
  // MQTTブローカーコールバック（静的）
  static void onMqttMessage(const char* topic, const char* payload, size_t length);
  static JoystickMQTTManager* instance_; // シングルトンインスタンス
};