#include "wifi/WiFiManager.h"

WiFiManager::WiFiManager() : connected(false) {
}

WiFiManager::~WiFiManager() {
  end();
}

bool WiFiManager::begin() {
  // TDD原則: テスト通過のための最小限実装
  return false;
}

void WiFiManager::end() {
  if (connected) {
    WiFi.disconnect();
    connected = false;
  }
}

bool WiFiManager::isConnected() {
  return connected && WiFi.isConnected();
}

String WiFiManager::getLocalIP() {
  if (isConnected()) {
    return WiFi.localIP().toString();
  }
  return "0.0.0.0";
}

// JsonObject版の初期化メソッド
bool WiFiManager::initialize(const JsonObject& config) {
  // TDD原則: テスト通過のための最小限実装
  // 実際の初期化処理はテスト後に実装
  return false;
}

// ConfigManager::Config版の初期化メソッド（CoreTasks.cpp用）
bool WiFiManager::initialize(const ConfigManager::Config& config) {
  // TDD原則: テスト通過のための最小限実装
  // ConfigManager::Configからの設定取得は後で実装
  return false;
}

void WiFiManager::loop() {
  // TDD原則: テスト通過のための最小限実装
  // 実際のループ処理はテスト後に実装
}