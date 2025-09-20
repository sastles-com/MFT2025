/*
 * WiFi アクセスポイント機能
 * isolation-sphere分散MQTT制御システム
 */

#ifndef WIFI_AP_H
#define WIFI_AP_H

#include <WiFi.h>
#include <IPAddress.h>

// WiFi設定構造体
struct WiFiAPConfig {
  const char* ssid;
  const char* password;
  IPAddress local_ip;
  IPAddress gateway;
  IPAddress subnet;
  int channel;
  int max_connections;
  bool hidden;
};

// 関数プロトタイプ
bool wifi_ap_init(const char* ssid, const char* password, IPAddress local_ip, IPAddress gateway, IPAddress subnet);
bool wifi_ap_is_active();
int wifi_ap_get_client_count();
void wifi_ap_monitor();
void wifi_ap_stop();
String wifi_ap_get_status();

#endif // WIFI_AP_H