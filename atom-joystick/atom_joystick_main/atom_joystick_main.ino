/*
 * isolation-sphere分散MQTT制御システム
 * M5Stack Atom-JoyStick中央制御ハブ
 * 
 * 機能:
 * - WiFiアクセスポイント (IsolationSphere-Direct)
 * - 軽量MQTTブローカー (uMQTT)
 * - Joystick入力処理 (アナログ + ボタン)
 * - LCD状態表示 (デバイス管理UI)
 * - ESP32デバイス自動発見・統合制御
 * 
 * 作成日: 2025-09-03
 * プロジェクト: isolation-sphere
 * ライセンス: MIT
 */

#include <M5Unified.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "wifi_ap.h"
#include "mqtt_broker.h"
#include "joystick_input.h"
#include "lcd_display.h"

// システム設定
const char* SYSTEM_VERSION = "1.0.0";
const char* BUILD_DATE = __DATE__ " " __TIME__;

// WiFiアクセスポイント設定
const char* AP_SSID = "IsolationSphere-Direct";
const char* AP_PASSWORD = "isolation-sphere-mqtt";
IPAddress AP_IP(192, 168, 100, 1);
IPAddress AP_GATEWAY(192, 168, 100, 1);
IPAddress AP_SUBNET(255, 255, 255, 0);

// MQTT設定 (config.json準拠)
const int MQTT_PORT = 1884;
const int MAX_CLIENTS = 8;
const char* MQTT_CLIENT_ID = "atom-joystick-hub";

// システム状態
struct SystemState {
  bool wifi_ap_active;
  bool mqtt_broker_active;
  int connected_devices;
  unsigned long uptime_ms;
  float cpu_temperature;
  int battery_level;
} system_state;

// 前方宣言
void setup();
void loop();
void initializeSystem();
void handleSystemTasks();
void updateSystemState();
void publishSystemStatus();

void setup() {
  // M5Unified初期化
  M5.begin();
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  Serial.println("=================================");
  Serial.println(" isolation-sphere Control Hub");
  Serial.println(" M5Stack Atom-JoyStick");
  Serial.printf(" Version: %s\n", SYSTEM_VERSION);
  Serial.printf(" Build: %s\n", BUILD_DATE);
  Serial.println("=================================");

  // システム初期化
  initializeSystem();
  
  Serial.println("✅ System initialization completed");
  Serial.println("🚀 Starting distributed MQTT control system...");
}

void loop() {
  // M5Unified更新
  M5.update();
  
  // システムタスク処理
  handleSystemTasks();
  
  // 各モジュール更新
  joystick_update();
  mqtt_broker_loop();
  lcd_display_update();
  
  // システム状態更新
  static unsigned long last_status_update = 0;
  if (millis() - last_status_update > 1000) { // 1秒間隔
    updateSystemState();
    publishSystemStatus();
    last_status_update = millis();
  }
  
  delay(10); // 10ms基本ループ
}

void initializeSystem() {
  // LCD表示初期化
  lcd_display_init();
  lcd_display_show_startup("isolation-sphere Hub", SYSTEM_VERSION);
  
  // WiFiアクセスポイント開始
  Serial.println("📡 Starting WiFi Access Point...");
  if (wifi_ap_init(AP_SSID, AP_PASSWORD, AP_IP, AP_GATEWAY, AP_SUBNET)) {
    system_state.wifi_ap_active = true;
    Serial.printf("✅ WiFi AP: %s (IP: %s)\n", AP_SSID, AP_IP.toString().c_str());
    lcd_display_show_status("WiFi AP", "ACTIVE", true);
  } else {
    Serial.println("❌ WiFi AP initialization failed");
    lcd_display_show_status("WiFi AP", "FAILED", false);
  }
  
  // MQTTブローカー開始
  Serial.println("🔄 Starting MQTT Broker...");
  if (mqtt_broker_init(MQTT_PORT, MAX_CLIENTS)) {
    system_state.mqtt_broker_active = true;
    Serial.printf("✅ MQTT Broker: Port %d (Max clients: %d)\n", MQTT_PORT, MAX_CLIENTS);
    lcd_display_show_status("MQTT Broker", "ACTIVE", true);
  } else {
    Serial.println("❌ MQTT Broker initialization failed");
    lcd_display_show_status("MQTT Broker", "FAILED", false);
  }
  
  // Joystick入力初期化
  Serial.println("🎮 Initializing Joystick Input...");
  if (joystick_init()) {
    Serial.println("✅ Joystick input system ready");
    lcd_display_show_status("Joystick", "READY", true);
  } else {
    Serial.println("❌ Joystick initialization failed");
    lcd_display_show_status("Joystick", "FAILED", false);
  }
  
  delay(2000); // 初期化完了表示
}

void handleSystemTasks() {
  // WiFi接続監視
  wifi_ap_monitor();
  
  // MQTT接続管理
  mqtt_broker_handle_clients();
  
  // Joystick入力処理
  static JoystickState last_js_state;
  JoystickState current_js_state = joystick_get_state();
  
  if (joystick_state_changed(&last_js_state, &current_js_state)) {
    // Joystick入力変化をMQTT配信
    mqtt_broker_publish_joystick_state(&current_js_state);
    last_js_state = current_js_state;
  }
  
  // ボタン入力処理
  if (M5.BtnA.wasPressed()) {
    Serial.println("🔘 Button A pressed - Toggle playback");
    mqtt_broker_publish("isolation-sphere/cmd/playback/toggle", "1", true);
    lcd_display_show_action("TOGGLE", "Playback");
  }
  
  if (M5.BtnB.wasPressed()) {
    Serial.println("🔘 Button B pressed - Next video");
    mqtt_broker_publish("isolation-sphere/cmd/playback/next", "1", true);
    lcd_display_show_action("NEXT", "Video");
  }
}

void updateSystemState() {
  system_state.uptime_ms = millis();
  system_state.connected_devices = mqtt_broker_get_client_count();
  system_state.cpu_temperature = M5.In_I2C.readTemperature(); // M5AtomS3内部温度
  system_state.battery_level = M5.Power.getBatteryLevel();
  
  // LCD表示更新
  lcd_display_update_system_info(&system_state);
}

void publishSystemStatus() {
  // システム状態をMQTT配信
  StaticJsonDocument<512> status_doc;
  status_doc["uptime_ms"] = system_state.uptime_ms;
  status_doc["connected_devices"] = system_state.connected_devices;
  status_doc["wifi_ap_active"] = system_state.wifi_ap_active;
  status_doc["mqtt_broker_active"] = system_state.mqtt_broker_active;
  status_doc["cpu_temperature"] = system_state.cpu_temperature;
  status_doc["battery_level"] = system_state.battery_level;
  status_doc["version"] = SYSTEM_VERSION;
  status_doc["build_date"] = BUILD_DATE;
  
  String status_json;
  serializeJson(status_doc, status_json);
  
  mqtt_broker_publish("isolation-sphere/hub/status", status_json.c_str(), true);
  
  // デバッグ出力
  Serial.printf("📊 System Status - Uptime: %lu ms, Devices: %d, Temp: %.1f°C, Battery: %d%%\n",
    system_state.uptime_ms, system_state.connected_devices, 
    system_state.cpu_temperature, system_state.battery_level);
}