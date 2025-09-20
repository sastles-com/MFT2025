/**
 * @file udp_receiver.cpp
 * @brief UDP受信・JSON解析実装
 */

#include "udp_receiver.h"

/**
 * @brief コンストラクタ
 */
UDPReceiver::UDPReceiver() 
  : initialized_(false)
  , stats_({0, 0, 0, 0, 0.0, 0.0}) {
}

/**
 * @brief デストラクタ
 */
UDPReceiver::~UDPReceiver() {
  end();
}

/**
 * @brief UDP受信システム初期化
 */
bool UDPReceiver::begin(const ConfigManager& config) {
  config_ = config;
  
  Serial.println("UDPReceiver: 初期化開始");
  
  int udp_port = config_.getUDPPort();
  
  // UDP開始
  if (!udp_.begin(udp_port)) {
    Serial.printf("❌ UDPReceiver: ポート%d開始失敗\n", udp_port);
    return false;
  }
  
  Serial.printf("✅ UDPReceiver: ポート%d開始成功\n", udp_port);
  
  // 統計初期化
  resetStats();
  initialized_ = true;
  
  return true;
}

/**
 * @brief 終了処理
 */
void UDPReceiver::end() {
  if (initialized_) {
    udp_.stop();
    initialized_ = false;
    Serial.println("UDPReceiver: 終了完了");
  }
}

/**
 * @brief データ受信・解析
 */
bool UDPReceiver::receiveData(JoystickData& data) {
  if (!initialized_) {
    return false;
  }
  
  // パケット受信確認
  int packet_size = udp_.parsePacket();
  if (packet_size == 0) {
    return false; // データなし
  }
  
  // バッファサイズ確認
  if (packet_size >= RECEIVE_BUFFER_SIZE) {
    Serial.printf("⚠️  UDPReceiver: パケットサイズ過大 (%d bytes)\n", packet_size);
    udp_.flush(); // バッファクリア
    stats_.packets_dropped++;
    return false;
  }
  
  // データ読み込み
  int bytes_read = udp_.read(receive_buffer_, packet_size);
  receive_buffer_[bytes_read] = '\0'; // NULL終端
  
  // デバッグ出力
  if (config_.getDebugConfig().serial_output) {
    Serial.println("██ 📥📥 UDP受信成功 📥📥");
    Serial.printf("██   サイズ: %d bytes\n", bytes_read);
    Serial.printf("██   送信元: %s:%d\n", udp_.remoteIP().toString().c_str(), udp_.remotePort());
  }
  
  // JSON解析
  bool parse_success = parseJoystickJson(receive_buffer_, data);
  
  // 統計更新
  updateStats(bytes_read, parse_success);
  
  if (parse_success && validateJoystickData(data)) {
    data.valid = true;
    stats_.last_receive_time = millis();
    return true;
  } else {
    data.valid = false;
    return false;
  }
}

/**
 * @brief データ利用可能性確認
 */
bool UDPReceiver::isDataAvailable() {
  if (!initialized_) {
    return false;
  }
  return udp_.parsePacket() > 0;
}

/**
 * @brief JSON解析処理
 */
bool UDPReceiver::parseJoystickJson(const char* json_str, JoystickData& data) {
  StaticJsonDocument<JSON_BUFFER_SIZE> doc;
  
  // JSON解析
  DeserializationError error = deserializeJson(doc, json_str);
  if (error) {
    Serial.printf("❌ JSON解析失敗: %s\n", error.c_str());
    if (config_.getDebugConfig().serial_output) {
      printRawData(json_str, strlen(json_str));
    }
    stats_.json_parse_errors++;
    return false;
  }
  
  // データ抽出・正規化（raw値 → -1.0~1.0範囲）
  float raw_left_x = doc["left"]["x"] | 2048.0f;
  float raw_left_y = doc["left"]["y"] | 2048.0f;
  float raw_right_x = doc["right"]["x"] | 2048.0f;
  float raw_right_y = doc["right"]["y"] | 2048.0f;
  
  // Atom-JoyStick正規化（0-4095範囲 → -1.0~1.0）
  data.left_x = (raw_left_x - 2048.0f) / 2048.0f;
  data.left_y = (raw_left_y - 2048.0f) / 2048.0f;
  data.right_x = (raw_right_x - 2048.0f) / 2048.0f;
  data.right_y = (raw_right_y - 2048.0f) / 2048.0f;
  
  // スティック押し込みボタン
  data.left_stick_button = doc["left"]["button"] | false;
  data.right_stick_button = doc["right"]["button"] | false;
  
  // L/Rボタン
  data.button_left = doc["buttons"]["left"] | false;
  data.button_right = doc["buttons"]["right"] | false;
  
  data.battery = doc["battery"] | 0.0f;
  data.timestamp = doc["timestamp"] | 0UL;
  
  // デバッグ出力
  if (config_.getDebugConfig().serial_output) {
    printParsedData(data);
  }
  
  return true;
}

/**
 * @brief Joystickデータ検証
 */
bool UDPReceiver::validateJoystickData(const JoystickData& data) const {
  // スティック値範囲確認 (-1.0 ~ 1.0)
  if (data.left_x < -1.0f || data.left_x > 1.0f ||
      data.left_y < -1.0f || data.left_y > 1.0f ||
      data.right_x < -1.0f || data.right_x > 1.0f ||
      data.right_y < -1.0f || data.right_y > 1.0f) {
    Serial.println("❌ Joystick値範囲エラー");
    return false;
  }
  
  // バッテリー値確認 (0.0 ~ 5.0V程度)
  if (data.battery < 0.0f || data.battery > 6.0f) {
    Serial.printf("⚠️  バッテリー値異常: %.2fV\n", data.battery);
    // 警告のみ、検証は通す
  }
  
  // タイムスタンプ確認
  unsigned long now = millis();
  if (data.timestamp > now + 1000) { // 1秒先は異常
    // タイムスタンプログ削除（デバイス間時刻差のため無意味）
    // 警告のみ、検証は通す
  }
  
  return true;
}

/**
 * @brief 統計情報更新
 */
void UDPReceiver::updateStats(size_t packet_size, bool parse_success) {
  if (parse_success) {
    stats_.packets_received++;
  } else {
    stats_.packets_dropped++;
  }
  
  // 平均パケットサイズ更新
  static const int AVERAGE_SAMPLES = 10;
  static float size_samples[AVERAGE_SAMPLES] = {0};
  static int sample_index = 0;
  
  size_samples[sample_index] = (float)packet_size;
  sample_index = (sample_index + 1) % AVERAGE_SAMPLES;
  
  float sum = 0;
  for (int i = 0; i < AVERAGE_SAMPLES; i++) {
    sum += size_samples[i];
  }
  stats_.avg_packet_size = sum / AVERAGE_SAMPLES;
  
  // パケットロス率計算
  stats_.packet_loss_rate = calculatePacketLossRate();
}

/**
 * @brief パケットロス率計算
 */
float UDPReceiver::calculatePacketLossRate() const {
  unsigned long total_packets = stats_.packets_received + stats_.packets_dropped;
  if (total_packets == 0) {
    return 0.0f;
  }
  return (float)stats_.packets_dropped / total_packets * 100.0f;
}

/**
 * @brief 統計情報出力
 */
void UDPReceiver::printStats() const {
  Serial.println("\n========== UDP受信統計 ==========");
  Serial.printf("受信パケット: %lu\n", stats_.packets_received);
  Serial.printf("ドロップパケット: %lu\n", stats_.packets_dropped);
  Serial.printf("JSON解析エラー: %lu\n", stats_.json_parse_errors);
  Serial.printf("平均パケットサイズ: %.1f bytes\n", stats_.avg_packet_size);
  Serial.printf("パケットロス率: %.2f%%\n", stats_.packet_loss_rate);
  
  if (stats_.last_receive_time > 0) {
    unsigned long since_last = millis() - stats_.last_receive_time;
    Serial.printf("最終受信: %lu秒前\n", since_last / 1000);
  }
  
  Serial.println("==================================\n");
}

/**
 * @brief 統計リセット
 */
void UDPReceiver::resetStats() {
  stats_.packets_received = 0;
  stats_.packets_dropped = 0;
  stats_.json_parse_errors = 0;
  stats_.last_receive_time = 0;
  stats_.avg_packet_size = 0.0f;
  stats_.packet_loss_rate = 0.0f;
  
  Serial.println("UDPReceiver: 統計リセット完了");
}

/**
 * @brief 生データ出力（デバッグ用）
 */
void UDPReceiver::printRawData(const char* data, size_t length) const {
  Serial.printf("Raw UDP Data (%d bytes): ", length);
  for (size_t i = 0; i < length; i++) {
    if (isprint(data[i])) {
      Serial.print(data[i]);
    } else {
      Serial.printf("\\x%02X", (unsigned char)data[i]);
    }
  }
  Serial.println();
}

/**
 * @brief 解析済みデータ出力（デバッグ用）
 */
void UDPReceiver::printParsedData(const JoystickData& data) const {
  Serial.println("██ 🎮🎮 Joystick データ解析成功 🎮🎮");
  Serial.printf("██   左スティック: (%.2f, %.2f) 押込:%s\n", 
                data.left_x, data.left_y, data.left_stick_button ? "🔴" : "⚪");
  Serial.printf("██   右スティック: (%.2f, %.2f) 押込:%s\n", 
                data.right_x, data.right_y, data.right_stick_button ? "🔴" : "⚪");
  Serial.printf("██   ボタン: L:%s R:%s\n", 
                data.button_left ? "🔴" : "⚪", 
                data.button_right ? "🔴" : "⚪");
  Serial.printf("██   バッテリー: %.1fV | タイムスタンプ: %lu\n", data.battery, data.timestamp);
}