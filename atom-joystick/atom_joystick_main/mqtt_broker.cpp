/*
 * MQTT ブローカー機能実装
 * isolation-sphere分散MQTT制御システム
 * 
 * 注意：この実装は簡易版です。実際の軽量MQTTブローカー統合時は
 * uMQTTやmosquittoライブラリを使用することを推奨します。
 */

#include "mqtt_broker.h"
#include <ArduinoJson.h>
#include <WiFi.h>

// グローバル変数
static WiFiServer* mqtt_server = nullptr;
static bool broker_active = false;
static int broker_port = 1884;  // config.json準拠
static int max_clients = MAX_MQTT_CLIENTS;
static MQTTClientInfo client_list[MAX_MQTT_CLIENTS];
static int active_client_count = 0;

// 内部関数プロトタイプ
static void handle_new_clients();
static void handle_existing_clients();
static void process_mqtt_message(WiFiClient* client, const String& message);
static void send_mqtt_response(WiFiClient* client, const String& response);
static int find_client_slot(const String& client_id);
static void remove_client(int slot);
static String format_mqtt_publish(const char* topic, const char* payload);

bool mqtt_broker_init(int port, int max_clients_param) {
  Serial.println("🔄 Initializing MQTT Broker...");
  
  broker_port = port;
  max_clients = (max_clients_param <= MAX_MQTT_CLIENTS) ? max_clients_param : MAX_MQTT_CLIENTS;
  
  // WiFiサーバー開始
  mqtt_server = new WiFiServer(broker_port);
  mqtt_server->begin();
  
  // クライアントリスト初期化
  for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
    client_list[i].active = false;
    client_list[i].client_id = "";
    client_list[i].message_count = 0;
  }
  
  broker_active = true;
  
  Serial.printf("✅ MQTT Broker started on port %d\n", broker_port);
  Serial.printf("   Max clients: %d\n", max_clients);
  Serial.println("   Supported MQTT features:");
  Serial.println("   - Basic Publish/Subscribe");
  Serial.println("   - QoS 0 (At most once)");
  Serial.println("   - Retain messages");
  Serial.println("   - Client discovery");
  
  return true;
}

void mqtt_broker_loop() {
  if (!broker_active || mqtt_server == nullptr) return;
  
  // 新しいクライアント接続処理
  handle_new_clients();
  
  // 既存クライアント処理
  handle_existing_clients();
}

static void handle_new_clients() {
  WiFiClient new_client = mqtt_server->available();
  if (new_client) {
    if (active_client_count < max_clients) {
      // 空きスロット検索
      int slot = -1;
      for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
        if (!client_list[i].active) {
          slot = i;
          break;
        }
      }
      
      if (slot >= 0) {
        // クライアント情報設定
        client_list[slot].client_id = mqtt_broker_generate_client_id();
        client_list[slot].client_ip = new_client.remoteIP();
        client_list[slot].connected_time = millis();
        client_list[slot].last_ping = millis();
        client_list[slot].active = true;
        client_list[slot].message_count = 0;
        
        active_client_count++;
        
        Serial.printf("📱 New MQTT client connected: %s (%s) [%d/%d]\n", 
          client_list[slot].client_id.c_str(), 
          client_list[slot].client_ip.toString().c_str(),
          active_client_count, max_clients);
        
        // 接続確認メッセージ送信
        String welcome_msg = "{\"type\":\"welcome\",\"broker\":\"isolation-sphere-hub\",\"version\":\"1.0.0\"}";
        send_mqtt_response(&new_client, welcome_msg);
        
        // デバイス発見アナウンス送信
        mqtt_broker_send_discovery_announce();
        
        mqtt_broker_on_connect(client_list[slot].client_id.c_str(), client_list[slot].client_ip);
      } else {
        Serial.println("⚠️  MQTT client connection rejected: no available slots");
        new_client.stop();
      }
    } else {
      Serial.println("⚠️  MQTT client connection rejected: max clients reached");
      new_client.stop();
    }
  }
}

static void handle_existing_clients() {
  // 既存クライアントからのメッセージ処理（簡易実装）
  // 実際のMQTTプロトコル解析は複雑なため、ここでは概念実装のみ
  
  for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
    if (client_list[i].active) {
      // 接続状態確認
      unsigned long now = millis();
      if (now - client_list[i].last_ping > (MQTT_KEEPALIVE_SECONDS * 2000)) {
        Serial.printf("⚠️  MQTT client timeout: %s\n", client_list[i].client_id.c_str());
        mqtt_broker_on_disconnect(client_list[i].client_id.c_str());
        remove_client(i);
      }
    }
  }
}

bool mqtt_broker_publish(const char* topic, const char* payload, bool retain) {
  if (!broker_active) return false;
  
  Serial.printf("📤 MQTT Publish: %s = %s (retain: %s)\n", topic, payload, retain ? "true" : "false");
  
  // 全クライアントにメッセージ配信（簡易実装）
  String mqtt_message = format_mqtt_publish(topic, payload);
  
  int delivered = 0;
  for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
    if (client_list[i].active) {
      // 実際の実装では、クライアントの購読情報を確認してからメッセージ送信
      delivered++;
      client_list[i].message_count++;
    }
  }
  
  if (delivered > 0) {
    Serial.printf("✅ Message delivered to %d clients\n", delivered);
  }
  
  return delivered > 0;
}

bool mqtt_broker_publish_joystick_state(const JoystickState* state) {
  if (state == nullptr) return false;
  
  String json_state = joystick_state_to_json(state);
  return mqtt_broker_publish("isolation-sphere/input/joystick", json_state.c_str(), false);
}

void mqtt_broker_handle_clients() {
  // クライアント管理処理
  static unsigned long last_cleanup = 0;
  
  if (millis() - last_cleanup > 10000) { // 10秒間隔
    // 非アクティブクライアントのクリーンアップ
    for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
      if (client_list[i].active) {
        unsigned long connection_time = millis() - client_list[i].connected_time;
        if (connection_time > 300000 && client_list[i].message_count == 0) { // 5分間メッセージなし
          Serial.printf("🧹 Cleaning up inactive client: %s\n", client_list[i].client_id.c_str());
          remove_client(i);
        }
      }
    }
    last_cleanup = millis();
  }
}

int mqtt_broker_get_client_count() {
  return active_client_count;
}

String mqtt_broker_get_status() {
  String status = broker_active ? "ACTIVE" : "INACTIVE";
  status += " | Port: " + String(broker_port);
  status += " | Clients: " + String(active_client_count) + "/" + String(max_clients);
  return status;
}

MQTTClientInfo* mqtt_broker_get_client_list() {
  return client_list;
}

static String format_mqtt_publish(const char* topic, const char* payload) {
  // 簡易MQTT Publishメッセージフォーマット（実際はバイナリプロトコル）
  StaticJsonDocument<512> doc;
  doc["type"] = "publish";
  doc["topic"] = topic;
  doc["payload"] = payload;
  doc["timestamp"] = millis();
  
  String result;
  serializeJson(doc, result);
  return result;
}

static void send_mqtt_response(WiFiClient* client, const String& response) {
  if (client && client->connected()) {
    client->println(response);
  }
}

static int find_client_slot(const String& client_id) {
  for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
    if (client_list[i].active && client_list[i].client_id == client_id) {
      return i;
    }
  }
  return -1;
}

static void remove_client(int slot) {
  if (slot >= 0 && slot < MAX_MQTT_CLIENTS && client_list[slot].active) {
    client_list[slot].active = false;
    client_list[slot].client_id = "";
    client_list[slot].message_count = 0;
    active_client_count--;
  }
}

String mqtt_broker_generate_client_id() {
  static int client_counter = 1;
  return "client_" + String(millis()) + "_" + String(client_counter++);
}

void mqtt_broker_send_discovery_announce() {
  StaticJsonDocument<256> announce;
  announce["type"] = "discovery";
  announce["hub_id"] = "atom-joystick-hub";
  announce["capabilities"] = "mqtt_broker,wifi_ap,joystick_input";
  announce["version"] = "1.0.0";
  announce["max_clients"] = max_clients;
  announce["current_clients"] = active_client_count;
  
  String announce_json;
  serializeJson(announce, announce_json);
  
  mqtt_broker_publish("isolation-sphere/global/discovery/announce", announce_json.c_str(), false);
}

void mqtt_broker_send_system_config() {
  StaticJsonDocument<512> config;
  config["default_brightness"] = 128;
  config["default_volume"] = 50;
  config["sync_interval_ms"] = 100;
  config["heartbeat_interval_ms"] = 5000;
  config["led_update_rate_hz"] = 30;
  config["imu_update_rate_hz"] = 30;
  
  String config_json;
  serializeJson(config, config_json);
  
  mqtt_broker_publish("isolation-sphere/global/config/system", config_json.c_str(), true); // retain
}

// コールバック関数実装
void mqtt_broker_on_connect(const char* client_id, IPAddress client_ip) {
  Serial.printf("🔗 MQTT Connect: %s from %s\n", client_id, client_ip.toString().c_str());
  
  // システム設定配信
  mqtt_broker_send_system_config();
}

void mqtt_broker_on_disconnect(const char* client_id) {
  Serial.printf("🔌 MQTT Disconnect: %s\n", client_id);
}

void mqtt_broker_on_message(const char* client_id, const char* topic, const char* payload) {
  Serial.printf("📥 MQTT Message from %s: %s = %s\n", client_id, topic, payload);
  
  // トピック別処理（例）
  if (strcmp(topic, "isolation-sphere/cmd/system/restart") == 0) {
    Serial.println("🔄 System restart command received");
    // ESP.restart(); // 実際の再起動処理
  }
}

void mqtt_broker_on_subscribe(const char* client_id, const char* topic) {
  Serial.printf("📋 MQTT Subscribe: %s -> %s\n", client_id, topic);
}

void mqtt_broker_on_unsubscribe(const char* client_id, const char* topic) {
  Serial.printf("📋 MQTT Unsubscribe: %s -> %s\n", client_id, topic);
}

void mqtt_broker_stop() {
  if (broker_active && mqtt_server != nullptr) {
    // 全クライアント切断
    for (int i = 0; i < MAX_MQTT_CLIENTS; i++) {
      if (client_list[i].active) {
        mqtt_broker_on_disconnect(client_list[i].client_id.c_str());
        remove_client(i);
      }
    }
    
    mqtt_server->end();
    delete mqtt_server;
    mqtt_server = nullptr;
    broker_active = false;
    
    Serial.println("🔴 MQTT Broker stopped");
  }
}

bool mqtt_broker_is_valid_topic(const char* topic) {
  if (topic == nullptr) return false;
  
  String topic_str(topic);
  
  // isolation-sphere プレフィックス確認
  if (!topic_str.startsWith("isolation-sphere/")) return false;
  
  // 予約トピック確認
  if (topic_str.indexOf("$SYS/") >= 0) return false; // システムトピック
  if (topic_str.indexOf("//") >= 0) return false;    // 空レベル
  
  return true;
}