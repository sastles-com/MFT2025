/*
 * WiFi アクセスポイント機能実装
 * isolation-sphere分散MQTT制御システム
 */

#include "wifi_ap.h"
#include <WiFi.h>

// グローバル変数
static bool ap_active = false;
static WiFiAPConfig current_config;

bool wifi_ap_init(const char* ssid, const char* password, IPAddress local_ip, IPAddress gateway, IPAddress subnet) {
  Serial.println("🔧 Initializing WiFi Access Point...");
  
  // WiFi AP設定
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  
  // アクセスポイント開始
  bool result = WiFi.softAP(ssid, password, 1, 0, 8); // channel=1, hidden=false, max_connections=8
  
  if (result) {
    ap_active = true;
    
    // 設定を保存
    current_config.ssid = ssid;
    current_config.password = password;
    current_config.local_ip = local_ip;
    current_config.gateway = gateway;
    current_config.subnet = subnet;
    current_config.channel = 1;
    current_config.max_connections = 8;
    current_config.hidden = false;
    
    Serial.printf("✅ WiFi AP started successfully\n");
    Serial.printf("   SSID: %s\n", ssid);
    Serial.printf("   IP: %s\n", local_ip.toString().c_str());
    Serial.printf("   Gateway: %s\n", gateway.toString().c_str());
    Serial.printf("   Subnet: %s\n", subnet.toString().c_str());
    Serial.printf("   Channel: %d\n", current_config.channel);
    Serial.printf("   Max Connections: %d\n", current_config.max_connections);
    
    return true;
  } else {
    Serial.println("❌ Failed to start WiFi AP");
    ap_active = false;
    return false;
  }
}

bool wifi_ap_is_active() {
  return ap_active && (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA);
}

int wifi_ap_get_client_count() {
  if (!ap_active) return 0;
  return WiFi.softAPgetStationNum();
}

void wifi_ap_monitor() {
  static unsigned long last_check = 0;
  static int last_client_count = 0;
  
  if (millis() - last_check > 5000) { // 5秒間隔でチェック
    if (ap_active) {
      int current_clients = WiFi.softAPgetStationNum();
      
      if (current_clients != last_client_count) {
        Serial.printf("📱 WiFi clients changed: %d → %d\n", last_client_count, current_clients);
        last_client_count = current_clients;
      }
      
      // APステータス確認
      if (!wifi_ap_is_active()) {
        Serial.println("⚠️  WiFi AP disconnected, attempting restart...");
        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(current_config.local_ip, current_config.gateway, current_config.subnet);
        WiFi.softAP(current_config.ssid, current_config.password, current_config.channel, current_config.hidden, current_config.max_connections);
      }
    }
    last_check = millis();
  }
}

void wifi_ap_stop() {
  if (ap_active) {
    WiFi.softAPdisconnect(true);
    ap_active = false;
    Serial.println("🔴 WiFi AP stopped");
  }
}

String wifi_ap_get_status() {
  if (!ap_active) {
    return "INACTIVE";
  }
  
  String status = "ACTIVE";
  status += " | Clients: " + String(WiFi.softAPgetStationNum());
  status += " | IP: " + WiFi.softAPIP().toString();
  status += " | SSID: " + String(current_config.ssid);
  
  return status;
}