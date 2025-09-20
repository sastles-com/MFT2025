/**
 * @file config_manager.h
 * @brief 設定管理システム - Joystickと共通仕様
 * @description SPIFFS config.jsonの読み込み・管理
 */

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

/**
 * @brief WiFi設定構造体
 */
struct WiFiConfig {
  String ssid;
  String password;
  String mode;          // "client" or "ap"
  String static_ip;     // 静的IP（空文字列の場合はDHCP）
  String joystick_ssid; // Joystick接続先SSID
};

/**
 * @brief 通信設定構造体
 */
struct CommunicationConfig {
  int udp_port;         // UDP受信ポート
  String joystick_ip;   // Joystick送信元IP
  int response_timeout; // 受信タイムアウト(ms)
};

/**
 * @brief LED設定構造体
 */
struct LEDConfig {
  int pin;              // LED制御ピン
  int count;            // LED個数
  int brightness;       // 明度 (0-255)
  int update_rate;      // 更新レート(Hz)
};

/**
 * @brief デバッグ設定構造体
 */
struct DebugConfig {
  bool serial_output;   // シリアル出力有効
  bool performance_monitor; // 性能監視有効
};

/**
 * @brief オープニング設定構造体
 */
struct OpeningConfig {
  bool enabled;                // オープニング有効/無効
  int frame_duration_ms;       // 1フレーム表示時間（ミリ秒）
  int brightness;              // LCD明度 (0-255)
  bool fade_effect;            // フェード効果有効/無効
  int fade_steps;              // フェードステップ数
};

/**
 * @brief 設定管理クラス
 */
class ConfigManager {
public:
  ConfigManager();
  ~ConfigManager();
  
  // 初期化・終了処理
  bool begin();
  void end();
  
  // 設定読み込み・保存
  bool loadConfig();
  bool saveConfig();
  bool resetToDefaults();
  
  // 設定値取得
  const WiFiConfig& getWiFiConfig() const { return wifi_config_; }
  const CommunicationConfig& getCommunicationConfig() const { return comm_config_; }
  const LEDConfig& getLEDConfig() const { return led_config_; }
  const DebugConfig& getDebugConfig() const { return debug_config_; }
  const OpeningConfig& getOpeningConfig() const { return opening_config_; }
  
  // 設定値変更
  void setWiFiConfig(const WiFiConfig& config);
  void setCommunicationConfig(const CommunicationConfig& config);
  void setLEDConfig(const LEDConfig& config);
  void setDebugConfig(const DebugConfig& config);
  void setOpeningConfig(const OpeningConfig& config);
  
  // 便利な取得メソッド
  String getWiFiSSID() const { return wifi_config_.ssid; }
  String getWiFiPassword() const { return wifi_config_.password; }
  String getJoystickSSID() const { return wifi_config_.joystick_ssid; }
  int getUDPPort() const { return comm_config_.udp_port; }
  String getJoystickIP() const { return comm_config_.joystick_ip; }
  int getLEDPin() const { return led_config_.pin; }
  int getLEDBrightness() const { return led_config_.brightness; }
  
  // 設定ファイル情報
  bool configExists() const;
  size_t getConfigSize() const;
  void printConfig() const;

private:
  static const char* CONFIG_FILE_PATH;
  static const size_t JSON_BUFFER_SIZE = 2048;
  
  WiFiConfig wifi_config_;
  CommunicationConfig comm_config_;
  LEDConfig led_config_;
  DebugConfig debug_config_;
  OpeningConfig opening_config_;
  
  bool config_loaded_;
  
  // 内部メソッド
  bool parseJsonConfig(const String& json_str);
  String generateJsonConfig() const;
  void setDefaultValues();
  bool validateConfig() const;
};