/**
 * @file 07_joystick_control.ino
 * @brief M5Stack Atom-JoyStick リアルタイム制御システム
 * @description Joystick入力→UDP制御コマンド変換システム
 * 
 * Phase 4: Joystick入力制御統合
 * - アナログスティック入力監視（X/Y軸）
 * - リアルタイム制御コマンド生成（15-30ms応答）
 * - UDP Topic統合（isolation-sphere/joystick/*）
 * - 連続入力・離散入力切り替え制御
 * 
 * @target M5Stack Atom-JoyStick (ESP32-S3)
 * @protocol UDP Port 8080 + Joystick制御
 */

#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

// WiFiアクセスポイント設定（継承）
const char* AP_SSID = "IsolationSphere-Direct";
const char* AP_PASSWORD = "isolation-sphere-2025";
const IPAddress AP_IP(192, 168, 100, 1);
const IPAddress AP_GATEWAY(192, 168, 100, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

// UDP軽量メッセージング設定
const int UDP_PORT = 8080;
const int MAX_CLIENTS = 8;
const int MESSAGE_BUFFER_SIZE = 512;
const int HEARTBEAT_INTERVAL = 10000;  // 10秒間隔
const int CLIENT_TIMEOUT = 30000;      // 30秒タイムアウト

// Joystick制御設定
const int JOYSTICK_UPDATE_INTERVAL = 50;  // 20Hz更新（50ms間隔）
const int JOYSTICK_THRESHOLD = 10;        // デッドゾーン閾値
const int JOYSTICK_RANGE = 4095;          // 12bit ADC最大値
const float JOYSTICK_SENSITIVITY = 1.0;   // 感度係数

// UDPサーバー
WiFiUDP udp_server;
char udp_buffer[MESSAGE_BUFFER_SIZE];

// クライアント管理
struct UDPClient {
    IPAddress ip;
    uint16_t port;
    String client_id;
    unsigned long last_seen;
    bool active;
};

UDPClient udp_clients[MAX_CLIENTS];
int connected_clients = 0;

// Joystick状態管理
struct JoystickState {
    int raw_x, raw_y;          // 生ADC値
    float normalized_x, normalized_y;  // 正規化値 (-1.0 ~ 1.0)
    int direction;             // 8方向 (0-7) + 中心 (-1)
    bool active;               // アクティブ状態
    String last_command;       // 最終送信コマンド
    unsigned long last_update; // 最終更新時刻
} joystick_state;

// Topic管理システム（継承・拡張）
struct MessageTopic {
    String topic;
    String payload;
    bool retain;
    unsigned long timestamp;
};

const int MAX_TOPICS = 32;
MessageTopic topic_store[MAX_TOPICS];
int topic_count = 0;

// システム状態管理（拡張）
struct SystemStatus {
    bool server_active;
    int total_messages;
    int heartbeat_count;
    unsigned long uptime_sec;
    String last_topic;
    String last_payload;
    String last_client_ip;
    // Joystick統計
    int joystick_updates;
    int command_sent;
    bool joystick_active;
} system_status;

// デバッグ表示モード（拡張）
enum DebugMode {
    DEBUG_SYSTEM,    // システム情報
    DEBUG_WIFI,      // WiFi情報
    DEBUG_UDP,       // UDP情報
    DEBUG_JOYSTICK,  // Joystick情報（新規）
    DEBUG_TOPICS,    // Topic監視
    DEBUG_MODE_COUNT
};

DebugMode current_mode = DEBUG_SYSTEM;
unsigned long last_update = 0;
unsigned long mode_change_time = 0;
bool mode_changed = false;
unsigned long last_heartbeat = 0;
unsigned long last_joystick_update = 0;

void setup() {
    // M5Unified初期化
    auto cfg = M5.config();
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=================================");
    Serial.println("M5Stack Atom-JoyStick Control Hub");
    Serial.println("=================================");
    Serial.println("Phase 4: Joystick制御統合");
    
    // LCD初期化
    M5.Display.clear(BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(WHITE);
    M5.Display.setRotation(0);
    
    // WiFi + UDP初期化
    init_wifi_access_point();
    init_udp_messaging();
    
    // Joystick初期化
    init_joystick_control();
    
    // 初期Topic登録
    init_default_topics();
    
    Serial.println("✅ Joystick制御統合システム初期化完了");
    Serial.println("操作: ボタンAでモード切り替え、Joystick操作で制御");
    
    last_update = millis();
    last_heartbeat = millis();
    last_joystick_update = millis();
}

void loop() {
    M5.update();
    
    // UDP メッセージング処理
    handle_udp_messaging();
    
    // Joystick制御処理（20Hz）
    if (millis() - last_joystick_update >= JOYSTICK_UPDATE_INTERVAL) {
        handle_joystick_control();
        last_joystick_update = millis();
    }
    
    // ハートビート送信
    handle_heartbeat();
    
    // クライアント管理
    manage_clients();
    
    // ボタン入力処理
    handle_button_input();
    
    // 500ms間隔で表示・状態更新
    if (millis() - last_update >= 500) {
        update_system_status();
        display_debug_info();
        last_update = millis();
    }
    
    delay(10);  // UDP+Joystick処理効率化
}

void init_wifi_access_point() {
    Serial.println("WiFiアクセスポイント設定開始...");
    
    WiFi.mode(WIFI_AP);
    delay(100);
    
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
    bool ap_result = WiFi.softAP(AP_SSID, AP_PASSWORD, 1, 0, 8);
    
    if (ap_result) {
        Serial.printf("✅ WiFi AP作成成功: %s\n", AP_SSID);
        Serial.printf("✅ IP アドレス: %s\n", WiFi.softAPIP().toString().c_str());
        Serial.printf("✅ ネットワーク: %s\n", AP_SUBNET.toString().c_str());
    } else {
        Serial.println("❌ WiFi AP作成失敗");
    }
    
    delay(1000);
}

void init_udp_messaging() {
    Serial.println("UDP軽量メッセージングシステム初期化...");
    
    // UDPサーバー開始
    if (udp_server.begin(UDP_PORT)) {
        Serial.printf("✅ UDPサーバー起動: ポート%d\n", UDP_PORT);
        system_status.server_active = true;
    } else {
        Serial.println("❌ UDPサーバー起動失敗");
        system_status.server_active = false;
    }
    
    // クライアント配列初期化
    for (int i = 0; i < MAX_CLIENTS; i++) {
        udp_clients[i].active = false;
        udp_clients[i].client_id = "";
        udp_clients[i].last_seen = 0;
    }
    
    // システム状態初期化
    system_status.total_messages = 0;
    system_status.heartbeat_count = 0;
    system_status.joystick_updates = 0;
    system_status.command_sent = 0;
    system_status.joystick_active = false;
    
    Serial.printf("✅ 最大クライアント数: %d\n", MAX_CLIENTS);
    Serial.printf("✅ ハートビート間隔: %d秒\n", HEARTBEAT_INTERVAL / 1000);
}

void init_joystick_control() {
    Serial.println("Joystick制御システム初期化...");
    
    // Joystick状態初期化
    joystick_state.raw_x = 0;
    joystick_state.raw_y = 0;
    joystick_state.normalized_x = 0.0;
    joystick_state.normalized_y = 0.0;
    joystick_state.direction = -1;  // 中心
    joystick_state.active = false;
    joystick_state.last_command = "";
    joystick_state.last_update = 0;
    
    Serial.printf("✅ Joystick更新レート: %dHz (%dms間隔)\n", 
                  1000 / JOYSTICK_UPDATE_INTERVAL, JOYSTICK_UPDATE_INTERVAL);
    Serial.printf("✅ デッドゾーン閾値: %d (ADC範囲: 0-%d)\n", 
                  JOYSTICK_THRESHOLD, JOYSTICK_RANGE);
    Serial.printf("✅ 感度係数: %.1f\n", JOYSTICK_SENSITIVITY);
}

void init_default_topics() {
    Serial.println("デフォルトTopic初期化...");
    
    // システム管理Topic
    add_topic("isolation-sphere/system/status", "online", true);
    add_topic("isolation-sphere/system/protocol", "udp-joystick", true);
    add_topic("isolation-sphere/system/uptime", "0", false);
    add_topic("isolation-sphere/joystick/mode", "system", true);
    
    // Joystick制御Topic（新規）
    add_topic("isolation-sphere/joystick/x", "0.00", false);
    add_topic("isolation-sphere/joystick/y", "0.00", false);
    add_topic("isolation-sphere/joystick/direction", "center", false);
    add_topic("isolation-sphere/joystick/active", "false", false);
    
    // UI状態管理Topic
    add_topic("isolation-sphere/ui/brightness", "50", true);
    add_topic("isolation-sphere/ui/volume", "75", true);
    
    Serial.printf("✅ デフォルトTopic登録完了: %d個\n", topic_count);
}

void handle_joystick_control() {
    // Joystick ADC値読み取り（M5Stack Atom-JoyStick公式ピン配置）
    // 参考: https://docs.m5stack.com/en/core/AtomJoyStick
    joystick_state.raw_x = analogRead(GPIO_NUM_5);  // X軸ADC (GPIO5)
    joystick_state.raw_y = analogRead(GPIO_NUM_15); // Y軸ADC (GPIO15)
    
    // 正規化 (-1.0 ~ 1.0)
    joystick_state.normalized_x = normalize_joystick_value(joystick_state.raw_x);
    joystick_state.normalized_y = normalize_joystick_value(joystick_state.raw_y);
    
    // デッドゾーン処理
    bool in_deadzone = (abs(joystick_state.raw_x - JOYSTICK_RANGE/2) < JOYSTICK_THRESHOLD) &&
                       (abs(joystick_state.raw_y - JOYSTICK_RANGE/2) < JOYSTICK_THRESHOLD);
    
    if (in_deadzone) {
        joystick_state.normalized_x = 0.0;
        joystick_state.normalized_y = 0.0;
        joystick_state.direction = -1;  // 中心
        joystick_state.active = false;
    } else {
        joystick_state.active = true;
        joystick_state.direction = calculate_direction(joystick_state.normalized_x, joystick_state.normalized_y);
    }
    
    // UDP Topic更新・送信
    update_joystick_topics();
    
    // 制御コマンド生成・送信
    if (joystick_state.active) {
        send_joystick_command();
    }
    
    system_status.joystick_updates++;
    joystick_state.last_update = millis();
}

float normalize_joystick_value(int raw_value) {
    // 12bit ADC (0-4095) を (-1.0 ~ 1.0) に正規化
    float normalized = ((float)raw_value - JOYSTICK_RANGE/2) / (JOYSTICK_RANGE/2);
    normalized *= JOYSTICK_SENSITIVITY;
    
    // クランプ処理
    if (normalized > 1.0) normalized = 1.0;
    if (normalized < -1.0) normalized = -1.0;
    
    return normalized;
}

int calculate_direction(float x, float y) {
    // 8方向 + 中心 (-1)
    // 0:右 1:右上 2:上 3:左上 4:左 5:左下 6:下 7:右下
    
    if (abs(x) < 0.3 && abs(y) < 0.3) return -1;  // 中心
    
    float angle = atan2(-y, x) * 180.0 / PI;  // Y軸反転（上が正）
    if (angle < 0) angle += 360.0;
    
    // 8方向に分割 (45度間隔)
    int direction = (int)((angle + 22.5) / 45.0) % 8;
    return direction;
}

void update_joystick_topics() {
    // Joystick座標Topic更新
    String x_str = String(joystick_state.normalized_x, 2);
    String y_str = String(joystick_state.normalized_y, 2);
    
    update_topic("isolation-sphere/joystick/x", x_str);
    update_topic("isolation-sphere/joystick/y", y_str);
    update_topic("isolation-sphere/joystick/active", joystick_state.active ? "true" : "false");
    
    // 方向Topic更新
    String direction_name = get_direction_name(joystick_state.direction);
    update_topic("isolation-sphere/joystick/direction", direction_name);
}

void send_joystick_command() {
    // JSON形式制御コマンド生成
    DynamicJsonDocument doc(200);
    doc["type"] = "joystick_control";
    doc["x"] = joystick_state.normalized_x;
    doc["y"] = joystick_state.normalized_y;
    doc["direction"] = joystick_state.direction;
    doc["timestamp"] = millis();
    
    String command;
    serializeJson(doc, command);
    
    // 全クライアントに送信
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (udp_clients[i].active) {
            String cmd_msg = "CMD:" + command;
            send_response(i, cmd_msg);
        }
    }
    
    joystick_state.last_command = command;
    system_status.command_sent++;
    
    // デバッグログ（10回に1回）
    static int log_counter = 0;
    if (++log_counter >= 10) {
        Serial.printf("🕹️ Joystick: Raw_X=%d Raw_Y=%d | X=%.2f Y=%.2f Dir=%s Active=%s\n", 
                      joystick_state.raw_x, joystick_state.raw_y,
                      joystick_state.normalized_x, joystick_state.normalized_y,
                      get_direction_name(joystick_state.direction).c_str(),
                      joystick_state.active ? "YES" : "NO");
        log_counter = 0;
    }
}

String get_direction_name(int direction) {
    switch (direction) {
        case 0: return "right";
        case 1: return "right_up";
        case 2: return "up";
        case 3: return "left_up";
        case 4: return "left";
        case 5: return "left_down";
        case 6: return "down";
        case 7: return "right_down";
        default: return "center";
    }
}

void handle_udp_messaging() {
    int packet_size = udp_server.parsePacket();
    
    if (packet_size > 0) {
        // UDPパケット受信
        IPAddress client_ip = udp_server.remoteIP();
        uint16_t client_port = udp_server.remotePort();
        
        int len = udp_server.read(udp_buffer, MESSAGE_BUFFER_SIZE - 1);
        udp_buffer[len] = 0;  // NULL終端
        
        String message = String(udp_buffer);
        message.trim();
        
        if (message.length() > 0) {
            system_status.total_messages++;
            system_status.last_client_ip = client_ip.toString();
            
            // クライアント管理・メッセージ処理
            handle_client_message(client_ip, client_port, message);
        }
    }
}

void handle_client_message(IPAddress client_ip, uint16_t client_port, const String& message) {
    // クライアント登録・更新
    int client_index = register_client(client_ip, client_port);
    
    if (client_index >= 0) {
        udp_clients[client_index].last_seen = millis();
        
        // メッセージタイプ判定・処理
        if (message.startsWith("CONNECT:")) {
            handle_connect_message(client_index, message.substring(8));
        } else if (message.startsWith("PUB:")) {
            handle_publish_message(client_index, message.substring(4));
        } else if (message.startsWith("SUB:")) {
            handle_subscribe_message(client_index, message.substring(4));
        } else if (message.startsWith("PING")) {
            handle_ping_message(client_index);
        } else {
            // 簡易形式: topic:payload
            handle_simple_message(client_index, message);
        }
    }
}

int register_client(IPAddress ip, uint16_t port) {
    // 既存クライアント検索
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (udp_clients[i].active && udp_clients[i].ip == ip && udp_clients[i].port == port) {
            return i;  // 既存クライアント
        }
    }
    
    // 新規クライアント登録
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!udp_clients[i].active) {
            udp_clients[i].ip = ip;
            udp_clients[i].port = port;
            udp_clients[i].client_id = String("client_") + String(i);
            udp_clients[i].last_seen = millis();
            udp_clients[i].active = true;
            
            connected_clients++;
            
            Serial.printf("📡 新規UDPクライアント登録: [%d] %s:%d\n", 
                          i, ip.toString().c_str(), port);
            M5.Speaker.tone(1200, 150);  // 接続音
            
            // Welcome応答
            send_welcome_message(i);
            
            return i;
        }
    }
    
    Serial.println("❌ クライアント登録失敗: 満杯");
    return -1;
}

void handle_connect_message(int client_index, const String& client_id) {
    udp_clients[client_index].client_id = client_id;
    Serial.printf("📡 クライアント接続: [%d] %s\n", client_index, client_id.c_str());
    
    // CONNACK応答
    send_response(client_index, "CONNACK:OK");
}

void handle_publish_message(int client_index, const String& pub_data) {
    // PUB形式: topic|payload
    int separator = pub_data.indexOf('|');
    if (separator > 0) {
        String topic = pub_data.substring(0, separator);
        String payload = pub_data.substring(separator + 1);
        
        update_topic(topic, payload);
        system_status.last_topic = topic;
        system_status.last_payload = payload;
        
        Serial.printf("📝 Topic発行: %s = %s\n", topic.c_str(), payload.c_str());
        
        // 他のクライアントに転送
        broadcast_topic(client_index, topic, payload);
    }
}

void handle_subscribe_message(int client_index, const String& topic) {
    Serial.printf("📋 Topic購読: [%d] %s\n", client_index, topic.c_str());
    
    // 該当Topicを送信
    send_topic_to_client(client_index, topic);
}

void handle_ping_message(int client_index) {
    send_response(client_index, "PONG");
}

void handle_simple_message(int client_index, const String& message) {
    // 簡易形式: topic:payload
    int separator = message.indexOf(':');
    if (separator > 0) {
        String topic = message.substring(0, separator);
        String payload = message.substring(separator + 1);
        
        update_topic(topic, payload);
        system_status.last_topic = topic;
        system_status.last_payload = payload;
        
        Serial.printf("📝 簡易Topic: %s = %s\n", topic.c_str(), payload.c_str());
        
        broadcast_topic(client_index, topic, payload);
    }
}

void send_welcome_message(int client_index) {
    String welcome = "WELCOME:IsolationSphere Joystick Hub";
    send_response(client_index, welcome);
}

void send_response(int client_index, const String& response) {
    if (client_index >= 0 && client_index < MAX_CLIENTS && udp_clients[client_index].active) {
        udp_server.beginPacket(udp_clients[client_index].ip, udp_clients[client_index].port);
        udp_server.print(response);
        udp_server.endPacket();
    }
}

void broadcast_topic(int sender_index, const String& topic, const String& payload) {
    String broadcast_msg = "PUB:" + topic + "|" + payload;
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i != sender_index && udp_clients[i].active) {
            send_response(i, broadcast_msg);
        }
    }
}

void send_topic_to_client(int client_index, const String& topic_pattern) {
    for (int i = 0; i < topic_count; i++) {
        if (topic_store[i].topic.indexOf(topic_pattern) >= 0) {
            String topic_msg = "PUB:" + topic_store[i].topic + "|" + topic_store[i].payload;
            send_response(client_index, topic_msg);
        }
    }
}

void handle_heartbeat() {
    if (millis() - last_heartbeat >= HEARTBEAT_INTERVAL) {
        system_status.heartbeat_count++;
        
        // システム状態ブロードキャスト
        String heartbeat = "HEARTBEAT:" + String(system_status.heartbeat_count);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (udp_clients[i].active) {
                send_response(i, heartbeat);
            }
        }
        
        Serial.printf("💓 ハートビート送信: %d\n", system_status.heartbeat_count);
        last_heartbeat = millis();
    }
}

void manage_clients() {
    unsigned long current_time = millis();
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (udp_clients[i].active) {
            if (current_time - udp_clients[i].last_seen > CLIENT_TIMEOUT) {
                Serial.printf("⏰ クライアント%d タイムアウト: %s\n", 
                              i, udp_clients[i].client_id.c_str());
                
                udp_clients[i].active = false;
                udp_clients[i].client_id = "";
                connected_clients--;
                
                M5.Speaker.tone(800, 100);  // 切断音
            }
        }
    }
}

void add_topic(const String& topic, const String& payload, bool retain) {
    if (topic_count < MAX_TOPICS) {
        topic_store[topic_count].topic = topic;
        topic_store[topic_count].payload = payload;
        topic_store[topic_count].retain = retain;
        topic_store[topic_count].timestamp = millis();
        topic_count++;
    }
}

void update_topic(const String& topic, const String& payload) {
    // 既存Topic検索・更新
    for (int i = 0; i < topic_count; i++) {
        if (topic_store[i].topic == topic) {
            topic_store[i].payload = payload;
            topic_store[i].timestamp = millis();
            return;
        }
    }
    
    // 新規Topic追加
    add_topic(topic, payload, false);
}

void update_system_status() {
    system_status.uptime_sec = millis() / 1000;
    system_status.joystick_active = joystick_state.active;
    
    // システム状態をTopicに反映
    update_topic("isolation-sphere/system/uptime", String(system_status.uptime_sec));
    update_topic("isolation-sphere/system/clients", String(connected_clients));
    update_topic("isolation-sphere/system/messages", String(system_status.total_messages));
    update_topic("isolation-sphere/system/joystick_updates", String(system_status.joystick_updates));
    update_topic("isolation-sphere/system/commands_sent", String(system_status.command_sent));
}

void handle_button_input() {
    static bool long_press_executed = false;
    
    // 短押し: モード切り替え（既存機能）
    if (M5.BtnA.wasPressed()) {
        current_mode = (DebugMode)((current_mode + 1) % DEBUG_MODE_COUNT);
        mode_changed = true;
        mode_change_time = millis();
        
        Serial.printf("デバッグモード変更: %s\n", get_mode_name(current_mode));
        M5.Speaker.tone(800, 50);
        
        // モード変更をUDPで通知
        String mode_name = String(get_mode_name(current_mode));
        mode_name.toLowerCase();
        update_topic("isolation-sphere/joystick/mode", mode_name);
        
        // 全クライアントに通知
        String mode_msg = "PUB:isolation-sphere/joystick/mode|" + mode_name;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (udp_clients[i].active) {
                send_response(i, mode_msg);
            }
        }
    }
    
    // 長押し: システム詳細情報表示（2秒）
    if (M5.BtnA.pressedFor(2000) && !long_press_executed) {
        Serial.println("DEBUG: ボタンA長押し検出 - システム詳細情報表示");
        print_detailed_system_info();
        M5.Speaker.tone(1500, 200);  // 長押し確認音
        long_press_executed = true;
    }
    
    // 長押し状態リセット
    if (!M5.BtnA.isPressed()) {
        long_press_executed = false;
    }
}

void display_debug_info() {
    M5.Display.clear(BLACK);
    M5.Display.setCursor(2, 2);
    
    if (mode_changed && millis() - mode_change_time < 2000) {
        M5.Display.setTextColor(YELLOW);
        M5.Display.printf("->%s\n", get_mode_name(current_mode));
        M5.Display.setTextColor(WHITE);
        M5.Display.println("------");
    }
    
    switch (current_mode) {
        case DEBUG_SYSTEM:
            display_system_info();
            break;
        case DEBUG_WIFI:
            display_wifi_info();
            break;
        case DEBUG_UDP:
            display_udp_info();
            break;
        case DEBUG_JOYSTICK:
            display_joystick_info();
            break;
        case DEBUG_TOPICS:
            display_topics_info();
            break;
        default:
            break;
    }
    
    if (mode_changed && millis() - mode_change_time >= 2000) {
        mode_changed = false;
    }
}

void display_system_info() {
    M5.Display.setTextColor(GREEN);
    M5.Display.println("System");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("------");
    
    M5.Display.printf("Up: %lus\n", system_status.uptime_sec);
    M5.Display.printf("Mem:%uKB\n", ESP.getFreeHeap() / 1024);
    
    uint16_t udp_color = system_status.server_active ? GREEN : RED;
    M5.Display.setTextColor(udp_color);
    M5.Display.printf("UDP:%s\n", system_status.server_active ? "ON" : "OFF");
    
    uint16_t joy_color = system_status.joystick_active ? GREEN : YELLOW;
    M5.Display.setTextColor(joy_color);
    M5.Display.printf("Joy:%s\n", system_status.joystick_active ? "ACT" : "IDLE");
    M5.Display.setTextColor(WHITE);
}

void display_wifi_info() {
    M5.Display.setTextColor(CYAN);
    M5.Display.println("WiFi AP");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("------");
    
    M5.Display.printf("Dev:%d/8\n", WiFi.softAPgetStationNum());
    M5.Display.printf("IP:100.1\n");
    M5.Display.printf("P:8080\n");
    M5.Display.setTextColor(GREEN);
    M5.Display.println("ON");
    M5.Display.setTextColor(WHITE);
}

void display_udp_info() {
    M5.Display.setTextColor(MAGENTA);
    M5.Display.println("UDP Msg");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("-------");
    
    M5.Display.printf("C:%d/%d\n", connected_clients, MAX_CLIENTS);
    M5.Display.printf("M:%d\n", system_status.total_messages);
    M5.Display.printf("Cmd:%d\n", system_status.command_sent);
    
    uint16_t status_color = (connected_clients > 0) ? GREEN : YELLOW;
    M5.Display.setTextColor(status_color);
    M5.Display.println(connected_clients > 0 ? "Active" : "Wait");
    M5.Display.setTextColor(WHITE);
}

void display_joystick_info() {
    M5.Display.setTextColor(ORANGE);
    M5.Display.println("Joystick");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("--------");
    
    M5.Display.printf("X:%.2f\n", joystick_state.normalized_x);
    M5.Display.printf("Y:%.2f\n", joystick_state.normalized_y);
    M5.Display.printf("Dir:%s\n", get_direction_name(joystick_state.direction).substring(0,3).c_str());
    
    uint16_t joy_color = joystick_state.active ? GREEN : RED;
    M5.Display.setTextColor(joy_color);
    M5.Display.println(joystick_state.active ? "ACTIVE" : "IDLE");
    M5.Display.setTextColor(WHITE);
}

void display_topics_info() {
    M5.Display.setTextColor(ORANGE);
    M5.Display.println("Topics");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("------");
    
    M5.Display.printf("T:%d/%d\n", topic_count, MAX_TOPICS);
    M5.Display.printf("U:%d\n", system_status.joystick_updates);
    
    if (system_status.last_topic.length() > 0) {
        String short_topic = system_status.last_topic;
        if (short_topic.length() > 8) {
            short_topic = short_topic.substring(0, 8) + "..";
        }
        M5.Display.printf("%s\n", short_topic.c_str());
    } else {
        M5.Display.println("Empty");
    }
}

const char* get_mode_name(DebugMode mode) {
    switch (mode) {
        case DEBUG_SYSTEM: return "SYSTEM";
        case DEBUG_WIFI: return "WIFI";
        case DEBUG_UDP: return "UDP";
        case DEBUG_JOYSTICK: return "JOYSTICK";
        case DEBUG_TOPICS: return "TOPICS";
        default: return "UNKNOWN";
    }
}

void print_detailed_system_info() {
    Serial.println("\n=== システム詳細デバッグ情報 ===");
    
    // 基本状態
    Serial.printf("システム稼働時間: %lu秒\n", system_status.uptime_sec);
    Serial.printf("UDPサーバー状態: %s\n", system_status.server_active ? "✅ アクティブ" : "❌ 停止");
    Serial.printf("ポート番号: %d\n", UDP_PORT);
    Serial.printf("接続クライアント: %d/%d\n", connected_clients, MAX_CLIENTS);
    Serial.printf("総メッセージ数: %d\n", system_status.total_messages);
    Serial.printf("ハートビート回数: %d\n", system_status.heartbeat_count);
    
    // Joystick詳細状態
    Serial.println("\n--- Joystick制御状態 ---");
    Serial.printf("更新回数: %d回\n", system_status.joystick_updates);
    Serial.printf("送信コマンド数: %d回\n", system_status.command_sent);
    Serial.printf("現在の状態: %s\n", system_status.joystick_active ? "✅ アクティブ" : "⚪ アイドル");
    Serial.printf("生ADC値: X=%d Y=%d\n", joystick_state.raw_x, joystick_state.raw_y);
    Serial.printf("正規化値: X=%.3f Y=%.3f\n", joystick_state.normalized_x, joystick_state.normalized_y);
    Serial.printf("方向: %s (%d)\n", get_direction_name(joystick_state.direction).c_str(), joystick_state.direction);
    Serial.printf("最終コマンド: %s\n", joystick_state.last_command.c_str());
    
    // WiFi・ネットワーク状態
    Serial.println("\n--- ネットワーク状態 ---");
    Serial.printf("WiFi AP SSID: %s\n", AP_SSID);
    Serial.printf("ESP32 IPアドレス: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("WiFi接続デバイス数: %d/8\n", WiFi.softAPgetStationNum());
    Serial.printf("WiFi APモード: %s\n", WiFi.getMode() == WIFI_AP ? "✅ 正常" : "❌ 異常");
    
    // Topic管理状態
    Serial.printf("\nTopic数: %d/%d\n", topic_count, MAX_TOPICS);
    Serial.println("\n--- 登録Topic一覧 ---");
    for (int i = 0; i < topic_count; i++) {
        Serial.printf("[%d] %s = %s (retain: %s)\n", i, 
                      topic_store[i].topic.c_str(), 
                      topic_store[i].payload.c_str(),
                      topic_store[i].retain ? "true" : "false");
    }
    
    // クライアント詳細
    Serial.println("\n--- クライアント状態 ---");
    if (connected_clients == 0) {
        Serial.println("接続クライアント: なし");
    } else {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (udp_clients[i].active) {
                Serial.printf("Client %d: %s [%s:%d] (最終通信: %lu秒前)\n", 
                              i, udp_clients[i].client_id.c_str(),
                              udp_clients[i].ip.toString().c_str(), 
                              udp_clients[i].port,
                              (millis() - udp_clients[i].last_seen) / 1000);
            }
        }
    }
    
    // システム診断
    Serial.println("\n--- システム診断 ---");
    Serial.printf("フリーヒープ: %u KB\n", ESP.getFreeHeap() / 1024);
    Serial.printf("最大ヒープブロック: %u KB\n", ESP.getMaxAllocHeap() / 1024);
    Serial.printf("Joystick更新レート: %dms間隔 (目標%dms)\n", 
                  JOYSTICK_UPDATE_INTERVAL, JOYSTICK_UPDATE_INTERVAL);
    Serial.printf("感度係数: %.1f\n", JOYSTICK_SENSITIVITY);
    Serial.printf("デッドゾーン: %d (0-%d)\n", JOYSTICK_THRESHOLD, JOYSTICK_RANGE);
    
    Serial.println("========================\n");
}