/**
 * @file 06_udp_messaging.ino
 * @brief M5Stack Atom-JoyStick UDP軽量メッセージングシステム
 * @description isolation-sphere UDP基盤統合メッセージングハブ
 * 
 * Phase 3: UDP統合メッセージングシステム
 * - TCP MQTT → UDP軽量プロトコル変更
 * - Topic階層システム維持
 * - 15-30ms応答性実現
 * - 既存UDP通信システム統合
 * 
 * @target M5Stack Atom-JoyStick (ESP32-S3)
 * @protocol UDP Port 8080 (HTTP代替、MQTT競合回避)
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
const int UDP_PORT = 8080;  // HTTP代替ポート（1883 UDP競合回避）
const int MAX_CLIENTS = 8;
const int MESSAGE_BUFFER_SIZE = 512;
const int HEARTBEAT_INTERVAL = 10000;  // 10秒間隔
const int CLIENT_TIMEOUT = 30000;      // 30秒タイムアウト

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

// Topic管理システム（継承）
struct MessageTopic {
    String topic;
    String payload;
    bool retain;
    unsigned long timestamp;
};

const int MAX_TOPICS = 32;
MessageTopic topic_store[MAX_TOPICS];
int topic_count = 0;

// システム状態管理
struct UDPSystemStatus {
    bool server_active;
    int total_messages;
    int heartbeat_count;
    unsigned long uptime_sec;
    String last_topic;
    String last_payload;
    String last_client_ip;
    // デバッグ用統計
    int parse_packet_calls;
    int parse_packet_success;
} udp_status;

// デバッグ表示モード（継承）
enum DebugMode {
    DEBUG_SYSTEM,    // システム情報
    DEBUG_WIFI,      // WiFi情報
    DEBUG_UDP,       // UDP情報（変更）
    DEBUG_TOPICS,    // Topic監視
    DEBUG_MODE_COUNT
};

DebugMode current_mode = DEBUG_SYSTEM;
unsigned long last_update = 0;
unsigned long mode_change_time = 0;
bool mode_changed = false;
unsigned long last_heartbeat = 0;

void setup() {
    // M5Unified初期化
    auto cfg = M5.config();
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=================================");
    Serial.println("M5Stack Atom-JoyStick UDP Hub");
    Serial.println("=================================");
    Serial.println("Phase 3: UDP統合メッセージング");
    
    // LCD初期化
    M5.Display.clear(BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(WHITE);
    M5.Display.setRotation(0);
    
    // WiFi + UDP初期化
    init_wifi_access_point();
    init_udp_messaging();
    
    // 初期Topic登録
    init_default_topics();
    
    Serial.println("✅ UDP統合メッセージングシステム初期化完了");
    Serial.println("操作: ボタンAでモード切り替え、ボタンBで詳細表示");
    
    last_update = millis();
    last_heartbeat = millis();
}

void loop() {
    M5.update();
    
    // UDP メッセージング処理
    handle_udp_messaging();
    
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
    
    delay(10);  // UDP処理効率化
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
        udp_status.server_active = true;
    } else {
        Serial.println("❌ UDPサーバー起動失敗");
        udp_status.server_active = false;
    }
    
    // クライアント配列初期化
    for (int i = 0; i < MAX_CLIENTS; i++) {
        udp_clients[i].active = false;
        udp_clients[i].client_id = "";
        udp_clients[i].last_seen = 0;
    }
    
    // システム状態初期化
    udp_status.total_messages = 0;
    udp_status.heartbeat_count = 0;
    udp_status.parse_packet_calls = 0;
    udp_status.parse_packet_success = 0;
    
    Serial.printf("✅ 最大クライアント数: %d\n", MAX_CLIENTS);
    Serial.printf("✅ ハートビート間隔: %d秒\n", HEARTBEAT_INTERVAL / 1000);
}

void init_default_topics() {
    Serial.println("デフォルトTopic初期化...");
    
    // システム管理Topic
    add_topic("isolation-sphere/system/status", "online", true);
    add_topic("isolation-sphere/system/protocol", "udp-messaging", true);
    add_topic("isolation-sphere/system/uptime", "0", false);
    add_topic("isolation-sphere/joystick/mode", "system", true);
    
    // UI状態管理Topic
    add_topic("isolation-sphere/ui/brightness", "50", true);
    add_topic("isolation-sphere/ui/volume", "75", true);
    
    Serial.printf("✅ デフォルトTopic登録完了: %d個\n", topic_count);
}

void handle_udp_messaging() {
    // parsePacket()呼び出し監視
    udp_status.parse_packet_calls++;
    int packet_size = udp_server.parsePacket();
    
    // 10回に1回、parsePacket()の動作状況をログ出力
    static int log_counter = 0;
    if (++log_counter >= 10) {
        Serial.printf("DEBUG: parsePacket() 統計 - 呼び出し:%d 成功:%d (成功率:%.1f%%)\n", 
                      udp_status.parse_packet_calls, 
                      udp_status.parse_packet_success,
                      udp_status.parse_packet_calls > 0 ? 
                      (float)udp_status.parse_packet_success * 100.0f / udp_status.parse_packet_calls : 0.0f);
        log_counter = 0;
    }
    
    if (packet_size > 0) {
        udp_status.parse_packet_success++;
        
        // UDPパケット受信
        IPAddress client_ip = udp_server.remoteIP();
        uint16_t client_port = udp_server.remotePort();
        
        Serial.printf("📩 UDP受信 [%s:%d] サイズ:%d bytes\n", 
                      client_ip.toString().c_str(), client_port, packet_size);
        
        int len = udp_server.read(udp_buffer, MESSAGE_BUFFER_SIZE - 1);
        udp_buffer[len] = 0;  // NULL終端
        
        String message = String(udp_buffer);
        message.trim();
        
        Serial.printf("📩 メッセージ内容: '%s' (長さ:%d)\n", 
                      message.c_str(), message.length());
        
        if (message.length() > 0) {
            udp_status.total_messages++;
            udp_status.last_client_ip = client_ip.toString();
            
            Serial.printf("📝 処理開始: クライアント管理・メッセージ処理\n");
            
            // クライアント管理・メッセージ処理
            handle_client_message(client_ip, client_port, message);
        } else {
            Serial.println("⚠️ 警告: 空のメッセージを受信");
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
        udp_status.last_topic = topic;
        udp_status.last_payload = payload;
        
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
        udp_status.last_topic = topic;
        udp_status.last_payload = payload;
        
        Serial.printf("📝 簡易Topic: %s = %s\n", topic.c_str(), payload.c_str());
        
        broadcast_topic(client_index, topic, payload);
    }
}

void send_welcome_message(int client_index) {
    String welcome = "WELCOME:IsolationSphere UDP Hub";
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
        udp_status.heartbeat_count++;
        
        // システム状態ブロードキャスト
        String heartbeat = "HEARTBEAT:" + String(udp_status.heartbeat_count);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (udp_clients[i].active) {
                send_response(i, heartbeat);
            }
        }
        
        Serial.printf("💓 ハートビート送信: %d\n", udp_status.heartbeat_count);
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
    udp_status.uptime_sec = millis() / 1000;
    
    // システム状態をTopicに反映
    update_topic("isolation-sphere/system/uptime", String(udp_status.uptime_sec));
    update_topic("isolation-sphere/system/clients", String(connected_clients));
    update_topic("isolation-sphere/system/messages", String(udp_status.total_messages));
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
    
    // 長押し: UDP詳細情報表示（2秒）
    if (M5.BtnA.pressedFor(2000) && !long_press_executed) {
        Serial.println("DEBUG: ボタンA長押し検出 - UDP詳細情報表示");
        print_detailed_udp_info();
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
    
    M5.Display.printf("Up: %lus\n", udp_status.uptime_sec);
    M5.Display.printf("Mem:%uKB\n", ESP.getFreeHeap() / 1024);
    
    uint16_t udp_color = udp_status.server_active ? GREEN : RED;
    M5.Display.setTextColor(udp_color);
    M5.Display.printf("UDP:%s\n", udp_status.server_active ? "ON" : "OFF");
    M5.Display.setTextColor(WHITE);
}

void display_wifi_info() {
    M5.Display.setTextColor(CYAN);
    M5.Display.println("WiFi AP");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("------");
    
    M5.Display.printf("Dev:%d/8\n", WiFi.softAPgetStationNum());
    M5.Display.printf("IP:100.1\n");
    M5.Display.setTextColor(GREEN);
    M5.Display.println("ON");
    M5.Display.setTextColor(WHITE);
}

void display_udp_info() {
    M5.Display.setTextColor(MAGENTA);
    M5.Display.println("UDP Msg");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("-------");
    
    M5.Display.printf("P:8080\n");
    M5.Display.printf("C:%d/%d\n", connected_clients, MAX_CLIENTS);
    M5.Display.printf("M:%d\n", udp_status.total_messages);
    
    uint16_t status_color = (connected_clients > 0) ? GREEN : YELLOW;
    M5.Display.setTextColor(status_color);
    M5.Display.println(connected_clients > 0 ? "Active" : "Wait");
    M5.Display.setTextColor(WHITE);
}

void display_topics_info() {
    M5.Display.setTextColor(ORANGE);
    M5.Display.println("Topics");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("------");
    
    M5.Display.printf("T:%d/%d\n", topic_count, MAX_TOPICS);
    
    if (udp_status.last_topic.length() > 0) {
        M5.Display.println("Last:");
        String short_topic = udp_status.last_topic;
        if (short_topic.length() > 6) {
            short_topic = short_topic.substring(0, 6) + "..";
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
        case DEBUG_TOPICS: return "TOPICS";
        default: return "UNKNOWN";
    }
}

void print_detailed_udp_info() {
    Serial.println("\n=== UDP詳細デバッグ情報 ===");
    
    // 基本状態
    Serial.printf("UDPサーバー状態: %s\n", udp_status.server_active ? "✅ アクティブ" : "❌ 停止");
    Serial.printf("ポート番号: %d\n", UDP_PORT);
    Serial.printf("接続クライアント: %d/%d\n", connected_clients, MAX_CLIENTS);
    Serial.printf("総メッセージ数: %d\n", udp_status.total_messages);
    Serial.printf("ハートビート回数: %d\n", udp_status.heartbeat_count);
    
    // WiFi・ネットワーク状態
    Serial.println("\n--- ネットワーク状態 ---");
    Serial.printf("WiFi AP SSID: %s\n", AP_SSID);
    Serial.printf("ESP32 IPアドレス: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("WiFi接続デバイス数: %d/8\n", WiFi.softAPgetStationNum());
    Serial.printf("WiFi APモード: %s\n", WiFi.getMode() == WIFI_AP ? "✅ 正常" : "❌ 異常");
    
    // UDP詳細状態
    Serial.println("\n--- UDP詳細状態 ---");
    Serial.printf("UDPバッファサイズ: %d bytes\n", MESSAGE_BUFFER_SIZE);
    Serial.printf("クライアントタイムアウト: %d秒\n", CLIENT_TIMEOUT / 1000);
    Serial.printf("ハートビート間隔: %d秒\n", HEARTBEAT_INTERVAL / 1000);
    Serial.printf("最終クライアントIP: %s\n", 
                  udp_status.last_client_ip.length() > 0 ? udp_status.last_client_ip.c_str() : "なし");
    
    // parsePacket()統計
    Serial.println("\n--- parsePacket()統計 ---");
    Serial.printf("呼び出し回数: %d\n", udp_status.parse_packet_calls);
    Serial.printf("成功回数: %d\n", udp_status.parse_packet_success);
    Serial.printf("成功率: %.2f%%\n", 
                  udp_status.parse_packet_calls > 0 ? 
                  (float)udp_status.parse_packet_success * 100.0f / udp_status.parse_packet_calls : 0.0f);
    
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
    Serial.printf("システム稼働時間: %lu秒\n", millis() / 1000);
    Serial.printf("最大ヒープブロック: %u KB\n", ESP.getMaxAllocHeap() / 1024);
    
    Serial.println("========================\n");
}