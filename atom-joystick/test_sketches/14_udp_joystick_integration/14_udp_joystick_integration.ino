/**
 * @file 14_udp_joystick_integration.ino
 * @brief M5Stack Atom-JoyStick UDP統合システム
 * @description JoystickデータをUDP送信し、ESP32制御を実現
 * 
 * Phase 4.7: UDP通信統合システム実装
 * - WiFiアクセスポイント: IsolationSphere-Direct
 * - JoystickデータのUDP送信（ポート1884）
 * - 15-30ms応答性実現
 * - 4モードUI搭載
 * 
 * @target M5Stack Atom-JoyStick (ESP32-S3)
 * @integration isolation-sphere分散制御システム
 */

#include <M5Unified.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <TJpg_Decoder.h>

// オープニング・ブザーシステム・MQTT統合・UI統合
#include "JoystickOpeningDisplay.h"
#include "JoystickBuzzer.h"
#include "JoystickConfig.h"
#include "JoystickMQTTManager.h"
#include "JoystickDualDialUI.h"

// ========== I2C設定（13_correct_joystick_testから継承） ==========
#define I2C_ADDRESS                (0x59)
#define LEFT_STICK_X_ADDRESS       (0x00)
#define LEFT_STICK_Y_ADDRESS       (0x02)
#define RIGHT_STICK_X_ADDRESS      (0x20)
#define RIGHT_STICK_Y_ADDRESS      (0x22)
#define LEFT_STICK_BUTTON_ADDRESS  (0x70)
#define RIGHT_STICK_BUTTON_ADDRESS (0x71)
#define LEFT_BUTTON_ADDRESS        (0x72)
#define RIGHT_BUTTON_ADDRESS       (0x73)
#define BATTERY_VOLTAGE1_ADDRESS   (0x60)
#define BATTERY_VOLTAGE2_ADDRESS   (0x62)

// I2Cピン設定（atoms3joy.h公式仕様）
const int I2C_SDA_PIN = 38;
const int I2C_SCL_PIN = 39;
const uint32_t I2C_FREQUENCY = 100000U;  // 100kHz

// ========== 設定管理・通信設定 ==========
JoystickConfig config;
WiFiUDP udp;

// ========== Joystickデータ構造 ==========
struct JoystickData {
    uint16_t left_x;
    uint16_t left_y;
    uint16_t right_x;
    uint16_t right_y;
    bool left_stick_button;
    bool right_stick_button;
    bool left_button;
    bool right_button;
    uint16_t battery_voltage1;
    uint16_t battery_voltage2;
    bool valid;
    unsigned long timestamp;
    uint32_t sequence;
};

// ========== UI/表示モード管理 ==========
enum UIMode {
    MODE_JOYSTICK_MONITOR = 0,    // Joystick監視モード（従来UI・デバッグ用）
    MODE_NETWORK_STATUS = 1,      // ネットワーク状態モード（従来UI・デバッグ用）
    MODE_UDP_COMMUNICATION = 2,   // UDP通信モード（従来UI・デバッグ用）
    MODE_SYSTEM_SETTINGS = 3      // システム設定モード（従来UI・デバッグ用）
};

// デュアルダイアルUI使用フラグ
bool use_dual_dial_ui = true;  // デフォルトで新UIを使用

// ========== グローバル変数 ==========
JoystickData joystick_data;
UIMode current_mode = MODE_JOYSTICK_MONITOR;
unsigned long last_joystick_read = 0;
unsigned long last_udp_send = 0;
unsigned long last_display_update = 0;
unsigned long last_button_press = 0;
uint32_t udp_sequence = 0;
uint32_t udp_success_count = 0;
uint32_t udp_error_count = 0;
bool wifi_connected = false;

// オープニング・ブザーシステム・MQTT統合・UI統合
JoystickOpeningDisplay opening_display;
JoystickBuzzer buzzer;
JoystickMQTTManager mqtt_manager;
JoystickDualDialUI dual_dial_ui;

// 設定関連グローバル変数
WiFiAPConfig wifi_config;
UDPConfig udp_config;
SystemConfig system_config;

// 表示更新間隔設定
const int DISPLAY_UPDATE_INTERVAL = 250;  // 250ms LCD更新
const int BUTTON_DEBOUNCE = 200;           // ボタンデバウンス

void setup() {
    // M5Unified初期化
    auto cfg = M5.config();
    cfg.external_spk = false;
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("============================================");
    Serial.println("M5Stack Atom-JoyStick UDP統合システム");
    Serial.println("============================================");
    Serial.printf("Phase 4.10: config.json設定システム統合\n");
    Serial.println();
    
    // 設定ファイル初期化・読み込み
    if (!config.begin()) {
        Serial.println("❌ 設定ファイル初期化失敗");
        buzzer.error_tone();
    } else {
        Serial.println("✅ 設定ファイル初期化完了");
        
        // 設定取得
        wifi_config = config.getWiFiAPConfig();
        udp_config = config.getUDPConfig();
        system_config = config.getSystemConfig();
        
        // 設定表示
        config.printConfig();
        
        Serial.printf("WiFi AP: %s\n", wifi_config.ssid);
        Serial.printf("IP Range: %s\n", wifi_config.local_ip.toString().c_str());
        Serial.printf("UDP Port: %d\n", udp_config.port);
        Serial.printf("Target ESP32: %s\n", udp_config.target_ip.toString().c_str());
        Serial.printf("更新間隔: %dms\n", udp_config.update_interval_ms);
        Serial.printf("デバイス名: %s\n", system_config.device_name);
        Serial.println();
    }
    
    // ブザーシステム初期化（設定ファイル反映）
    if (!buzzer.begin()) {
        Serial.println("❌ ブザーシステム初期化失敗");
    } else {
        Serial.println("✅ ブザーシステム初期化完了");
        
        // 設定ファイルからブザー設定適用
        buzzer.setEnabled(system_config.buzzer_enabled);
        buzzer.setVolume(system_config.buzzer_volume);
        
        if (system_config.buzzer_enabled) {
            buzzer.startup_melody();  // 3音下降メロディー（設定で有効の場合のみ）
        }
    }
    
    // オープニング表示システム初期化（設定ファイル反映）
    if (system_config.opening_animation_enabled) {
        if (!opening_display.begin()) {
            Serial.println("❌ オープニング表示初期化失敗");
        } else {
            Serial.println("✅ オープニング表示初期化完了");
            
            // オープニングメロディ開始
            buzzer.opening_startup_melody();
            
            // オープニング演出実行
            if (opening_display.playOpeningSequence()) {
                buzzer.opening_completion_melody();  // 完了メロディ
            } else {
                buzzer.error_tone();  // エラー音
            }
        }
    } else {
        Serial.println("⏭️ オープニング演出: 設定により無効");
    }
    
    // LCD初期化
    M5.Display.clear(BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setRotation(0);
    
    // I2C初期化
    initialize_i2c();
    
    // WiFiアクセスポイント初期化
    initialize_wifi_ap();
    
    // UDP初期化
    initialize_udp();
    
    // MQTTブローカー初期化
    if (!mqtt_manager.begin(config)) {
        Serial.println("❌ MQTTブローカー初期化失敗");
    } else {
        Serial.println("✅ MQTTブローカー初期化完了");
        
        // 初期状態をMQTTで配信
        mqtt_manager.publishCurrentMode("control");
        mqtt_manager.publishBrightness(180);  // デフォルト明度
        mqtt_manager.publishVolume(75);       // デフォルト音量
        mqtt_manager.publishWiFiClients(WiFi.softAPgetStationNum());
    }
    
    // デュアルダイアルUI初期化
    if (!dual_dial_ui.begin(config)) {
        Serial.println("❌ デュアルダイアルUI初期化失敗");
        use_dual_dial_ui = false;  // 従来UIにフォールバック
    } else {
        Serial.println("✅ デュアルダイアルUI初期化完了");
        
        // UIモードをLiveモードに設定（デフォルト）
        dual_dial_ui.setMode(UI_MODE_LIVE);
        
        // UIコールバック設定
        dual_dial_ui.setValueChangeCallback([](const char* topic, int value) {
            // MQTT配信（適切な配信メソッドを使用）
            String topic_str = String(topic);
            if (topic_str == "control/brightness") {
                mqtt_manager.publishBrightness(value);
            } else if (topic_str == "control/color_temp") {
                mqtt_manager.publishColorTemp(value);
            } else if (topic_str == "video/volume") {
                mqtt_manager.publishVolume(value);
            } else if (topic_str == "video/selected_id") {
                mqtt_manager.publishSelectedVideoId(value);
            }
            // 他のトピックも追加可能
            
            Serial.printf("📡 UI→MQTT配信: %s = %d\n", topic, value);
        });
        
        dual_dial_ui.setConfirmCallback([](uint8_t item_index, int value) {
            buzzer.button_click();  // 確定音
            Serial.printf("✅ UI確定: 項目%d = %d\n", item_index, value);
        });
    }
    
    // 初期データ
    joystick_data.valid = false;
    joystick_data.sequence = 0;
    
    display_welcome_screen();
    delay(3000);
    
    Serial.println("✅ UDP統合システム初期化完了");
    Serial.println("操作:");
    Serial.println("  - Joystick: リアルタイムUDP送信");
    Serial.println("  - ボタンA: UI モード切り替え");
    Serial.println("  - UDP送信: 30ms間隔（33.3Hz）");
    Serial.println("  - 応答性: 15-30ms目標");
    
    last_joystick_read = millis();
    last_udp_send = millis();
    last_display_update = millis();
}

void loop() {
    M5.update();
    
    // MQTT更新処理
    mqtt_manager.update();
    
    // ボタン入力処理
    handle_button_input();
    
    // Joystickデータ読み取り（設定ファイルから取得）
    if (millis() - last_joystick_read >= udp_config.joystick_read_interval_ms) {
        read_joystick_registers();
        
        // デュアルダイアルUI入力更新
        if (use_dual_dial_ui && joystick_data.valid) {
            // raw値を正規化（-1.0 ~ 1.0）
            float left_x = (joystick_data.left_x - 2048.0f) / 2048.0f;
            float left_y = (joystick_data.left_y - 2048.0f) / 2048.0f;
            float right_x = (joystick_data.right_x - 2048.0f) / 2048.0f;
            float right_y = (joystick_data.right_y - 2048.0f) / 2048.0f;
            
            dual_dial_ui.updateInputs(
                left_x, left_y, joystick_data.left_stick_button,
                right_x, right_y, joystick_data.right_stick_button,
                joystick_data.left_button, joystick_data.right_button
            );
        }
        
        last_joystick_read = millis();
    }
    
    // UDP送信（設定ファイルから取得）
    if (millis() - last_udp_send >= udp_config.update_interval_ms && joystick_data.valid) {
        send_udp_joystick_data();
        last_udp_send = millis();
    }
    
    // Joystick状態をMQTT配信（モード別処理）
    if (joystick_data.valid) {
        publish_joystick_to_mqtt();
    }
    
    // 表示更新（4Hz）
    if (millis() - last_display_update >= DISPLAY_UPDATE_INTERVAL) {
        update_display();
        last_display_update = millis();
    }
    
    delay(1);  // 最小遅延（高応答性維持）
}

void display_welcome_screen() {
    M5.Display.clear(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextColor(CYAN);
    M5.Display.setTextSize(2);
    M5.Display.println("UDP");
    M5.Display.println("Joystick");
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(WHITE);
    M5.Display.println("");
    M5.Display.println("Phase 4.10");
    M5.Display.printf("AP: %s\n", wifi_config.ssid);
    M5.Display.printf("UDP: %d\n", udp_config.port);
}

void initialize_i2c() {
    Wire.end();
    delay(100);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
    delay(200);
    
    Serial.printf("I2C初期化: SDA=%d, SCL=%d, Freq=%dHz\n", 
                  I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
    
    // 接続テスト
    Wire.beginTransmission(I2C_ADDRESS);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
        Serial.printf("✅ Joystick I2C接続成功 (0x%02X)\n", I2C_ADDRESS);
    } else {
        Serial.printf("❌ Joystick I2C接続失敗 (エラー: %d)\n", error);
    }
}

void initialize_wifi_ap() {
    Serial.println("WiFi アクセスポイント設定中...");
    
    // WiFi完全リセット
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    // WiFi設定（config.json設定使用）
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(wifi_config.local_ip, wifi_config.gateway, wifi_config.subnet);
    
    // ESP32互換性設定
    Serial.println("🔧 ESP32間通信互換設定適用中...");
    
    // 設定詳細表示（config.json設定）
    Serial.printf("設定パラメーター (config.json):\n");
    Serial.printf("  SSID: %s\n", wifi_config.ssid);
    Serial.printf("  パスワード: %s\n", strlen(wifi_config.password) > 0 ? "[設定済み]" : "オープン");
    Serial.printf("  IP: %s\n", wifi_config.local_ip.toString().c_str());
    Serial.printf("  チャンネル: %d\n", wifi_config.channel);
    Serial.printf("  ステルスモード: %s\n", wifi_config.hidden ? "ON" : "OFF");
    Serial.printf("  最大接続数: %d\n", wifi_config.max_connections);
    
    // ESP32間互換性を考慮したAP設定（config.json設定使用）
    bool ap_success;
    if (strlen(wifi_config.password) > 0) {
        ap_success = WiFi.softAP(wifi_config.ssid, wifi_config.password, wifi_config.channel, wifi_config.hidden, wifi_config.max_connections);
    } else {
        // オープンネットワーク（ESP32-ESP32認証問題回避）
        ap_success = WiFi.softAP(wifi_config.ssid, "", wifi_config.channel, wifi_config.hidden, wifi_config.max_connections);
    }
    
    if (ap_success) {
        wifi_connected = true;
        Serial.printf("✅ WiFi AP起動成功\n");
        Serial.printf("   SSID: %s\n", wifi_config.ssid);
        Serial.printf("   パスワード: %s\n", strlen(wifi_config.password) > 0 ? "[設定済み]" : "オープン (ESP32互換)");
        Serial.printf("   IP: %s\n", WiFi.softAPIP().toString().c_str());
        Serial.printf("   MAC: %s\n", WiFi.softAPmacAddress().c_str());
        Serial.printf("   ステルスモード: %s\n", wifi_config.hidden ? "ON (隠蔽)" : "OFF (可視)");
        Serial.printf("   最大接続数: 8デバイス\n");
        Serial.printf("   チャンネル: 11 (ESP32互換)\n");
        
        // WiFi接続成功音
        buzzer.wifi_connected_tone();
        
        // 接続デバイス監視開始
        Serial.println("接続デバイス監視開始...");
    } else {
        wifi_connected = false;
        Serial.println("❌ WiFi AP起動失敗");
        Serial.println("   再試行を実行します...");
        
        // 再試行
        delay(1000);
        ap_success = WiFi.softAP(wifi_config.ssid, "", 1, 0, 4);  // チャンネル1, 最大4接続
        if (ap_success) {
            wifi_connected = true;
            Serial.println("✅ 再試行でWiFi AP起動成功（チャンネル1）");
        }
    }
    
    delay(1000);
}

void initialize_udp() {
    if (wifi_connected) {
        bool udp_success = udp.begin(udp_config.port);
        
        if (udp_success) {
            Serial.printf("✅ UDP初期化成功 (ポート: %d)\n", udp_config.port);
            Serial.printf("   ローカルIP: %s\n", WiFi.softAPIP().toString().c_str());
            Serial.printf("   送信先: %s:%d\n", udp_config.target_ip.toString().c_str(), udp_config.port);
            
            // UDP接続成功音
            buzzer.udp_connected_tone();
        } else {
            Serial.printf("❌ UDP初期化失敗\n");
            buzzer.error_tone();
        }
    } else {
        Serial.println("❌ UDP初期化スキップ（WiFi未接続）");
    }
    
    // WiFi AP詳細状態表示
    Serial.printf("📡 WiFi AP詳細状態:\n");
    Serial.printf("   モード: %s\n", WiFi.getMode() == WIFI_AP ? "AP" : "OTHER");
    Serial.printf("   SSID: %s\n", WiFi.softAPSSID().c_str());
    Serial.printf("   IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("   MAC: %s\n", WiFi.softAPmacAddress().c_str());
}

void read_joystick_registers() {
    bool success = true;
    
    // LEFT STICK
    joystick_data.left_x = read_register_16bit(LEFT_STICK_X_ADDRESS);
    if (joystick_data.left_x == 0xFFFF) success = false;
    
    joystick_data.left_y = read_register_16bit(LEFT_STICK_Y_ADDRESS);
    if (joystick_data.left_y == 0xFFFF) success = false;
    
    // RIGHT STICK
    joystick_data.right_x = read_register_16bit(RIGHT_STICK_X_ADDRESS);
    if (joystick_data.right_x == 0xFFFF) success = false;
    
    joystick_data.right_y = read_register_16bit(RIGHT_STICK_Y_ADDRESS);
    if (joystick_data.right_y == 0xFFFF) success = false;
    
    // BUTTONS
    joystick_data.left_stick_button = read_register_8bit(LEFT_STICK_BUTTON_ADDRESS) > 0;
    joystick_data.right_stick_button = read_register_8bit(RIGHT_STICK_BUTTON_ADDRESS) > 0;
    joystick_data.left_button = read_register_8bit(LEFT_BUTTON_ADDRESS) > 0;
    joystick_data.right_button = read_register_8bit(RIGHT_BUTTON_ADDRESS) > 0;
    
    // BATTERY
    joystick_data.battery_voltage1 = read_register_16bit(BATTERY_VOLTAGE1_ADDRESS);
    joystick_data.battery_voltage2 = read_register_16bit(BATTERY_VOLTAGE2_ADDRESS);
    
    joystick_data.valid = success;
    joystick_data.timestamp = millis();
    joystick_data.sequence++;
}

uint16_t read_register_16bit(uint8_t reg_addr) {
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(reg_addr);
    uint8_t result = Wire.endTransmission(false);  // Repeated start
    
    if (result != 0) {
        return 0xFFFF;
    }
    
    Wire.requestFrom(I2C_ADDRESS, (uint8_t)2);
    
    if (Wire.available() >= 2) {
        uint8_t low_byte = Wire.read();
        uint8_t high_byte = Wire.read();
        return (high_byte << 8) | low_byte;
    }
    
    return 0xFFFF;
}

uint8_t read_register_8bit(uint8_t reg_addr) {
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(reg_addr);
    uint8_t result = Wire.endTransmission(false);  // Repeated start
    
    if (result != 0) {
        return 0xFF;
    }
    
    Wire.requestFrom(I2C_ADDRESS, (uint8_t)1);
    
    if (Wire.available() >= 1) {
        return Wire.read();
    }
    
    return 0xFF;
}

void send_udp_joystick_data() {
    if (!wifi_connected || !joystick_data.valid) {
        return;
    }
    
    // 接続クライアント数確認（ESP32が接続している場合のみ送信）
    int connected_clients = WiFi.softAPgetStationNum();
    if (connected_clients == 0) {
        // クライアント未接続時はエラーカウントしない
        static int no_client_counter = 0;
        if (++no_client_counter >= 100) {  // 10回に1回ログ
            Serial.println("💡 ESP32未接続: UDP送信待機中");
            no_client_counter = 0;
        }
        return;
    }
    
    // JSON形式でUDPデータ生成
    StaticJsonDocument<256> json_doc;
    json_doc["type"] = "joystick";
    json_doc["sequence"] = udp_sequence++;
    json_doc["timestamp"] = millis();
    
    JsonObject left = json_doc.createNestedObject("left");
    left["x"] = joystick_data.left_x;
    left["y"] = joystick_data.left_y;
    left["button"] = joystick_data.left_stick_button;
    
    JsonObject right = json_doc.createNestedObject("right");
    right["x"] = joystick_data.right_x;
    right["y"] = joystick_data.right_y;
    right["button"] = joystick_data.right_stick_button;
    
    JsonObject buttons = json_doc.createNestedObject("buttons");
    buttons["left"] = joystick_data.left_button;
    buttons["right"] = joystick_data.right_button;
    
    JsonObject battery = json_doc.createNestedObject("battery");
    battery["voltage1"] = joystick_data.battery_voltage1;
    battery["voltage2"] = joystick_data.battery_voltage2;
    
    // UDP送信
    String json_string;
    serializeJson(json_doc, json_string);
    
    udp.beginPacket(udp_config.target_ip, udp_config.port);
    udp.print(json_string);
    int sent_result = udp.endPacket();
    
    if (sent_result == 1) {
        udp_success_count++;
        
        // 20回に1回ログ出力
        static int log_counter = 0;
        if (++log_counter >= 20) {
            Serial.printf("🚀 UDP送信: L_X=%d L_Y=%d R_X=%d R_Y=%d → %s:%d (Clients:%d)\n", 
                          joystick_data.left_x, joystick_data.left_y,
                          joystick_data.right_x, joystick_data.right_y,
                          udp_config.target_ip.toString().c_str(), udp_config.port, connected_clients);
            log_counter = 0;
        }
    } else {
        udp_error_count++;
        
        // 送信失敗のログは10回に1回
        static int error_counter = 0;
        if (++error_counter >= 10) {
            Serial.printf("❌ UDP送信失敗: %d (Clients:%d)\n", sent_result, connected_clients);
            error_counter = 0;
        }
    }
}

void handle_button_input() {
    if (M5.BtnA.wasPressed() && (millis() - last_button_press) > BUTTON_DEBOUNCE) {
        if (use_dual_dial_ui) {
            // デュアルダイアルUIのモード切り替え
            UIOperationMode current_ui_mode = dual_dial_ui.getCurrentMode();
            UIOperationMode next_mode = (UIOperationMode)((current_ui_mode + 1) % 5);
            dual_dial_ui.setMode(next_mode);
            
            // モード名をMQTTで配信
            String mode_names[] = {"live", "control", "video", "maintain", "system"};
            mqtt_manager.publishCurrentMode(mode_names[next_mode]);
            
            Serial.printf("🎛️ デュアルダイアルUI モード変更: %s\n", mode_names[next_mode].c_str());
        } else {
            // 従来UIのモード切り替え
            current_mode = (UIMode)((current_mode + 1) % 4);
            String mode_names[] = {"control", "video", "adjust", "system"};
            mqtt_manager.publishCurrentMode(mode_names[current_mode]);
            
            Serial.printf("UI モード変更: %d\n", current_mode);
        }
        
        last_button_press = millis();
        
        // ボタンクリック音
        buzzer.button_click();
    }
}

void update_display() {
    if (use_dual_dial_ui) {
        // デュアルダイアルUI描画
        dual_dial_ui.draw();
    } else {
        // 従来UI描画（デバッグ・フォールバック用）
        M5.Display.clear(BLACK);
        M5.Display.setCursor(0, 0);
        M5.Display.setTextSize(1);
        
        switch (current_mode) {
            case MODE_JOYSTICK_MONITOR:
                display_joystick_monitor();
                break;
            case MODE_NETWORK_STATUS:
                display_network_status();
                break;
            case MODE_UDP_COMMUNICATION:
                display_udp_communication();
                break;
            case MODE_SYSTEM_SETTINGS:
                display_system_settings();
                break;
        }
        
        // 動作確認ドット
        static int dot_counter = 0;
        dot_counter++;
        M5.Display.setCursor(120, 120);
        M5.Display.setTextColor(dot_counter % 10 < 5 ? BLUE : BLACK);
        M5.Display.print("*");
    }
}

void display_joystick_monitor() {
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(GREEN);
    M5.Display.println("Joystick");
    M5.Display.println("Monitor");
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(WHITE);
    M5.Display.println("-------");
    
    if (joystick_data.valid) {
        M5.Display.printf("L_X:%4d\n", joystick_data.left_x);
        M5.Display.printf("L_Y:%4d\n", joystick_data.left_y);
        M5.Display.printf("R_X:%4d\n", joystick_data.right_x);
        M5.Display.printf("R_Y:%4d\n", joystick_data.right_y);
        
        M5.Display.setTextColor(joystick_data.left_stick_button ? RED : WHITE);
        M5.Display.printf("L_BTN:%s\n", joystick_data.left_stick_button ? "ON" : "OFF");
        M5.Display.setTextColor(joystick_data.right_stick_button ? RED : WHITE);
        M5.Display.printf("R_BTN:%s\n", joystick_data.right_stick_button ? "ON" : "OFF");
    } else {
        M5.Display.setTextColor(RED);
        M5.Display.println("No data");
    }
    
    M5.Display.setTextColor(CYAN);
    M5.Display.printf("Seq:%d\n", joystick_data.sequence);
}

void display_network_status() {
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(YELLOW);
    M5.Display.println("Network");
    M5.Display.println("Status");
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(WHITE);
    M5.Display.println("-------");
    
    M5.Display.printf("AP:%s\n", wifi_config.ssid);
    M5.Display.setTextColor(wifi_connected ? GREEN : RED);
    M5.Display.printf("St:%s\n", wifi_connected ? "ON" : "OFF");
    M5.Display.setTextColor(WHITE);
    M5.Display.printf("IP:%s\n", WiFi.softAPIP().toString().c_str());
    M5.Display.printf("Cl:%d/8\n", WiFi.softAPgetStationNum());
    M5.Display.printf("To:%s\n", udp_config.target_ip.toString().c_str());
}

void display_udp_communication() {
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(CYAN);
    M5.Display.println("UDP");
    M5.Display.println("Status");
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(WHITE);
    M5.Display.println("------");
    
    M5.Display.printf("Pt:%d\n", udp_config.port);
    M5.Display.printf("33Hz\n");
    M5.Display.setTextColor(GREEN);
    M5.Display.printf("OK:%d\n", udp_success_count);
    M5.Display.setTextColor(RED);
    M5.Display.printf("ER:%d\n", udp_error_count);
    M5.Display.setTextColor(WHITE);
    
    float success_rate = (udp_success_count + udp_error_count > 0) ? 
                        (100.0 * udp_success_count / (udp_success_count + udp_error_count)) : 0;
    M5.Display.printf("%.1f%%\n", success_rate);
    
    M5.Display.printf("Sq:%d\n", udp_sequence);
}

void display_system_settings() {
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(MAGENTA);
    M5.Display.println("System");
    M5.Display.println("Config");
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(WHITE);
    M5.Display.println("------");
    
    M5.Display.printf("Ph:4.7\n");
    M5.Display.printf("Md:%d/4\n", current_mode + 1);
    M5.Display.printf("I2C:%02X\n", I2C_ADDRESS);
    M5.Display.printf("%dkHz\n", I2C_FREQUENCY / 1000);
    
    M5.Display.setTextColor(GREEN);
    M5.Display.printf("B1:%d\n", joystick_data.battery_voltage1);
    M5.Display.printf("B2:%d\n", joystick_data.battery_voltage2);
    
    M5.Display.setTextColor(WHITE);
    M5.Display.printf("%ds\n", millis() / 1000);
}

// ========== MQTT統合配信処理 ==========

/**
 * @brief Joystick状態をMQTT配信（モード別処理）
 */
void publish_joystick_to_mqtt() {
    // Joystick値を正規化（-1.0 ~ 1.0）
    float left_x_norm = (joystick_data.left_x - 2048.0f) / 2048.0f;
    float left_y_norm = (joystick_data.left_y - 2048.0f) / 2048.0f;
    float right_x_norm = (joystick_data.right_x - 2048.0f) / 2048.0f;
    float right_y_norm = (joystick_data.right_y - 2048.0f) / 2048.0f;
    
    // 現在のモードに応じたMQTT配信
    switch (current_mode) {
        case MODE_JOYSTICK_MONITOR:  // Control モード
            publish_control_mode_mqtt(left_x_norm, left_y_norm, right_x_norm, right_y_norm);
            break;
            
        case MODE_NETWORK_STATUS:    // Video モード
            publish_video_mode_mqtt(left_x_norm, left_y_norm, right_x_norm, right_y_norm);
            break;
            
        case MODE_UDP_COMMUNICATION: // Adjust モード
            publish_adjust_mode_mqtt(left_x_norm, left_y_norm, right_x_norm, right_y_norm);
            break;
            
        case MODE_SYSTEM_SETTINGS:   // System モード
            publish_system_mode_mqtt(left_x_norm, left_y_norm, right_x_norm, right_y_norm);
            break;
    }
}

/**
 * @brief Control モード MQTT配信
 */
void publish_control_mode_mqtt(float left_x, float left_y, float right_x, float right_y) {
    // Left stick Y軸 → LED明度制御 (0-255)
    int brightness = (int)((left_y + 1.0f) * 127.5f); // -1~1 を 0~255 に変換
    brightness = constrain(brightness, 0, 255);
    mqtt_manager.publishBrightness(brightness);
    
    // Left stick X軸 → 色温度制御 (2700K-6500K)  
    int color_temp = (int)(2700 + (left_x + 1.0f) * 1900); // -1~1 を 2700~6500 に変換
    color_temp = constrain(color_temp, 2700, 6500);
    mqtt_manager.publishColorTemp(color_temp);
    
    // Right stick → 球体回転制御
    mqtt_manager.publishRotationX(right_x);
    mqtt_manager.publishRotationY(right_y);
    
    // ボタン → 再生制御（エッジ検出による重複配信防止）
    static bool prev_left_button = false;
    static bool prev_right_button = false;
    
    // 左ボタン押下エッジ検出
    if (joystick_data.left_button && !prev_left_button) {
        mqtt_manager.publishPlayback(true);  // 再生開始（押下時のみ）
    }
    // 右ボタン押下エッジ検出  
    if (joystick_data.right_button && !prev_right_button) {
        mqtt_manager.publishPlayback(false); // 再生停止（押下時のみ）
    }
    
    // 前回状態保存
    prev_left_button = joystick_data.left_button;
    prev_right_button = joystick_data.right_button;
}

/**
 * @brief Video モード MQTT配信
 */
void publish_video_mode_mqtt(float left_x, float left_y, float right_x, float right_y) {
    // Left stick Y軸 → 動画選択（ID 0-10）
    int video_id = (int)((left_y + 1.0f) * 5.0f); // -1~1 を 0~10 に変換
    video_id = constrain(video_id, 0, 10);
    mqtt_manager.publishSelectedVideoId(video_id);
    
    // Left stick X軸 → 音量制御 (0-100)
    int volume = (int)((left_x + 1.0f) * 50.0f); // -1~1 を 0~100 に変換  
    volume = constrain(volume, 0, 100);
    mqtt_manager.publishVolume(volume);
    
    // Right stick X軸 → シーク制御（秒単位）
    int seek_pos = (int)((right_x + 1.0f) * 300.0f); // -1~1 を 0~600秒 に変換
    seek_pos = constrain(seek_pos, 0, 600);
    mqtt_manager.publishSeekPosition(seek_pos);
    
    // Right stick Y軸 → 再生速度制御 (0.5-2.0x)
    float speed = 0.5f + (right_y + 1.0f) * 0.75f; // -1~1 を 0.5~2.0 に変換
    speed = constrain(speed, 0.5f, 2.0f);
    mqtt_manager.publishPlaybackSpeed(speed);
}

/**
 * @brief Adjust モード MQTT配信
 */
void publish_adjust_mode_mqtt(float left_x, float left_y, float right_x, float right_y) {
    // Left stick Y軸 → パラメータ選択 (0-4)
    int param_index = (int)((left_y + 1.0f) * 2.5f); // -1~1 を 0~4 に変換
    param_index = constrain(param_index, 0, 4);
    mqtt_manager.publishSelectedParameter(param_index);
    
    // Left stick X軸 → 選択パラメータの値調整 (0-255)
    int param_value = (int)((left_x + 1.0f) * 127.5f); // -1~1 を 0~255 に変換
    param_value = constrain(param_value, 0, 255);
    mqtt_manager.publishParameterValue(param_index, param_value);
    
    // Right stick → 他のパラメータの微調整
    if (abs(right_x) > 0.1f || abs(right_y) > 0.1f) {
        // Right stick操作時は他のパラメータも調整
        for (int i = 0; i < 5; i++) {
            if (i != param_index) {
                int fine_value = (int)(128 + right_x * 64 + right_y * 32); // 微調整値
                fine_value = constrain(fine_value, 0, 255);
                mqtt_manager.publishParameterValue(i, fine_value);
            }
        }
    }
}

/**
 * @brief System モード MQTT配信
 */
void publish_system_mode_mqtt(float left_x, float left_y, float right_x, float right_y) {
    // System モードでは主に監視情報を配信
    static unsigned long last_system_publish = 0;
    
    if (millis() - last_system_publish > 2000) { // 2秒間隔
        // CPU温度（仮想値）
        float cpu_temp = 40.0f + (millis() % 20000) / 1000.0f; // 40-60度の範囲でシミュレート
        mqtt_manager.publishCPUTemp(cpu_temp);
        
        // WiFi接続クライアント数
        mqtt_manager.publishWiFiClients(WiFi.softAPgetStationNum());
        
        // 稼働時間
        mqtt_manager.publishUptime(millis());
        
        last_system_publish = millis();
    }
}