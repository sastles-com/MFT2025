/**
 * @file udp_receiver.h
 * @brief UDP受信・JSON解析システム
 * @description Joystickからのポート1884 UDP受信・解析
 */

#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "config_manager.h"

/**
 * @brief Joystickデータ構造体
 */
struct JoystickData {
  // アナログスティック値 (-1.0 ~ 1.0)
  float left_x;
  float left_y;
  float right_x;
  float right_y;
  
  // ボタン状態
  bool left_stick_button;   // 左スティック押し込み
  bool right_stick_button;  // 右スティック押し込み
  bool button_left;         // 左ボタン（L）
  bool button_right;        // 右ボタン（R）
  
  // システム情報
  float battery;
  unsigned long timestamp;
  
  // データ有効フラグ
  bool valid;
  
  JoystickData() : left_x(0), left_y(0), right_x(0), right_y(0)
                 , left_stick_button(false), right_stick_button(false)
                 , button_left(false), button_right(false)
                 , battery(0), timestamp(0), valid(false) {}
};

/**
 * @brief UDP受信システム統計
 */
struct UDPReceiveStats {
  unsigned long packets_received;
  unsigned long packets_dropped;
  unsigned long json_parse_errors;
  unsigned long last_receive_time;
  float avg_packet_size;
  float packet_loss_rate;
};

/**
 * @brief UDP受信・解析クラス
 */
class UDPReceiver {
public:
  UDPReceiver();
  ~UDPReceiver();
  
  // 初期化・終了処理
  bool begin(const ConfigManager& config);
  void end();
  
  // データ受信
  bool receiveData(JoystickData& data);
  bool isDataAvailable();
  
  // 統計情報
  const UDPReceiveStats& getStats() const { return stats_; }
  void printStats() const;
  void resetStats();
  
  // 設定変更
  bool changePort(int new_port);
  bool setTimeout(int timeout_ms);

private:
  WiFiUDP udp_;
  ConfigManager config_;
  bool initialized_;
  
  // 受信バッファ
  static const size_t RECEIVE_BUFFER_SIZE = 512;
  static const size_t JSON_BUFFER_SIZE = 1024;
  char receive_buffer_[RECEIVE_BUFFER_SIZE];
  
  // 統計情報
  UDPReceiveStats stats_;
  
  // 内部処理メソッド
  bool parseJoystickJson(const char* json_str, JoystickData& data);
  bool validateJoystickData(const JoystickData& data) const;
  void updateStats(size_t packet_size, bool parse_success);
  float calculatePacketLossRate() const;
  
  // デバッグ用
  void printRawData(const char* data, size_t length) const;
  void printParsedData(const JoystickData& data) const;
};