/**
 * @file config_manager.cpp
 * @brief 設定管理システム実装
 */

#include "config_manager.h"

const char* ConfigManager::CONFIG_FILE_PATH = "/config.json";

/**
 * @brief コンストラクタ
 */
ConfigManager::ConfigManager() : config_loaded_(false) {
  setDefaultValues();
}

/**
 * @brief デストラクタ
 */
ConfigManager::~ConfigManager() {
  end();
}

/**
 * @brief 設定管理システム初期化
 */
bool ConfigManager::begin() {
  Serial.println("ConfigManager: 初期化開始");
  
  if (!SPIFFS.begin(true)) {
    Serial.println("ConfigManager: SPIFFS初期化失敗");
    return false;
  }
  
  // 設定ファイル読み込み
  if (configExists()) {
    Serial.printf("ConfigManager: 設定ファイル発見 (サイズ: %d bytes)\n", getConfigSize());
    if (loadConfig()) {
      Serial.println("ConfigManager: 設定ファイル読み込み成功");
      Serial.printf("ConfigManager: 読み込み済み静的IP: %s\n", wifi_config_.static_ip.c_str());
      config_loaded_ = true;
    } else {
      Serial.println("ConfigManager: 設定ファイル読み込み失敗、デフォルト値使用");
      setDefaultValues();
      Serial.printf("ConfigManager: デフォルト静的IP適用: %s\n", wifi_config_.static_ip.c_str());
    }
  } else {
    Serial.println("ConfigManager: 設定ファイル未存在、デフォルト値で作成");
    setDefaultValues();
    Serial.printf("ConfigManager: 新規作成時静的IP: %s\n", wifi_config_.static_ip.c_str());
    saveConfig(); // デフォルト設定を保存
  }
  
  // IP設定確認・強制修正（192.168.100.20 → 192.168.100.100）
  if (wifi_config_.static_ip == "192.168.100.20") {
    Serial.println("████████████████████████████████████████████████████████");
    Serial.println("██ ⚠️ ⚠️ ⚠️  古いIP検出・強制修正実行  ⚠️ ⚠️ ⚠️       ██");
    Serial.println("████████████████████████████████████████████████████████");
    Serial.println("██ 旧IP: 192.168.100.20 → 新IP: 192.168.100.100        ██");
    Serial.println("██ config.jsonに従った修正を実行                       ██");
    Serial.println("████████████████████████████████████████████████████████");
    wifi_config_.static_ip = "192.168.100.100";
    saveConfig(); // 修正した設定を保存
    Serial.println("██ ✅ IP設定修正完了・設定ファイル更新済み           ██");
    Serial.println("████████████████████████████████████████████████████████");
  }
  
  printConfig();
  return true;
}

/**
 * @brief 終了処理
 */
void ConfigManager::end() {
  // 必要に応じて最終設定保存
}

/**
 * @brief 設定ファイル読み込み
 */
bool ConfigManager::loadConfig() {
  File config_file = SPIFFS.open(CONFIG_FILE_PATH, "r");
  if (!config_file) {
    Serial.println("ConfigManager: 設定ファイル読み込み失敗");
    return false;
  }
  
  String json_str = config_file.readString();
  config_file.close();
  
  if (json_str.length() == 0) {
    Serial.println("ConfigManager: 設定ファイルが空");
    return false;
  }
  
  return parseJsonConfig(json_str);
}

/**
 * @brief 設定ファイル保存
 */
bool ConfigManager::saveConfig() {
  String json_str = generateJsonConfig();
  
  File config_file = SPIFFS.open(CONFIG_FILE_PATH, "w");
  if (!config_file) {
    Serial.println("ConfigManager: 設定ファイル書き込み失敗");
    return false;
  }
  
  size_t written = config_file.print(json_str);
  config_file.close();
  
  if (written != json_str.length()) {
    Serial.println("ConfigManager: 設定ファイル書き込み不完全");
    return false;
  }
  
  Serial.printf("ConfigManager: 設定ファイル保存成功 (%d bytes)\n", written);
  return true;
}

/**
 * @brief JSON文字列解析
 */
bool ConfigManager::parseJsonConfig(const String& json_str) {
  StaticJsonDocument<JSON_BUFFER_SIZE> doc;
  DeserializationError error = deserializeJson(doc, json_str);
  
  if (error) {
    Serial.printf("ConfigManager: JSON解析失敗: %s\n", error.c_str());
    return false;
  }
  
  // WiFi設定読み込み
  JsonObject wifi = doc["wifi"];
  if (!wifi.isNull()) {
    wifi_config_.ssid = wifi["ssid"] | "IsolationSphere-Direct";
    wifi_config_.password = wifi["password"] | "";
    wifi_config_.mode = wifi["mode"] | "client";
    wifi_config_.static_ip = wifi["static_ip"] | "192.168.100.100";
    wifi_config_.joystick_ssid = wifi["joystick_ssid"] | "IsolationSphere-Direct";
  }
  
  // 通信設定読み込み
  JsonObject comm = doc["communication"];
  if (!comm.isNull()) {
    comm_config_.udp_port = comm["udp_port"] | 1884;
    comm_config_.joystick_ip = comm["joystick_ip"] | "192.168.100.1";
    comm_config_.response_timeout = comm["response_timeout"] | 100;
  }
  
  // LED設定読み込み
  JsonObject led = doc["led"];
  if (!led.isNull()) {
    led_config_.pin = led["pin"] | 35;
    led_config_.count = led["count"] | 1;
    led_config_.brightness = led["brightness"] | 128;
    led_config_.update_rate = led["update_rate"] | 30;
  }
  
  // デバッグ設定読み込み
  JsonObject debug = doc["debug"];
  if (!debug.isNull()) {
    debug_config_.serial_output = debug["serial_output"] | true;
    debug_config_.performance_monitor = debug["performance_monitor"] | true;
  }
  
  // オープニング設定読み込み
  JsonObject opening = doc["opening"];
  if (!opening.isNull()) {
    opening_config_.enabled = opening["enabled"] | true;
    opening_config_.frame_duration_ms = opening["frame_duration_ms"] | 400;
    opening_config_.brightness = opening["brightness"] | 200;
    opening_config_.fade_effect = opening["fade_effect"] | false;
    opening_config_.fade_steps = opening["fade_steps"] | 10;
  }
  
  return validateConfig();
}

/**
 * @brief JSON文字列生成
 */
String ConfigManager::generateJsonConfig() const {
  StaticJsonDocument<JSON_BUFFER_SIZE> doc;
  
  // WiFi設定
  JsonObject wifi = doc.createNestedObject("wifi");
  wifi["ssid"] = wifi_config_.ssid;
  wifi["password"] = wifi_config_.password;
  wifi["mode"] = wifi_config_.mode;
  wifi["static_ip"] = wifi_config_.static_ip;
  wifi["joystick_ssid"] = wifi_config_.joystick_ssid;
  
  // 通信設定
  JsonObject comm = doc.createNestedObject("communication");
  comm["udp_port"] = comm_config_.udp_port;
  comm["joystick_ip"] = comm_config_.joystick_ip;
  comm["response_timeout"] = comm_config_.response_timeout;
  
  // LED設定
  JsonObject led = doc.createNestedObject("led");
  led["pin"] = led_config_.pin;
  led["count"] = led_config_.count;
  led["brightness"] = led_config_.brightness;
  led["update_rate"] = led_config_.update_rate;
  
  // デバッグ設定
  JsonObject debug = doc.createNestedObject("debug");
  debug["serial_output"] = debug_config_.serial_output;
  debug["performance_monitor"] = debug_config_.performance_monitor;
  
  // オープニング設定
  JsonObject opening = doc.createNestedObject("opening");
  opening["enabled"] = opening_config_.enabled;
  opening["frame_duration_ms"] = opening_config_.frame_duration_ms;
  opening["brightness"] = opening_config_.brightness;
  opening["fade_effect"] = opening_config_.fade_effect;
  opening["fade_steps"] = opening_config_.fade_steps;
  
  String json_str;
  serializeJsonPretty(doc, json_str);
  return json_str;
}

/**
 * @brief デフォルト値設定
 */
void ConfigManager::setDefaultValues() {
  // WiFi設定デフォルト値
  wifi_config_.ssid = "IsolationSphere-Direct";
  wifi_config_.password = "";  // オープン接続
  wifi_config_.mode = "client";
  wifi_config_.static_ip = "192.168.100.100";  // 固定IP開始アドレス
  wifi_config_.joystick_ssid = "IsolationSphere-Direct";  // Joystick SSID
  
  // 通信設定デフォルト値
  comm_config_.udp_port = 1884;  // MQTT隣接ポート
  comm_config_.joystick_ip = "192.168.100.1";
  comm_config_.response_timeout = 100;
  
  // LED設定デフォルト値
  led_config_.pin = 35;
  led_config_.count = 1;
  led_config_.brightness = 128;
  led_config_.update_rate = 30;
  
  // デバッグ設定デフォルト値
  debug_config_.serial_output = true;
  debug_config_.performance_monitor = true;
  
  // オープニング設定デフォルト値
  opening_config_.enabled = true;
  opening_config_.frame_duration_ms = 400;
  opening_config_.brightness = 200;
  opening_config_.fade_effect = false;
  opening_config_.fade_steps = 10;
}

/**
 * @brief 設定値検証
 */
bool ConfigManager::validateConfig() const {
  // ポート番号検証
  if (comm_config_.udp_port < 1 || comm_config_.udp_port > 65535) {
    Serial.printf("ConfigManager: 無効なUDPポート: %d\n", comm_config_.udp_port);
    return false;
  }
  
  // LED設定検証
  if (led_config_.brightness < 0 || led_config_.brightness > 255) {
    Serial.printf("ConfigManager: 無効なLED明度: %d\n", led_config_.brightness);
    return false;
  }
  
  // WiFi SSID検証
  if (wifi_config_.ssid.length() == 0) {
    Serial.println("ConfigManager: WiFi SSID が空");
    return false;
  }
  
  return true;
}

/**
 * @brief 設定ファイル存在確認
 */
bool ConfigManager::configExists() const {
  return SPIFFS.exists(CONFIG_FILE_PATH);
}

/**
 * @brief 設定ファイルサイズ取得
 */
size_t ConfigManager::getConfigSize() const {
  File config_file = SPIFFS.open(CONFIG_FILE_PATH, "r");
  if (!config_file) {
    return 0;
  }
  
  size_t size = config_file.size();
  config_file.close();
  return size;
}

/**
 * @brief 設定内容出力
 */
void ConfigManager::printConfig() const {
  Serial.println("\n========== 設定内容 ==========");
  Serial.printf("WiFi SSID: %s\n", wifi_config_.ssid.c_str());
  Serial.printf("WiFi Mode: %s\n", wifi_config_.mode.c_str());
  Serial.printf("Static IP: %s\n", wifi_config_.static_ip.c_str());
  Serial.printf("UDP Port: %d\n", comm_config_.udp_port);
  Serial.printf("Joystick IP: %s\n", comm_config_.joystick_ip.c_str());
  Serial.printf("Response Timeout: %dms\n", comm_config_.response_timeout);
  Serial.printf("LED Pin: %d\n", led_config_.pin);
  Serial.printf("LED Count: %d\n", led_config_.count);
  Serial.printf("LED Brightness: %d\n", led_config_.brightness);
  Serial.printf("Update Rate: %dHz\n", led_config_.update_rate);
  Serial.println("==============================\n");
}

/**
 * @brief デフォルト設定にリセット
 */
bool ConfigManager::resetToDefaults() {
  setDefaultValues();
  return saveConfig();
}