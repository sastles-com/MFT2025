/**
 * @file wifi_client.h
 * @brief WiFi Client接続管理システム
 * @description IsolationSphere-Directへの自動接続・監視
 */

#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "config_manager.h"

/**
 * @brief WiFi接続状態
 */
enum class WiFiStatus {
  DISCONNECTED,
  CONNECTING,
  CONNECTED,
  CONNECTION_FAILED,
  RECONNECTING
};

/**
 * @brief WiFi Client管理クラス
 */
class WiFiManager {
public:
  WiFiManager();
  ~WiFiManager();
  
  // 初期化・終了処理
  bool begin(const ConfigManager& config);
  void end();
  
  // 接続管理
  bool connect();
  bool disconnect();
  bool update(); // 定期実行での状態監視・自動再接続
  
  // 状態取得
  WiFiStatus getStatus() const { return status_; }
  bool isConnected() const { return status_ == WiFiStatus::CONNECTED; }
  String getLocalIP() const;
  int getSignalStrength() const;
  unsigned long getUptime() const;
  
  // 統計情報
  struct ConnectionStats {
    unsigned long connect_count;
    unsigned long reconnect_count;
    unsigned long total_uptime;
    unsigned long last_connect_time;
    unsigned long last_disconnect_time;
  };
  
  const ConnectionStats& getStats() const { return stats_; }
  void printConnectionInfo() const;

private:
  WiFiStatus status_;
  ConfigManager config_;
  
  // 接続管理用
  unsigned long last_connection_attempt_;
  unsigned long connection_start_time_;
  unsigned long last_status_check_;
  
  static const unsigned long CONNECTION_TIMEOUT = 30000;    // 30秒（ESP32間通信用）
  static const unsigned long RECONNECTION_INTERVAL = 3000; // 3秒（高頻度再試行）
  static const unsigned long STATUS_CHECK_INTERVAL = 500;  // 0.5秒（リアルタイム監視）
  
  // 統計情報
  ConnectionStats stats_;
  
  // 内部メソッド
  bool attemptConnection();
  void handleConnectionTimeout();
  void updateConnectionStats();
  bool setupStaticIP();
  void printDebugInfo() const;
};