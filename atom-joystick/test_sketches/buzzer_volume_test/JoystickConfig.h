/**
 * @file JoystickConfig.h
 * @brief Atom-JoyStick 設定管理システム
 * @description SPIFFSベースJSON設定ファイル管理
 */

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <WiFi.h>

/**
 * @brief WiFiアクセスポイント設定
 */
struct WiFiAPConfig {
  char ssid[33];                    // SSID名（32文字+NULL終端）
  char password[65];                // パスワード（64文字+NULL終端）
  IPAddress local_ip;               // APローカルIP
  IPAddress gateway;                // ゲートウェイIP
  IPAddress subnet;                 // サブネットマスク
  int channel;                      // WiFiチャンネル（1-13）
  bool hidden;                      // SSID隠蔽設定
  int max_connections;              // 最大接続数（1-8）
  
  // デフォルト設定
  WiFiAPConfig() {
    strcpy(ssid, "IsolationSphere-Direct");
    password[0] = '\0';  // オープンネットワーク
    local_ip = IPAddress(192, 168, 100, 1);
    gateway = IPAddress(192, 168, 100, 1);
    subnet = IPAddress(255, 255, 255, 0);
    channel = 6;
    hidden = false;
    max_connections = 8;
  }
};

/**
 * @brief UDP通信設定
 */
struct UDPConfig {
  IPAddress target_ip;              // 送信先IP（ESP32）
  int port;                         // UDP送信ポート
  int update_interval_ms;           // 更新間隔（ms）
  int joystick_read_interval_ms;    // Joystick読み取り間隔（ms）
  int max_retry_count;              // 送信リトライ回数
  int timeout_ms;                   // 送信タイムアウト（ms）
  
  // デフォルト設定
  UDPConfig() {
    target_ip = IPAddress(192, 168, 100, 100);
    port = 1884;
    update_interval_ms = 30;
    joystick_read_interval_ms = 16;
    max_retry_count = 3;
    timeout_ms = 1000;
  }
};

/**
 * @brief システム設定
 */
struct SystemConfig {
  bool buzzer_enabled;              // ブザー有効/無効
  int buzzer_volume;                // ブザー音量（0-255）
  bool opening_animation_enabled;   // オープニング演出有効/無効
  int lcd_brightness;               // LCD輝度（0-255）
  bool debug_mode;                  // デバッグモード
  char device_name[33];             // デバイス名
  
  // デフォルト設定
  SystemConfig() {
    buzzer_enabled = true;
    buzzer_volume = 51;
    opening_animation_enabled = true;
    lcd_brightness = 200;
    debug_mode = false;
    strcpy(device_name, "AtomJoyStick-01");
  }
};

/**
 * @brief 設定統計
 */
struct ConfigStats {
  unsigned long load_count;         // 読み込み回数
  unsigned long save_count;         // 保存回数
  unsigned long error_count;        // エラー回数
  unsigned long last_load_time;     // 最終読み込み時刻
  unsigned long last_save_time;     // 最終保存時刻
};

/**
 * @brief Atom-JoyStick 設定管理クラス
 */
class JoystickConfig {
public:
  JoystickConfig();
  ~JoystickConfig();
  
  // 初期化・終了
  bool begin();
  void end();
  
  // 設定読み込み・保存
  bool loadConfig();
  bool saveConfig();
  bool resetToDefaults();
  
  // 設定アクセス
  const WiFiAPConfig& getWiFiAPConfig() const { return wifi_ap_config_; }
  const UDPConfig& getUDPConfig() const { return udp_config_; }
  const SystemConfig& getSystemConfig() const { return system_config_; }
  
  // 設定更新
  bool setWiFiAPConfig(const WiFiAPConfig& config);
  bool setUDPConfig(const UDPConfig& config);
  bool setSystemConfig(const SystemConfig& config);
  
  // 個別設定更新
  bool setSSID(const char* ssid);
  bool setPassword(const char* password);
  bool setTargetIP(const IPAddress& ip);
  bool setBuzzerEnabled(bool enabled);
  bool setBuzzerVolume(int volume);
  
  // 設定検証
  bool validateConfig() const;
  bool isConfigFileExists() const;
  
  // 統計・デバッグ
  const ConfigStats& getStats() const { return stats_; }
  void printConfig() const;
  void printStats() const;
  void resetStats();

private:
  WiFiAPConfig wifi_ap_config_;
  UDPConfig udp_config_;
  SystemConfig system_config_;
  ConfigStats stats_;
  bool initialized_;
  
  // 内部メソッド
  bool loadFromSPIFFS();
  bool saveToSPIFFS();
  bool parseJSON(const String& json_data);
  String createJSON() const;
  bool validateJSONStructure(const JsonDocument& doc) const;
  
  // ファイルパス
  static const char* CONFIG_FILE_PATH;
  static const char* BACKUP_FILE_PATH;
  
  // エラー・ログ出力
  void logError(const char* message, const char* detail = nullptr) const;
  void logInfo(const char* message) const;
  void updateStats(bool success, bool is_load_operation);
};