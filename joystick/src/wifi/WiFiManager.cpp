#include "wifi/WiFiManager.h"
#include <Arduino.h>

WiFiManager::WiFiManager() {
  // コンストラクタは最小限に
}

WiFiManager::~WiFiManager() {
  shutdown();
}

bool WiFiManager::initialize(const ConfigManager::Config &config) {
  Serial.println("[WiFi] Initializing WiFi AP...");
  
  const auto &wifiConfig = config.wifi;
  
  // WiFiが無効の場合は何もしない
  if (!wifiConfig.enabled) {
    Serial.println("[WiFi] WiFi disabled in config");
    return true;
  }

  // STAモードを無効化（APモードのみ）
  WiFi.mode(WIFI_AP);
  delay(100);

  // AP設定
  String ssid = wifiConfig.ap.ssid.empty() ? "isolation-joystick" : wifiConfig.ap.ssid.c_str();
  
  // IP設定を先に行う
  IPAddress local_ip, gateway, subnet;
  local_ip.fromString(wifiConfig.ap.localIp.empty() ? "192.168.4.1" : wifiConfig.ap.localIp.c_str());
  gateway.fromString(wifiConfig.ap.gateway.empty() ? "192.168.4.1" : wifiConfig.ap.gateway.c_str());
  subnet.fromString(wifiConfig.ap.subnet.empty() ? "255.255.255.0" : wifiConfig.ap.subnet.c_str());
  
  Serial.printf("[WiFi] Configuring AP IP: %s, Gateway: %s, Subnet: %s\n", 
               local_ip.toString().c_str(), gateway.toString().c_str(), subnet.toString().c_str());
  
  if (!WiFi.softAPConfig(local_ip, gateway, subnet)) {
    Serial.println("[WiFi] Failed to configure AP IP settings");
    return false;
  }
  
  Serial.printf("[WiFi] Starting AP: %s\n", ssid.c_str());
  
  // パスワードが空の場合はオープンネットワーク、そうでなければセキュアネットワーク
  bool success;
  if (wifiConfig.ap.password.empty()) {
    Serial.println("[WiFi] Starting as open network (no password)");
    success = WiFi.softAP(ssid.c_str(), nullptr, 6, false, 8);
  } else {
    Serial.printf("[WiFi] Starting as secure network with password\n");
    success = WiFi.softAP(ssid.c_str(), wifiConfig.ap.password.c_str(), 6, false, 8);
  }
  
  if (success) {
    apActive_ = true;
    Serial.printf("[WiFi] AP started successfully. IP: %s\n", WiFi.softAPIP().toString().c_str());
    return true;
  } else {
    Serial.println("[WiFi] Failed to start AP");
    return false;
  }
}

void WiFiManager::loop() {
  // 最小限のループ処理
  if (!apActive_) {
    return;
  }
  
  // 定期的なログ出力（30秒毎）
  static uint32_t lastLog = 0;
  uint32_t now = millis();
  if (now - lastLog > 30000) {
    int clients = getClientCount();
    Serial.printf("[WiFi] Status: %d clients connected, IP: %s\n", 
                 clients, getLocalIP().toString().c_str());
    lastLog = now;
  }
}

void WiFiManager::shutdown() {
  if (apActive_) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    apActive_ = false;
    Serial.println("[WiFi] WiFi AP shutdown");
  }
}
