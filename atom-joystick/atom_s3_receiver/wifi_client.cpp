/**
 * @file wifi_client.cpp
 * @brief WiFi Client接続管理実装
 */

#include "wifi_client.h"
#include <esp_wifi.h>

/**
 * @brief コンストラクタ
 */
WiFiManager::WiFiManager() 
  : status_(WiFiStatus::DISCONNECTED)
  , last_connection_attempt_(0)
  , connection_start_time_(0)
  , last_status_check_(0)
  , stats_({0, 0, 0, 0, 0}) {
}

/**
 * @brief デストラクタ
 */
WiFiManager::~WiFiManager() {
  end();
}

/**
 * @brief WiFi Client初期化
 */
bool WiFiManager::begin(const ConfigManager& config) {
  config_ = config;
  
  Serial.println();
  Serial.println("████████████████████████████████████████████████████████");
  Serial.println("██          🚀 WiFiClient初期化開始 🚀              ██");
  Serial.println("████████████████████████████████████████████████████████");
  
  // WiFi完全リセット・ESP32固有設定・NVS強制クリア
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  
  // ESP32 NVS強制クリア（古い認証情報完全削除）
  esp_wifi_restore();
  delay(500);
  Serial.println("██ ⚡ ESP32 NVS WiFi設定強制クリア完了              ██");
  
  // WiFi設定
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false); // 手動管理
  // WiFi.setAutoConnect(false); // メソッドが存在しない
  
  // ESP32固有の設定追加
  WiFi.setSleep(false);  // WiFiスリープ無効
  WiFi.setTxPower(WIFI_POWER_19_5dBm);  // 最大送信電力
  
  Serial.println("██ ✅ ESP32固有設定: WiFiスリープ無効・最大送信電力  ██");
  
  // WiFiスキャン実行してSSID検索
  Serial.println("██                                                    ██");
  Serial.println("██              🔍 WiFiスキャン開始 🔍                 ██");
  Serial.println("████████████████████████████████████████████████████████");
  
  int networks = WiFi.scanNetworks();
  if (networks == 0) {
    Serial.println("██ ❌❌❌ WiFiネットワーク未検出 ❌❌❌             ██");
  } else {
    Serial.printf("██ 🔍 検出ネットワーク数: %d                         ██\n", networks);
    Serial.println("██                                                    ██");
  
    bool target_found = false;
    for (int i = 0; i < networks; i++) {
      String ssid = WiFi.SSID(i);
      int32_t rssi = WiFi.RSSI(i);
      
      // 長いSSID名の場合は短縮表示
      if (ssid.length() > 20) {
        ssid = ssid.substring(0, 17) + "...";
      }
      
      Serial.printf("██ [%d] %-20s %4d dBm ██\n", i, ssid.c_str(), rssi);
      
      if (WiFi.SSID(i) == config_.getWiFiSSID()) {
        target_found = true;
        Serial.println("██ 🎯🎯 ターゲットSSID発見！🎯🎯                 ██");
      }
    }
    
    Serial.println("██                                                    ██");
    
    if (!target_found) {
      Serial.printf("██ ❌❌ ターゲットSSID未発見: %s ❌❌\n", config_.getWiFiSSID().c_str());
      Serial.println("██     Atom-JoyStickアクセスポイント確認要       ██");
    } else {
      Serial.printf("██ ✅✅ 接続予定SSID: %s ✅✅\n", config_.getWiFiSSID().c_str());
    }
  }
  
  Serial.println("████████████████████████████████████████████████████████");
  Serial.println();
  
  status_ = WiFiStatus::DISCONNECTED;
  
  // 初回接続試行
  return connect();
}

/**
 * @brief 終了処理
 */
void WiFiManager::end() {
  disconnect();
  WiFi.mode(WIFI_OFF);
}

/**
 * @brief WiFi接続開始
 */
bool WiFiManager::connect() {
  if (status_ == WiFiStatus::CONNECTING) {
    return false; // 既に接続試行中
  }
  
  Serial.println("WiFiClient: 接続開始");
  status_ = WiFiStatus::CONNECTING;
  connection_start_time_ = millis();
  last_connection_attempt_ = millis();
  
  return attemptConnection();
}

/**
 * @brief WiFi切断
 */
bool WiFiManager::disconnect() {
  if (status_ == WiFiStatus::CONNECTED) {
    stats_.last_disconnect_time = millis();
    updateConnectionStats();
  }
  
  WiFi.disconnect();
  status_ = WiFiStatus::DISCONNECTED;
  
  Serial.println("WiFiClient: 切断完了");
  return true;
}

/**
 * @brief 状態監視・自動再接続（定期実行）
 */
bool WiFiManager::update() {
  unsigned long now = millis();
  
  // 定期状態チェック
  if (now - last_status_check_ > STATUS_CHECK_INTERVAL) {
    last_status_check_ = now;
    
    switch (status_) {
      case WiFiStatus::CONNECTING:
        // 接続タイムアウト判定
        if (now - connection_start_time_ > CONNECTION_TIMEOUT) {
          handleConnectionTimeout();
        } else if (WiFi.status() == WL_CONNECTED) {
          // 接続成功
          status_ = WiFiStatus::CONNECTED;
          stats_.connect_count++;
          stats_.last_connect_time = now;
          
          Serial.println();
          Serial.println("██ ✅✅✅ WiFi接続成功！✅✅✅");
          Serial.printf("██   📍 ローカルIP: %s\n", WiFi.localIP().toString().c_str());
          Serial.printf("██   📡 信号強度: %d dBm\n", WiFi.RSSI());
          Serial.println("████████████████████████████████████");
          Serial.println();
        }
        break;
        
      case WiFiStatus::CONNECTED:
        // 接続状態確認
        if (WiFi.status() != WL_CONNECTED) {
          Serial.println();
          Serial.println("██ ⚠️ ⚠️ ⚠️  WiFi接続切断検出  ⚠️ ⚠️ ⚠️");
          Serial.println("████████████████████████████████████████");
          Serial.println();
          status_ = WiFiStatus::DISCONNECTED;
          stats_.last_disconnect_time = now;
          updateConnectionStats();
        }
        break;
        
      case WiFiStatus::DISCONNECTED:
      case WiFiStatus::CONNECTION_FAILED:
        // 自動再接続
        if (now - last_connection_attempt_ > RECONNECTION_INTERVAL) {
          Serial.println("██ 🔄🔄 WiFi自動再接続開始 🔄🔄");
          status_ = WiFiStatus::RECONNECTING;
          stats_.reconnect_count++;
          connect();
        }
        break;
        
      case WiFiStatus::RECONNECTING:
        // 再接続中（CONNECTING状態と同様）
        if (now - connection_start_time_ > CONNECTION_TIMEOUT) {
          handleConnectionTimeout();
        } else if (WiFi.status() == WL_CONNECTED) {
          status_ = WiFiStatus::CONNECTED;
          stats_.last_connect_time = now;
          Serial.println("✅ WiFiClient: 再接続成功");
        }
        break;
    }
  }
  
  return isConnected();
}

/**
 * @brief 実際の接続処理
 */
bool WiFiManager::attemptConnection() {
  const WiFiConfig& wifi_config = config_.getWiFiConfig();
  
  // WiFi接続開始 - ESP32互換性対応
  Serial.println("████████████████████████████████████████████████████████");
  Serial.println("██               🔧 WiFi接続開始 🔧                   ██");
  Serial.println("████████████████████████████████████████████████████████");
  Serial.printf("██ SSID: %s                      ██\n", wifi_config.ssid.c_str());
  Serial.println("██ パスワード: オープン接続 (ESP32互換・強制)       ██");
  Serial.printf("██ MAC Address: %s                ██\n", WiFi.macAddress().c_str());
  
  // 静的IP設定チェック
  if (!wifi_config.static_ip.isEmpty()) {
    Serial.printf("██ 静的IP設定: %s                    ██\n", wifi_config.static_ip.c_str());
  } else {
    Serial.println("██ DHCP使用（静的IP未設定）                        ██");
  }
  Serial.println("████████████████████████████████████████████████████████");
  
  // 静的IP設定（WiFi.begin()の前に実行）
  if (!wifi_config.static_ip.isEmpty()) {
    Serial.printf("🔧 WiFiClient: 静的IP設定適用: %s\n", wifi_config.static_ip.c_str());
    if (!setupStaticIP()) {
      Serial.println("❌ WiFiClient: 静的IP設定失敗");
      status_ = WiFiStatus::CONNECTION_FAILED;
      return false;
    }
  }
  
  // ESP32-ESP32互換性のためオープン接続を完全強制（設定ファイル無視）
  WiFi.begin("IsolationSphere-Direct", "");
  
  Serial.println("██ WiFi.begin() オープン接続実行完了                ██");
  if (!wifi_config.static_ip.isEmpty()) {
    Serial.printf("██ 静的IP(%s)で接続待機中...          ██\n", wifi_config.static_ip.c_str());
  } else {
    Serial.println("██ DHCP動的IPで接続待機中...                       ██");
  }
  return true;
}

/**
 * @brief 接続タイムアウト処理
 */
void WiFiManager::handleConnectionTimeout() {
  wl_status_t wifi_status = WiFi.status();
  Serial.println("❌ WiFiClient: 接続タイムアウト");
  Serial.printf("   WiFiステータス: %d\n", wifi_status);
  
  switch(wifi_status) {
    case WL_NO_SSID_AVAIL:
      Serial.println("   → SSID が見つかりません");
      break;
    case WL_CONNECT_FAILED:
      Serial.println("   → 接続に失敗しました（パスワード間違い？）");
      break;
    case WL_DISCONNECTED:
      Serial.println("   → 切断されました");
      break;
    default:
      Serial.printf("   → 不明なステータス: %d\n", wifi_status);
      break;
  }
  
  status_ = WiFiStatus::CONNECTION_FAILED;
  WiFi.disconnect();
}

/**
 * @brief 接続統計更新
 */
void WiFiManager::updateConnectionStats() {
  if (stats_.last_connect_time > 0 && stats_.last_disconnect_time > stats_.last_connect_time) {
    unsigned long session_uptime = stats_.last_disconnect_time - stats_.last_connect_time;
    stats_.total_uptime += session_uptime;
  }
}

/**
 * @brief 静的IP設定
 */
bool WiFiManager::setupStaticIP() {
  const WiFiConfig& wifi_config = config_.getWiFiConfig();
  
  IPAddress local_ip;
  if (!local_ip.fromString(wifi_config.static_ip)) {
    Serial.printf("❌ WiFiClient: 無効な静的IP: %s\n", wifi_config.static_ip.c_str());
    return false;
  }
  
  IPAddress gateway(192, 168, 100, 1);  // Joystickゲートウェイ
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns(192, 168, 100, 1);      // DNS設定
  
  if (!WiFi.config(local_ip, gateway, subnet, dns)) {
    Serial.println("❌ WiFiClient: 静的IP設定失敗");
    return false;
  }
  
  Serial.printf("✅ WiFiClient: 静的IP設定成功 -> %s\n", local_ip.toString().c_str());
  return true;
}

/**
 * @brief ローカルIP取得
 */
String WiFiManager::getLocalIP() const {
  if (isConnected()) {
    return WiFi.localIP().toString();
  }
  return "未接続";
}

/**
 * @brief 信号強度取得
 */
int WiFiManager::getSignalStrength() const {
  if (isConnected()) {
    return WiFi.RSSI();
  }
  return -999; // 無効値
}

/**
 * @brief 稼働時間取得
 */
unsigned long WiFiManager::getUptime() const {
  if (isConnected() && stats_.last_connect_time > 0) {
    return millis() - stats_.last_connect_time;
  }
  return 0;
}

/**
 * @brief 接続情報出力
 */
void WiFiManager::printConnectionInfo() const {
  Serial.println("\n========== WiFi接続情報 ==========");
  
  const char* status_names[] = {
    "切断", "接続中", "接続済み", "接続失敗", "再接続中"
  };
  Serial.printf("状態: %s\n", status_names[(int)status_]);
  
  if (isConnected()) {
    Serial.printf("ローカルIP: %s\n", getLocalIP().c_str());
    Serial.printf("信号強度: %d dBm\n", getSignalStrength());
    Serial.printf("稼働時間: %lu秒\n", getUptime() / 1000);
  }
  
  Serial.printf("接続回数: %lu\n", stats_.connect_count);
  Serial.printf("再接続回数: %lu\n", stats_.reconnect_count);
  Serial.printf("総稼働時間: %lu秒\n", stats_.total_uptime / 1000);
  
  Serial.println("==================================\n");
}