/**
 * @file JoystickConfig.cpp
 * @brief Atom-JoyStick 設定管理システム実装
 */

#include "JoystickConfig.h"

// 静的メンバー変数初期化
const char* JoystickConfig::CONFIG_FILE_PATH = "/config.json";
const char* JoystickConfig::BACKUP_FILE_PATH = "/config.backup.json";

/**
 * @brief コンストラクタ
 */
JoystickConfig::JoystickConfig() 
  : initialized_(false)
  , stats_({0, 0, 0, 0, 0}) {
}

/**
 * @brief デストラクタ
 */
JoystickConfig::~JoystickConfig() {
  end();
}

/**
 * @brief 初期化
 */
bool JoystickConfig::begin() {
  if (initialized_) {
    return true;
  }
  
  logInfo("JoystickConfig: 初期化開始");
  
  if (!SPIFFS.begin(true)) {
    logError("SPIFFS初期化失敗");
    return false;
  }
  
  // 設定ファイル存在確認・読み込み
  if (isConfigFileExists()) {
    logInfo("既存設定ファイル発見、読み込み実行");
    if (!loadConfig()) {
      logError("設定読み込み失敗、デフォルト設定使用");
      // デフォルト設定で継続（エラーではない）
    }
  } else {
    logInfo("設定ファイル未存在、デフォルト設定で初期化");
    // デフォルト設定を保存
    if (!saveConfig()) {
      logError("初期設定保存失敗");
    }
  }
  
  // 設定検証
  if (!validateConfig()) {
    logError("設定検証失敗、デフォルト設定にリセット");
    resetToDefaults();
    saveConfig();
  }
  
  initialized_ = true;
  logInfo("JoystickConfig: 初期化完了");
  
  return true;
}

/**
 * @brief 終了処理
 */
void JoystickConfig::end() {
  if (initialized_) {
    // 最終設定を保存
    saveConfig();
    initialized_ = false;
    logInfo("JoystickConfig: 終了完了");
  }
}

/**
 * @brief 設定読み込み
 */
bool JoystickConfig::loadConfig() {
  if (!initialized_) {
    return false;
  }
  
  logInfo("設定ファイル読み込み開始");
  
  bool result = loadFromSPIFFS();
  updateStats(result, true);
  
  if (result) {
    logInfo("設定読み込み完了");
  } else {
    logError("設定読み込み失敗");
  }
  
  return result;
}

/**
 * @brief 設定保存
 */
bool JoystickConfig::saveConfig() {
  if (!initialized_) {
    return false;
  }
  
  logInfo("設定ファイル保存開始");
  
  bool result = saveToSPIFFS();
  updateStats(result, false);
  
  if (result) {
    logInfo("設定保存完了");
  } else {
    logError("設定保存失敗");
  }
  
  return result;
}

/**
 * @brief デフォルト設定リセット
 */
bool JoystickConfig::resetToDefaults() {
  logInfo("デフォルト設定リセット実行");
  
  wifi_ap_config_ = WiFiAPConfig();
  udp_config_ = UDPConfig();
  system_config_ = SystemConfig();
  
  return saveConfig();
}

/**
 * @brief SPIFFSから読み込み
 */
bool JoystickConfig::loadFromSPIFFS() {
  if (!SPIFFS.exists(CONFIG_FILE_PATH)) {
    logError("設定ファイルが存在しません", CONFIG_FILE_PATH);
    return false;
  }
  
  File file = SPIFFS.open(CONFIG_FILE_PATH, "r");
  if (!file) {
    logError("設定ファイル開けません", CONFIG_FILE_PATH);
    return false;
  }
  
  String json_data = file.readString();
  file.close();
  
  if (json_data.length() == 0) {
    logError("設定ファイルが空です");
    return false;
  }
  
  return parseJSON(json_data);
}

/**
 * @brief SPIFFSに保存
 */
bool JoystickConfig::saveToSPIFFS() {
  // バックアップ作成
  if (SPIFFS.exists(CONFIG_FILE_PATH)) {
    SPIFFS.remove(BACKUP_FILE_PATH);
    SPIFFS.rename(CONFIG_FILE_PATH, BACKUP_FILE_PATH);
  }
  
  File file = SPIFFS.open(CONFIG_FILE_PATH, "w");
  if (!file) {
    logError("設定ファイル作成失敗", CONFIG_FILE_PATH);
    return false;
  }
  
  String json_data = createJSON();
  size_t written = file.print(json_data);
  file.close();
  
  if (written != json_data.length()) {
    logError("設定ファイル書き込み不完全");
    return false;
  }
  
  return true;
}

/**
 * @brief JSON解析
 */
bool JoystickConfig::parseJSON(const String& json_data) {
  JsonDocument doc;
  
  DeserializationError error = deserializeJson(doc, json_data);
  if (error) {
    logError("JSON解析エラー", error.c_str());
    return false;
  }
  
  if (!validateJSONStructure(doc)) {
    logError("JSON構造検証失敗");
    return false;
  }
  
  // WiFi AP設定読み込み
  if (doc["wifi_ap"].is<JsonObject>()) {
    JsonObject wifi = doc["wifi_ap"];
    if (wifi["ssid"].is<const char*>()) {
      strncpy(wifi_ap_config_.ssid, wifi["ssid"], sizeof(wifi_ap_config_.ssid) - 1);
      wifi_ap_config_.ssid[sizeof(wifi_ap_config_.ssid) - 1] = '\0';
    }
    if (wifi["password"].is<const char*>()) {
      strncpy(wifi_ap_config_.password, wifi["password"], sizeof(wifi_ap_config_.password) - 1);
      wifi_ap_config_.password[sizeof(wifi_ap_config_.password) - 1] = '\0';
    }
    if (wifi["local_ip"].is<const char*>()) {
      wifi_ap_config_.local_ip.fromString(wifi["local_ip"].as<const char*>());
    }
    if (wifi["gateway"].is<const char*>()) {
      wifi_ap_config_.gateway.fromString(wifi["gateway"].as<const char*>());
    }
    if (wifi["subnet"].is<const char*>()) {
      wifi_ap_config_.subnet.fromString(wifi["subnet"].as<const char*>());
    }
    if (wifi["channel"].is<int>()) {
      wifi_ap_config_.channel = wifi["channel"];
    }
    if (wifi["hidden"].is<bool>()) {
      wifi_ap_config_.hidden = wifi["hidden"];
    }
    if (wifi["max_connections"].is<int>()) {
      wifi_ap_config_.max_connections = wifi["max_connections"];
    }
  }
  
  // UDP設定読み込み
  if (doc["udp"].is<JsonObject>()) {
    JsonObject udp = doc["udp"];
    if (udp["target_ip"].is<const char*>()) {
      udp_config_.target_ip.fromString(udp["target_ip"].as<const char*>());
    }
    if (udp["port"].is<int>()) {
      udp_config_.port = udp["port"];
    }
    if (udp["update_interval_ms"].is<int>()) {
      udp_config_.update_interval_ms = udp["update_interval_ms"];
    }
  }
  
  // システム設定読み込み
  if (doc["system"].is<JsonObject>()) {
    JsonObject system = doc["system"];
    if (system["buzzer_enabled"].is<bool>()) {
      system_config_.buzzer_enabled = system["buzzer_enabled"];
    }
    if (system["buzzer_volume"].is<int>()) {
      system_config_.buzzer_volume = system["buzzer_volume"];
    }
    if (system["opening_animation_enabled"].is<bool>()) {
      system_config_.opening_animation_enabled = system["opening_animation_enabled"];
    }
    if (system["device_name"].is<const char*>()) {
      strncpy(system_config_.device_name, system["device_name"], sizeof(system_config_.device_name) - 1);
      system_config_.device_name[sizeof(system_config_.device_name) - 1] = '\0';
    }
  }
  
  return true;
}

/**
 * @brief JSON作成
 */
String JoystickConfig::createJSON() const {
  JsonDocument doc;
  
  // WiFi AP設定
  JsonObject wifi = doc["wifi_ap"].to<JsonObject>();
  wifi["ssid"] = wifi_ap_config_.ssid;
  wifi["password"] = wifi_ap_config_.password;
  wifi["local_ip"] = wifi_ap_config_.local_ip.toString();
  wifi["gateway"] = wifi_ap_config_.gateway.toString();
  wifi["subnet"] = wifi_ap_config_.subnet.toString();
  wifi["channel"] = wifi_ap_config_.channel;
  wifi["hidden"] = wifi_ap_config_.hidden;
  wifi["max_connections"] = wifi_ap_config_.max_connections;
  
  // UDP設定
  JsonObject udp = doc["udp"].to<JsonObject>();
  udp["target_ip"] = udp_config_.target_ip.toString();
  udp["port"] = udp_config_.port;
  udp["update_interval_ms"] = udp_config_.update_interval_ms;
  udp["joystick_read_interval_ms"] = udp_config_.joystick_read_interval_ms;
  udp["max_retry_count"] = udp_config_.max_retry_count;
  udp["timeout_ms"] = udp_config_.timeout_ms;
  
  // システム設定
  JsonObject system = doc["system"].to<JsonObject>();
  system["buzzer_enabled"] = system_config_.buzzer_enabled;
  system["buzzer_volume"] = system_config_.buzzer_volume;
  system["opening_animation_enabled"] = system_config_.opening_animation_enabled;
  system["lcd_brightness"] = system_config_.lcd_brightness;
  system["debug_mode"] = system_config_.debug_mode;
  system["device_name"] = system_config_.device_name;
  
  // メタデータ
  JsonObject meta = doc["meta"].to<JsonObject>();
  meta["version"] = "1.0";
  meta["created_at"] = millis();
  meta["device_type"] = "M5Stack-AtomJoyStick";
  
  String json_string;
  serializeJsonPretty(doc, json_string);
  return json_string;
}

/**
 * @brief JSON構造検証
 */
bool JoystickConfig::validateJSONStructure(const JsonDocument& doc) const {
  // 必須フィールド存在確認
  if (!doc["wifi_ap"].is<JsonObject>()) {
    return false;
  }
  if (!doc["udp"].is<JsonObject>()) {
    return false;
  }
  if (!doc["system"].is<JsonObject>()) {
    return false;
  }
  
  return true;
}

/**
 * @brief 設定検証
 */
bool JoystickConfig::validateConfig() const {
  // SSID長さ確認
  if (strlen(wifi_ap_config_.ssid) == 0 || strlen(wifi_ap_config_.ssid) > 32) {
    return false;
  }
  
  // パスワード長さ確認（オープンネットワークの場合は空文字許可）
  if (strlen(wifi_ap_config_.password) > 63) {
    return false;
  }
  
  // IPアドレス確認
  if (wifi_ap_config_.local_ip == IPAddress(0, 0, 0, 0)) {
    return false;
  }
  if (udp_config_.target_ip == IPAddress(0, 0, 0, 0)) {
    return false;
  }
  
  // ポート範囲確認
  if (udp_config_.port < 1024 || udp_config_.port > 65535) {
    return false;
  }
  
  // 間隔設定確認
  if (udp_config_.update_interval_ms < 10 || udp_config_.update_interval_ms > 1000) {
    return false;
  }
  
  return true;
}

/**
 * @brief 設定ファイル存在確認
 */
bool JoystickConfig::isConfigFileExists() const {
  return SPIFFS.exists(CONFIG_FILE_PATH);
}

/**
 * @brief WiFi AP設定更新
 */
bool JoystickConfig::setWiFiAPConfig(const WiFiAPConfig& config) {
  wifi_ap_config_ = config;
  return saveConfig();
}

/**
 * @brief UDP設定更新
 */
bool JoystickConfig::setUDPConfig(const UDPConfig& config) {
  udp_config_ = config;
  return saveConfig();
}

/**
 * @brief システム設定更新
 */
bool JoystickConfig::setSystemConfig(const SystemConfig& config) {
  system_config_ = config;
  return saveConfig();
}

/**
 * @brief SSID設定
 */
bool JoystickConfig::setSSID(const char* ssid) {
  if (!ssid || strlen(ssid) == 0 || strlen(ssid) > 32) {
    logError("無効なSSID");
    return false;
  }
  
  strncpy(wifi_ap_config_.ssid, ssid, sizeof(wifi_ap_config_.ssid) - 1);
  wifi_ap_config_.ssid[sizeof(wifi_ap_config_.ssid) - 1] = '\0';
  
  logInfo("SSID更新完了");
  return saveConfig();
}

/**
 * @brief パスワード設定
 */
bool JoystickConfig::setPassword(const char* password) {
  if (!password) {
    password = "";  // NULL の場合は空文字
  }
  
  if (strlen(password) > 63) {
    logError("パスワードが長すぎます");
    return false;
  }
  
  strncpy(wifi_ap_config_.password, password, sizeof(wifi_ap_config_.password) - 1);
  wifi_ap_config_.password[sizeof(wifi_ap_config_.password) - 1] = '\0';
  
  logInfo("パスワード更新完了");
  return saveConfig();
}

/**
 * @brief ターゲットIP設定
 */
bool JoystickConfig::setTargetIP(const IPAddress& ip) {
  if (ip == IPAddress(0, 0, 0, 0)) {
    logError("無効なIPアドレス");
    return false;
  }
  
  udp_config_.target_ip = ip;
  logInfo("ターゲットIP更新完了");
  return saveConfig();
}

/**
 * @brief ブザー有効設定
 */
bool JoystickConfig::setBuzzerEnabled(bool enabled) {
  system_config_.buzzer_enabled = enabled;
  return saveConfig();
}

/**
 * @brief ブザー音量設定
 */
bool JoystickConfig::setBuzzerVolume(int volume) {
  if (volume < 0 || volume > 255) {
    logError("音量範囲外");
    return false;
  }
  
  system_config_.buzzer_volume = volume;
  return saveConfig();
}

/**
 * @brief 設定印刷
 */
void JoystickConfig::printConfig() const {
  Serial.println();
  Serial.println("========== Joystick 設定情報 ==========");
  
  // WiFi AP設定
  Serial.println("【WiFi AP設定】");
  Serial.printf("  SSID: %s\n", wifi_ap_config_.ssid);
  Serial.printf("  パスワード: %s\n", strlen(wifi_ap_config_.password) > 0 ? "[設定済み]" : "[オープン]");
  Serial.printf("  ローカルIP: %s\n", wifi_ap_config_.local_ip.toString().c_str());
  Serial.printf("  チャンネル: %d\n", wifi_ap_config_.channel);
  Serial.printf("  最大接続数: %d\n", wifi_ap_config_.max_connections);
  
  // UDP設定
  Serial.println("【UDP通信設定】");
  Serial.printf("  ターゲットIP: %s\n", udp_config_.target_ip.toString().c_str());
  Serial.printf("  ポート: %d\n", udp_config_.port);
  Serial.printf("  更新間隔: %dms\n", udp_config_.update_interval_ms);
  Serial.printf("  読み取り間隔: %dms\n", udp_config_.joystick_read_interval_ms);
  
  // システム設定
  Serial.println("【システム設定】");
  Serial.printf("  デバイス名: %s\n", system_config_.device_name);
  Serial.printf("  ブザー: %s\n", system_config_.buzzer_enabled ? "有効" : "無効");
  Serial.printf("  ブザー音量: %d/255\n", system_config_.buzzer_volume);
  Serial.printf("  オープニング演出: %s\n", system_config_.opening_animation_enabled ? "有効" : "無効");
  Serial.printf("  デバッグモード: %s\n", system_config_.debug_mode ? "ON" : "OFF");
  
  Serial.println("=====================================");
  Serial.println();
}

/**
 * @brief 統計印刷
 */
void JoystickConfig::printStats() const {
  Serial.println();
  Serial.println("========== 設定管理統計 ==========");
  Serial.printf("読み込み回数: %lu\n", stats_.load_count);
  Serial.printf("保存回数: %lu\n", stats_.save_count);
  Serial.printf("エラー回数: %lu\n", stats_.error_count);
  Serial.printf("最終読み込み: %lums前\n", millis() - stats_.last_load_time);
  Serial.printf("最終保存: %lums前\n", millis() - stats_.last_save_time);
  Serial.println("===============================");
  Serial.println();
}

/**
 * @brief 統計リセット
 */
void JoystickConfig::resetStats() {
  stats_ = {0, 0, 0, 0, 0};
  logInfo("設定管理統計リセット完了");
}

/**
 * @brief 統計更新
 */
void JoystickConfig::updateStats(bool success, bool is_load_operation) {
  if (is_load_operation) {
    stats_.load_count++;
    stats_.last_load_time = millis();
  } else {
    stats_.save_count++;
    stats_.last_save_time = millis();
  }
  
  if (!success) {
    stats_.error_count++;
  }
}

/**
 * @brief エラーログ出力
 */
void JoystickConfig::logError(const char* message, const char* detail) const {
  Serial.printf("❌ JoystickConfig: %s", message);
  if (detail != nullptr) {
    Serial.printf(" - %s", detail);
  }
  Serial.println();
}

/**
 * @brief 情報ログ出力
 */
void JoystickConfig::logInfo(const char* message) const {
  Serial.printf("ℹ️ JoystickConfig: %s\n", message);
}