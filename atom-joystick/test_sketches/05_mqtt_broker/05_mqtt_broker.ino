/**
 * @file 05_mqtt_broker.ino
 * @brief M5Stack Atom-JoyStick MQTTブローカー実装
 * @description 分散MQTTシステム用軽量ブローカー機能
 * 
 * Phase 3: MQTT統合システム構築
 * - WiFi AP + MQTTブローカー統合
 * - Topic階層管理システム
 * - クライアント監視・管理
 * - UI・IMU状態同期基盤
 * 
 * @target M5Stack Atom-JoyStick (ESP32-S3)
 * @mqtt_port 1883
 */

#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <ArduinoJson.h>
#include <WiFiServer.h>
#include <WiFiClient.h>

// WiFiアクセスポイント設定（Phase 2から継承）
const char* AP_SSID = "IsolationSphere-Direct";
const char* AP_PASSWORD = "isolation-sphere-2025";
const IPAddress AP_IP(192, 168, 100, 1);
const IPAddress AP_GATEWAY(192, 168, 100, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

// MQTT設定
const int MQTT_PORT = 1883;
const int MAX_CLIENTS = 8;
const int MQTT_KEEP_ALIVE = 60;

// 軽量MQTTブローカー
WiFiServer mqtt_server(MQTT_PORT);
WiFiClient mqtt_clients[MAX_CLIENTS];
bool client_connected[MAX_CLIENTS];
String client_ids[MAX_CLIENTS];
unsigned long client_last_seen[MAX_CLIENTS];

// Topic階層システム
struct MQTTTopic {
    String topic;
    String payload;
    int qos;
    bool retain;
    unsigned long timestamp;
};

const int MAX_TOPICS = 32;
MQTTTopic topic_store[MAX_TOPICS];
int topic_count = 0;

// システム状態管理
struct MQTTSystemStatus {
    bool broker_active;
    int connected_clients;
    int total_messages;
    int topics_count;
    unsigned long uptime_sec;
    String last_topic;
    String last_payload;
} mqtt_status;

// デバッグ表示モード（Phase 2から拡張）
enum DebugMode {
    DEBUG_SYSTEM,    // システム情報
    DEBUG_WIFI,      // WiFi情報
    DEBUG_MQTT,      // MQTT情報（強化）
    DEBUG_TOPICS,    // Topic監視（新規）
    DEBUG_MODE_COUNT
};

DebugMode current_mode = DEBUG_SYSTEM;
unsigned long last_update = 0;
unsigned long mode_change_time = 0;
bool mode_changed = false;

void setup() {
    // M5Unified初期化
    auto cfg = M5.config();
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=================================");
    Serial.println("M5Stack Atom-JoyStick MQTT Hub");
    Serial.println("=================================");
    Serial.println("Phase 3: MQTT統合システム構築");
    
    // LCD初期化
    M5.Display.clear(BLACK);
    M5.Display.setTextSize(2);  // テキストサイズを2倍に拡大
    M5.Display.setTextColor(WHITE);
    M5.Display.setRotation(0);
    
    // WiFi + MQTT初期化
    init_wifi_access_point();
    init_mqtt_broker();
    
    // 初期Topic登録
    init_default_topics();
    
    Serial.println("✅ MQTT統合システム初期化完了");
    Serial.println("操作: ボタンAでモード切り替え、ボタンBで詳細表示");
    
    last_update = millis();
}

void loop() {
    M5.update();
    
    // MQTTブローカー処理
    handle_mqtt_broker();
    
    // ボタン入力処理
    handle_button_input();
    
    // 500ms間隔で表示・状態更新
    if (millis() - last_update >= 500) {
        update_system_status();
        display_debug_info();
        last_update = millis();
    }
    
    delay(10);  // MQTT処理のため短縮
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

void init_mqtt_broker() {
    Serial.println("MQTTブローカー初期化開始...");
    
    // MQTTサーバー開始
    mqtt_server.begin();
    Serial.printf("✅ MQTTブローカー起動: ポート%d\n", MQTT_PORT);
    
    // クライアント配列初期化
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_connected[i] = false;
        client_ids[i] = "";
        client_last_seen[i] = 0;
    }
    
    // システム状態初期化
    mqtt_status.broker_active = true;
    mqtt_status.connected_clients = 0;
    mqtt_status.total_messages = 0;
    mqtt_status.topics_count = 0;
    
    Serial.printf("✅ 最大クライアント数: %d\n", MAX_CLIENTS);
    Serial.printf("✅ Keep-Alive: %d秒\n", MQTT_KEEP_ALIVE);
}

void init_default_topics() {
    Serial.println("デフォルトTopic初期化...");
    
    // システム管理Topic
    add_topic("isolation-sphere/system/status", "online", 0, true);
    add_topic("isolation-sphere/system/uptime", "0", 0, false);
    add_topic("isolation-sphere/joystick/mode", "system", 0, true);
    
    // UI状態管理Topic（準備）
    add_topic("isolation-sphere/ui/brightness", "50", 0, true);
    add_topic("isolation-sphere/ui/volume", "75", 0, true);
    
    Serial.printf("✅ デフォルトTopic登録完了: %d個\n", topic_count);
}

void handle_mqtt_broker() {
    // 新規クライアント接続処理
    WiFiClient new_client = mqtt_server.available();
    if (new_client) {
        handle_new_client(new_client);
    }
    
    // 既存クライアント処理
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_connected[i] && mqtt_clients[i].connected()) {
            handle_client_message(i);
        } else if (client_connected[i]) {
            // クライアント切断処理
            handle_client_disconnect(i);
        }
    }
    
    // Keep-Alive チェック
    check_client_keepalive();
}

void handle_new_client(WiFiClient& new_client) {
    // 空きスロット検索
    int free_slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!client_connected[i]) {
            free_slot = i;
            break;
        }
    }
    
    if (free_slot >= 0) {
        mqtt_clients[free_slot] = new_client;
        client_connected[free_slot] = true;
        client_ids[free_slot] = String("client_") + String(free_slot);
        client_last_seen[free_slot] = millis();
        
        mqtt_status.connected_clients++;
        
        Serial.printf("📡 MQTT新規クライアント接続: slot %d\n", free_slot);
        M5.Speaker.tone(1200, 150);  // MQTT接続音
        
        // Welcome メッセージ送信
        String welcome = "Welcome to IsolationSphere MQTT Broker";
        new_client.print(welcome);
        
    } else {
        Serial.println("❌ MQTT接続拒否: クライアント満杯");
        new_client.stop();
    }
}

void handle_client_message(int client_index) {
    if (mqtt_clients[client_index].available()) {
        String message = mqtt_clients[client_index].readString();
        message.trim();
        
        if (message.length() > 0) {
            mqtt_status.total_messages++;
            client_last_seen[client_index] = millis();
            
            Serial.printf("📩 MQTT受信 [%d]: %s\n", client_index, message.c_str());
            
            // 簡易メッセージ処理（実際のMQTTプロトコルは簡略化）
            process_mqtt_message(client_index, message);
        }
    }
}

void handle_client_disconnect(int client_index) {
    Serial.printf("📡 MQTTクライアント切断: slot %d\n", client_index);
    
    mqtt_clients[client_index].stop();
    client_connected[client_index] = false;
    client_ids[client_index] = "";
    client_last_seen[client_index] = 0;
    
    mqtt_status.connected_clients--;
    M5.Speaker.tone(800, 100);  // MQTT切断音
}

void check_client_keepalive() {
    unsigned long current_time = millis();
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_connected[i]) {
            if (current_time - client_last_seen[i] > MQTT_KEEP_ALIVE * 1000) {
                Serial.printf("⏰ MQTTクライアント%d: Keep-Alive タイムアウト\n", i);
                handle_client_disconnect(i);
            }
        }
    }
}

void process_mqtt_message(int client_index, const String& message) {
    // 簡易Topic/Payload解析
    int separator = message.indexOf(':');
    if (separator > 0) {
        String topic = message.substring(0, separator);
        String payload = message.substring(separator + 1);
        
        // Topic更新
        update_topic(topic, payload);
        
        mqtt_status.last_topic = topic;
        mqtt_status.last_payload = payload;
        
        Serial.printf("📝 Topic更新: %s = %s\n", topic.c_str(), payload.c_str());
        
        // 他のクライアントに転送（Publish処理）
        broadcast_topic_update(client_index, topic, payload);
    }
}

void add_topic(const String& topic, const String& payload, int qos, bool retain) {
    if (topic_count < MAX_TOPICS) {
        topic_store[topic_count].topic = topic;
        topic_store[topic_count].payload = payload;
        topic_store[topic_count].qos = qos;
        topic_store[topic_count].retain = retain;
        topic_store[topic_count].timestamp = millis();
        topic_count++;
        
        mqtt_status.topics_count = topic_count;
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
    add_topic(topic, payload, 0, false);
}

void broadcast_topic_update(int sender_index, const String& topic, const String& payload) {
    String broadcast_msg = topic + ":" + payload + "\n";
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i != sender_index && client_connected[i] && mqtt_clients[i].connected()) {
            mqtt_clients[i].print(broadcast_msg);
        }
    }
}

void update_system_status() {
    mqtt_status.uptime_sec = millis() / 1000;
    
    // システム状態をTopicに反映
    update_topic("isolation-sphere/system/uptime", String(mqtt_status.uptime_sec));
    update_topic("isolation-sphere/system/clients", String(mqtt_status.connected_clients));
    update_topic("isolation-sphere/system/messages", String(mqtt_status.total_messages));
}

void handle_button_input() {
    if (M5.BtnA.wasPressed()) {
        current_mode = (DebugMode)((current_mode + 1) % DEBUG_MODE_COUNT);
        mode_changed = true;
        mode_change_time = millis();
        
        Serial.printf("デバッグモード変更: %s\n", get_mode_name(current_mode));
        M5.Speaker.tone(800, 50);
        
        // モード変更をMQTTで通知
        String mode_name = String(get_mode_name(current_mode));
        mode_name.toLowerCase();
        update_topic("isolation-sphere/joystick/mode", mode_name);
    }
    
    if (M5.BtnB.wasPressed()) {
        print_detailed_mqtt_info();
        M5.Speaker.tone(1200, 100);
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
        case DEBUG_MQTT:
            display_mqtt_info();
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
    
    M5.Display.printf("Up: %lus\n", mqtt_status.uptime_sec);
    M5.Display.printf("Mem:%uKB\n", ESP.getFreeHeap() / 1024);
    
    uint16_t mqtt_color = mqtt_status.broker_active ? GREEN : RED;
    M5.Display.setTextColor(mqtt_color);
    M5.Display.printf("MQTT:%s\n", mqtt_status.broker_active ? "ON" : "OFF");
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

void display_mqtt_info() {
    M5.Display.setTextColor(MAGENTA);
    M5.Display.println("MQTT");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("----");
    
    M5.Display.printf("P:1883\n");
    M5.Display.printf("C:%d/%d\n", mqtt_status.connected_clients, MAX_CLIENTS);
    M5.Display.printf("M:%d\n", mqtt_status.total_messages);
    
    uint16_t status_color = (mqtt_status.connected_clients > 0) ? GREEN : YELLOW;
    M5.Display.setTextColor(status_color);
    M5.Display.println(mqtt_status.connected_clients > 0 ? "Active" : "Wait");
    M5.Display.setTextColor(WHITE);
}

void display_topics_info() {
    M5.Display.setTextColor(ORANGE);
    M5.Display.println("Topics");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("------");
    
    M5.Display.printf("T:%d/%d\n", mqtt_status.topics_count, MAX_TOPICS);
    
    if (mqtt_status.last_topic.length() > 0) {
        M5.Display.println("Last:");
        String short_topic = mqtt_status.last_topic;
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
        case DEBUG_MQTT: return "MQTT";
        case DEBUG_TOPICS: return "TOPICS";
        default: return "UNKNOWN";
    }
}

void print_detailed_mqtt_info() {
    Serial.println("\n=== MQTT詳細情報 ===");
    Serial.printf("ブローカー状態: %s\n", mqtt_status.broker_active ? "アクティブ" : "停止");
    Serial.printf("接続クライアント: %d/%d\n", mqtt_status.connected_clients, MAX_CLIENTS);
    Serial.printf("総メッセージ数: %d\n", mqtt_status.total_messages);
    Serial.printf("Topic数: %d/%d\n", mqtt_status.topics_count, MAX_TOPICS);
    
    Serial.println("\n--- 登録Topic一覧 ---");
    for (int i = 0; i < topic_count; i++) {
        Serial.printf("[%d] %s = %s\n", i, 
                      topic_store[i].topic.c_str(), 
                      topic_store[i].payload.c_str());
    }
    
    Serial.println("\n--- クライアント状態 ---");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_connected[i]) {
            Serial.printf("Client %d: %s (最終通信: %lu秒前)\n", 
                          i, client_ids[i].c_str(), 
                          (millis() - client_last_seen[i]) / 1000);
        }
    }
    Serial.println("========================\n");
}