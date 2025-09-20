/*
 * MQTT ブローカー機能
 * isolation-sphere分散MQTT制御システム
 */

#ifndef MQTT_BROKER_H
#define MQTT_BROKER_H

#include <WiFi.h>
#include <PubSubClient.h>
#include "joystick_input.h"

// MQTT設定
#define MAX_MQTT_MESSAGE_SIZE 512
#define MAX_MQTT_CLIENTS 8
#define MQTT_KEEPALIVE_SECONDS 60

// MQTTクライアント情報構造体
struct MQTTClientInfo {
  String client_id;
  IPAddress client_ip;
  unsigned long connected_time;
  unsigned long last_ping;
  bool active;
  int message_count;
};

// 関数プロトタイプ
bool mqtt_broker_init(int port, int max_clients);
void mqtt_broker_loop();
void mqtt_broker_handle_clients();
int mqtt_broker_get_client_count();
bool mqtt_broker_publish(const char* topic, const char* payload, bool retain);
bool mqtt_broker_publish_joystick_state(const JoystickState* state);
void mqtt_broker_stop();
String mqtt_broker_get_status();
MQTTClientInfo* mqtt_broker_get_client_list();

// コールバック関数
void mqtt_broker_on_connect(const char* client_id, IPAddress client_ip);
void mqtt_broker_on_disconnect(const char* client_id);
void mqtt_broker_on_message(const char* client_id, const char* topic, const char* payload);
void mqtt_broker_on_subscribe(const char* client_id, const char* topic);
void mqtt_broker_on_unsubscribe(const char* client_id, const char* topic);

// ユーティリティ関数
bool mqtt_broker_is_valid_topic(const char* topic);
String mqtt_broker_generate_client_id();
void mqtt_broker_send_discovery_announce();
void mqtt_broker_send_system_config();

#endif // MQTT_BROKER_H