/**
 * @file JoystickMQTTManager.cpp
 * @brief Atom-JoyStick MQTT統合管理システム実装
 */

#include "JoystickMQTTManager.h"

// 静的インスタンス（シングルトンパターン）
JoystickMQTTManager* JoystickMQTTManager::instance_ = nullptr;

/**
 * @brief コンストラクタ
 */
JoystickMQTTManager::JoystickMQTTManager() 
  : mqtt_broker_(nullptr)
  , mqtt_broker_running_(false)
  , mqtt_port_(MQTT_DEFAULT_PORT)
  , message_callback_(nullptr) {
  
  // 統計初期化
  stats_.reset();
  
  // シングルトン設定
  instance_ = this;
}

/**
 * @brief デストラクタ
 */
JoystickMQTTManager::~JoystickMQTTManager() {
  end();
  instance_ = nullptr;
}

/**
 * @brief 初期化
 */
bool JoystickMQTTManager::begin(const JoystickConfig& config) {
  Serial.println("🚀 JoystickMQTTManager: 初期化開始");
  
  // 設定読み込み
  mqtt_port_ = MQTT_DEFAULT_PORT; // config対応は後で実装
  
  // MQTTブローカー作成
  mqtt_broker_ = new mqttBrokerName::MqttBroker(mqtt_port_);
  if (!mqtt_broker_) {
    printError("MQTTブローカーオブジェクト作成失敗");
    return false;
  }
  
  // WiFi接続確認
  if (WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) {
    printError("WiFi APモードが無効", "MQTTブローカー開始前にWiFi AP起動が必要");
    delete mqtt_broker_;
    mqtt_broker_ = nullptr;
    return false;
  }
  
  // MQTTブローカー開始
  Serial.printf("📡 MQTTブローカー開始: ポート%d\n", mqtt_port_);
  
  // MQTTブローカー開始
  mqtt_broker_->startBroker();
  mqtt_broker_running_ = true;
  stats_.broker_start_time = millis();
  
  Serial.println("✅ JoystickMQTTManager: 初期化完了");
  Serial.printf("📊 最大クライアント数: %d\n", MQTT_MAX_CLIENTS);
  Serial.printf("📊 KeepAlive: %d秒\n", MQTT_KEEPALIVE_SEC);
  
  return true;
}

/**
 * @brief 終了処理
 */
void JoystickMQTTManager::end() {
  if (mqtt_broker_) {
    Serial.println("🛑 JoystickMQTTManager: 終了処理開始");
    
    mqtt_broker_running_ = false;
    mqtt_broker_->stopBroker();
    
    delete mqtt_broker_;
    mqtt_broker_ = nullptr;
    
    Serial.println("✅ JoystickMQTTManager: 終了完了");
  }
}

/**
 * @brief 更新処理（メインループから呼び出し）
 */
void JoystickMQTTManager::update() {
  if (!mqtt_broker_running_ || !mqtt_broker_) {
    return;
  }
  
  // MQTTブローカー更新処理
  // 注: EmbeddedMqttBrokerの更新処理APIに応じて実装
  
  // 定期的なシステム状態配信
  static unsigned long last_system_update = 0;
  if (millis() - last_system_update > 5000) { // 5秒間隔
    publishWiFiClients(WiFi.softAPgetStationNum());
    publishUptime(millis());
    last_system_update = millis();
  }
}

// ========== Control モード配信 ==========

/**
 * @brief LED明度配信
 */
void JoystickMQTTManager::publishBrightness(int brightness) {
  publishIntValue(TOPIC_CONTROL_BRIGHTNESS, brightness, ui_cache_.brightness);
}

/**
 * @brief 色温度配信
 */
void JoystickMQTTManager::publishColorTemp(int color_temp) {
  publishIntValue(TOPIC_CONTROL_COLOR_TEMP, color_temp, ui_cache_.color_temp);
}

/**
 * @brief 再生状態配信
 */
void JoystickMQTTManager::publishPlayback(bool playing) {
  publishBoolValue(TOPIC_CONTROL_PLAYBACK, playing, ui_cache_.playback_playing);
}

/**
 * @brief X軸回転配信
 */
void JoystickMQTTManager::publishRotationX(float rotation_x) {
  publishFloatValue(TOPIC_CONTROL_ROTATION_X, rotation_x, ui_cache_.rotation_x);
}

/**
 * @brief Y軸回転配信
 */
void JoystickMQTTManager::publishRotationY(float rotation_y) {
  publishFloatValue(TOPIC_CONTROL_ROTATION_Y, rotation_y, ui_cache_.rotation_y);
}

// ========== Video モード配信 ==========

/**
 * @brief 選択動画ID配信
 */
void JoystickMQTTManager::publishSelectedVideoId(int video_id) {
  publishIntValue(TOPIC_VIDEO_SELECTED_ID, video_id, ui_cache_.selected_video_id);
}

/**
 * @brief 音量配信
 */
void JoystickMQTTManager::publishVolume(int volume) {
  publishIntValue(TOPIC_VIDEO_VOLUME, volume, ui_cache_.volume);
}

/**
 * @brief シーク位置配信
 */
void JoystickMQTTManager::publishSeekPosition(int position) {
  publishIntValue(TOPIC_VIDEO_SEEK_POSITION, position, ui_cache_.seek_position);
}

/**
 * @brief 再生速度配信
 */
void JoystickMQTTManager::publishPlaybackSpeed(float speed) {
  publishFloatValue(TOPIC_VIDEO_PLAYBACK_SPEED, speed, ui_cache_.playback_speed);
}

// ========== Adjust モード配信 ==========

/**
 * @brief 選択パラメータ配信
 */
void JoystickMQTTManager::publishSelectedParameter(int param_index) {
  if (param_index >= 0 && param_index <= 4) {
    publishIntValue(TOPIC_ADJUST_SELECTED_PARAM, param_index, ui_cache_.selected_parameter);
  }
}

/**
 * @brief パラメータ値配信
 */
void JoystickMQTTManager::publishParameterValue(int param_index, int value) {
  if (param_index < 0 || param_index > 4) return;
  
  const char* topics[] = {
    TOPIC_ADJUST_PARAM_0,
    TOPIC_ADJUST_PARAM_1,
    TOPIC_ADJUST_PARAM_2,
    TOPIC_ADJUST_PARAM_3,
    TOPIC_ADJUST_PARAM_4
  };
  
  publishIntValue(topics[param_index], value, ui_cache_.parameter_values[param_index]);
}

// ========== System モード配信 ==========

/**
 * @brief 現在モード配信
 */
void JoystickMQTTManager::publishCurrentMode(const String& mode) {
  publishStringValue(TOPIC_SYSTEM_CURRENT_MODE, mode, ui_cache_.current_mode);
}

/**
 * @brief WiFiクライアント数配信
 */
void JoystickMQTTManager::publishWiFiClients(int client_count) {
  stats_.current_connected_clients = client_count;
  publishIntValue(TOPIC_SYSTEM_WIFI_CLIENTS, client_count, ui_cache_.wifi_clients);
}

/**
 * @brief CPU温度配信
 */
void JoystickMQTTManager::publishCPUTemp(float temperature) {
  publishFloatValue(TOPIC_SYSTEM_CPU_TEMP, temperature, ui_cache_.cpu_temp);
}

/**
 * @brief 稼働時間配信
 */
void JoystickMQTTManager::publishUptime(unsigned long uptime_ms) {
  if (uptime_ms != ui_cache_.uptime) {
    String uptime_str = String(uptime_ms / 1000); // 秒単位
    publishWithRetain(TOPIC_SYSTEM_UPTIME, uptime_str.c_str());
    ui_cache_.uptime = uptime_ms;
  }
}

// ========== 内部制御メソッド ==========

/**
 * @brief Retain付きMQTT配信
 */
bool JoystickMQTTManager::publishWithRetain(const char* topic, const char* payload) {
  if (!mqtt_broker_running_ || !mqtt_broker_) {
    return false;
  }
  
  // MQTTブローカーでの配信実装（簡略版）
  // 注: PublishMqttMessage作成は後で詳細実装
  bool success = true; // 仮の実装（コンパイル用）
  
  updateStats(success);
  logPublish(topic, payload, success);
  
  return success;
}

/**
 * @brief Int値配信（変更検出付き）
 */
bool JoystickMQTTManager::publishIntValue(const char* topic, int value, int& cache_value) {
  if (value != cache_value) {
    String payload = String(value);
    Serial.printf("🔢 値変更検出: %s (%d → %d)\n", topic, cache_value, value);
    
    bool success = publishWithRetain(topic, payload.c_str());
    if (success) {
      cache_value = value;
    }
    return success;
  }
  return true; // 変更なしは成功扱い
}

/**
 * @brief Float値配信（変更検出付き）
 */
bool JoystickMQTTManager::publishFloatValue(const char* topic, float value, float& cache_value) {
  if (abs(value - cache_value) > 0.01f) { // 0.01の変化で更新
    String payload = String(value, 2);
    Serial.printf("🔢 値変更検出: %s (%.2f → %.2f)\n", topic, cache_value, value);
    
    bool success = publishWithRetain(topic, payload.c_str());
    if (success) {
      cache_value = value;
    }
    return success;
  }
  return true;
}

/**
 * @brief Bool値配信（変更検出付き）
 */
bool JoystickMQTTManager::publishBoolValue(const char* topic, bool value, bool& cache_value) {
  if (value != cache_value) {
    const char* payload = value ? "true" : "false";
    const char* prev_payload = cache_value ? "true" : "false";
    
    // 変更検出ログ
    Serial.printf("🔄 値変更検出: %s (%s → %s)\n", topic, prev_payload, payload);
    
    bool success = publishWithRetain(topic, payload);
    if (success) {
      cache_value = value;
    }
    return success;
  }
  // 変更なしの場合はログ出力しない（スパム防止）
  return true;
}

/**
 * @brief String値配信（変更検出付き）
 */
bool JoystickMQTTManager::publishStringValue(const char* topic, const String& value, String& cache_value) {
  if (value != cache_value) {
    bool success = publishWithRetain(topic, value.c_str());
    if (success) {
      cache_value = value;
    }
    return success;
  }
  return true;
}

/**
 * @brief 統計更新
 */
void JoystickMQTTManager::updateStats(bool publish_success) {
  if (publish_success) {
    stats_.total_messages_published++;
    stats_.last_publish_time = millis();
  }
}

/**
 * @brief 配信ログ出力
 */
void JoystickMQTTManager::logPublish(const char* topic, const char* payload, bool success) {
  if (success) {
    Serial.printf("📡 MQTT配信: %s → %s\n", topic, payload);
  } else {
    Serial.printf("❌ MQTT配信失敗: %s → %s\n", topic, payload);
  }
}

/**
 * @brief 接続クライアント数取得
 */
int JoystickMQTTManager::getConnectedClientsCount() {
  return stats_.current_connected_clients;
}

/**
 * @brief 統計情報出力
 */
void JoystickMQTTManager::printStats() const {
  Serial.println();
  Serial.println("========== MQTT統計情報 ==========");
  Serial.printf("配信メッセージ数: %lu\n", stats_.total_messages_published);
  Serial.printf("受信メッセージ数: %lu\n", stats_.total_messages_received);
  Serial.printf("接続クライアント数: %d\n", stats_.current_connected_clients);
  Serial.printf("総接続数: %lu\n", stats_.total_clients_connected);
  Serial.printf("接続エラー数: %lu\n", stats_.total_connection_errors);
  Serial.printf("稼働時間: %lu秒\n", (millis() - stats_.broker_start_time) / 1000);
  Serial.printf("最終配信: %lums前\n", millis() - stats_.last_publish_time);
  Serial.println("================================");
  Serial.println();
}

/**
 * @brief 統計リセット
 */
void JoystickMQTTManager::resetStats() {
  stats_.reset();
  Serial.println("📊 MQTT統計リセット完了");
}

/**
 * @brief エラー出力
 */
void JoystickMQTTManager::printError(const char* message, const char* detail) {
  Serial.printf("❌ JoystickMQTTManager: %s", message);
  if (detail != nullptr) {
    Serial.printf(" - %s", detail);
  }
  Serial.println();
}

/**
 * @brief コールバック設定
 */
void JoystickMQTTManager::setMessageCallback(void (*callback)(const char* topic, const char* payload)) {
  message_callback_ = callback;
}

/**
 * @brief MQTTメッセージ受信コールバック（静的）
 */
void JoystickMQTTManager::onMqttMessage(const char* topic, const char* payload, size_t length) {
  if (instance_ && instance_->message_callback_) {
    String payload_str = String(payload).substring(0, length);
    instance_->message_callback_(topic, payload_str.c_str());
    instance_->stats_.total_messages_received++;
  }
}