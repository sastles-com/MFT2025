/**
 * @file 04_wifi_access_point.ino
 * @brief M5Stack Atom-JoyStick WiFiアクセスポイント実装
 * @description 分散MQTTシステム用WiFiルーター機能
 * 
 * Phase 2: WiFi通信環境構築
 * - WiFiアクセスポイント作成
 * - IsolationSphere-Directネットワーク提供
 * - DHCPサーバー機能
 * - 接続デバイス監視
 * 
 * @target M5Stack Atom-JoyStick (ESP32-S3)
 * @network IsolationSphere-Direct (192.168.100.x)
 */

#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <ArduinoJson.h>

// WiFiアクセスポイント設定
const char* AP_SSID = "IsolationSphere-Direct";
const char* AP_PASSWORD = "isolation-sphere-2025";
const IPAddress AP_IP(192, 168, 100, 1);
const IPAddress AP_GATEWAY(192, 168, 100, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

// システム状態管理
struct WiFiSystemStatus {
    bool ap_active;
    int connected_devices;
    unsigned long uptime_sec;
    IPAddress ap_ip;
    String ap_ssid;
    int rssi_strength;
} wifi_status;

// デバッグ表示モード（Phase 1から継承）
enum DebugMode {
    DEBUG_SYSTEM,    // システム情報
    DEBUG_WIFI,      // WiFi情報（新規）
    DEBUG_DEVICES,   // 接続デバイス（新規）
    DEBUG_NETWORK,   // ネットワーク統計（新規）
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
    Serial.println("M5Stack Atom-JoyStick WiFi AP");
    Serial.println("=================================");
    Serial.println("Phase 2: WiFi通信環境構築");
    
    // LCD初期化
    M5.Display.clear(BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(WHITE);
    M5.Display.setRotation(0);
    
    // WiFiアクセスポイント初期化
    init_wifi_access_point();
    
    Serial.println("✅ WiFiアクセスポイント初期化完了");
    Serial.println("操作: ボタンAでモード切り替え、ボタンBで詳細表示");
    
    last_update = millis();
}

void loop() {
    M5.update();
    
    // ボタン入力処理
    handle_button_input();
    
    // 500ms間隔で表示・状態更新
    if (millis() - last_update >= 500) {
        update_wifi_status();
        display_debug_info();
        last_update = millis();
    }
    
    delay(50);
}

void init_wifi_access_point() {
    Serial.println("WiFiアクセスポイント設定開始...");
    
    // WiFi モード設定（アクセスポイントモード）
    WiFi.mode(WIFI_AP);
    delay(100);
    
    // IPアドレス設定
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
    
    // アクセスポイント開始
    bool ap_result = WiFi.softAP(AP_SSID, AP_PASSWORD, 1, 0, 8);  // チャンネル1、非表示false、最大8接続
    
    if (ap_result) {
        Serial.printf("✅ WiFi AP作成成功: %s\n", AP_SSID);
        Serial.printf("✅ IP アドレス: %s\n", WiFi.softAPIP().toString().c_str());
        Serial.printf("✅ ネットワーク: %s\n", AP_SUBNET.toString().c_str());
        Serial.printf("✅ 最大接続数: 8デバイス\n");
        
        // 初期状態更新
        wifi_status.ap_active = true;
        wifi_status.ap_ip = WiFi.softAPIP();
        wifi_status.ap_ssid = String(AP_SSID);
        
    } else {
        Serial.println("❌ WiFi AP作成失敗");
        wifi_status.ap_active = false;
    }
    
    delay(1000);
}

void update_wifi_status() {
    wifi_status.uptime_sec = millis() / 1000;
    wifi_status.connected_devices = WiFi.softAPgetStationNum();
    wifi_status.rssi_strength = WiFi.RSSI();  // AP mode では使用しないが将来用
    
    // 新規接続・切断検出
    static int last_device_count = 0;
    if (wifi_status.connected_devices != last_device_count) {
        if (wifi_status.connected_devices > last_device_count) {
            Serial.printf("📱 新規デバイス接続 (合計: %d台)\n", wifi_status.connected_devices);
            M5.Speaker.tone(1000, 100);  // 接続音
        } else {
            Serial.printf("📱 デバイス切断 (合計: %d台)\n", wifi_status.connected_devices);
            M5.Speaker.tone(800, 100);   // 切断音
        }
        last_device_count = wifi_status.connected_devices;
    }
}

void handle_button_input() {
    // ボタンA: モード切り替え
    if (M5.BtnA.wasPressed()) {
        current_mode = (DebugMode)((current_mode + 1) % DEBUG_MODE_COUNT);
        mode_changed = true;
        mode_change_time = millis();
        
        Serial.printf("デバッグモード変更: %s\n", get_mode_name(current_mode));
        M5.Speaker.tone(800, 50);
    }
    
    // ボタンB: 詳細情報表示
    if (M5.BtnB.wasPressed()) {
        print_detailed_wifi_info();
        M5.Speaker.tone(1200, 100);
    }
}

void display_debug_info() {
    M5.Display.clear(BLACK);
    M5.Display.setCursor(2, 2);
    
    // モード変更通知表示（2秒間）
    if (mode_changed && millis() - mode_change_time < 2000) {
        M5.Display.setTextColor(YELLOW);
        M5.Display.printf("Mode: %s\n", get_mode_name(current_mode));
        M5.Display.setTextColor(WHITE);
        M5.Display.println("----------");
    }
    
    switch (current_mode) {
        case DEBUG_SYSTEM:
            display_system_info();
            break;
        case DEBUG_WIFI:
            display_wifi_info();
            break;
        case DEBUG_DEVICES:
            display_devices_info();
            break;
        case DEBUG_NETWORK:
            display_network_stats();
            break;
        default:
            break;
    }
    
    // モード変更通知をクリア
    if (mode_changed && millis() - mode_change_time >= 2000) {
        mode_changed = false;
    }
}

void display_system_info() {
    M5.Display.setTextColor(GREEN);
    M5.Display.println("System Info");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("----------");
    
    M5.Display.printf("Uptime: %lus\n", wifi_status.uptime_sec);
    M5.Display.printf("Heap: %uKB\n", ESP.getFreeHeap() / 1024);
    M5.Display.printf("CPU: %dMHz\n", ESP.getCpuFreqMHz());
    
    // WiFi統合ステータス
    uint16_t wifi_color = wifi_status.ap_active ? GREEN : RED;
    M5.Display.setTextColor(wifi_color);
    M5.Display.printf("WiFi: %s\n", wifi_status.ap_active ? "AP ON" : "AP OFF");
    M5.Display.setTextColor(WHITE);
}

void display_wifi_info() {
    M5.Display.setTextColor(CYAN);
    M5.Display.println("WiFi AP");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("----------");
    
    if (wifi_status.ap_active) {
        M5.Display.printf("SSID:\n%s\n", wifi_status.ap_ssid.c_str());
        M5.Display.printf("IP: %s\n", wifi_status.ap_ip.toString().c_str());
        M5.Display.setTextColor(GREEN);
        M5.Display.println("Status: ON");
    } else {
        M5.Display.setTextColor(RED);
        M5.Display.println("Status: OFF");
        M5.Display.println("AP Failed");
    }
    M5.Display.setTextColor(WHITE);
}

void display_devices_info() {
    M5.Display.setTextColor(MAGENTA);
    M5.Display.println("Devices");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("----------");
    
    M5.Display.printf("Connected: %d/8\n", wifi_status.connected_devices);
    
    // 接続状況の視覚表示
    uint16_t device_color = (wifi_status.connected_devices > 0) ? GREEN : YELLOW;
    M5.Display.setTextColor(device_color);
    if (wifi_status.connected_devices == 0) {
        M5.Display.println("待機中...");
    } else {
        M5.Display.printf("Active: %d台\n", wifi_status.connected_devices);
    }
    M5.Display.setTextColor(WHITE);
    
    // 接続率表示
    int connection_percent = (wifi_status.connected_devices * 100) / 8;
    M5.Display.printf("Rate: %d%%\n", connection_percent);
}

void display_network_stats() {
    M5.Display.setTextColor(ORANGE);
    M5.Display.println("Network");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("----------");
    
    M5.Display.println("DHCP: Active");
    M5.Display.println("Range:");
    M5.Display.println("100.10-100.50");
    M5.Display.setTextColor(YELLOW);
    M5.Display.println("統計準備中");
    M5.Display.setTextColor(WHITE);
}

const char* get_mode_name(DebugMode mode) {
    switch (mode) {
        case DEBUG_SYSTEM: return "SYSTEM";
        case DEBUG_WIFI: return "WIFI";
        case DEBUG_DEVICES: return "DEVICES";
        case DEBUG_NETWORK: return "NETWORK";
        default: return "UNKNOWN";
    }
}

void print_detailed_wifi_info() {
    Serial.println("\n=== WiFi詳細情報 ===");
    Serial.printf("現在モード: %s\n", get_mode_name(current_mode));
    Serial.printf("アップタイム: %lu秒\n", wifi_status.uptime_sec);
    
    if (wifi_status.ap_active) {
        Serial.printf("AP SSID: %s\n", wifi_status.ap_ssid.c_str());
        Serial.printf("AP IP: %s\n", wifi_status.ap_ip.toString().c_str());
        Serial.printf("接続デバイス数: %d/8\n", wifi_status.connected_devices);
        Serial.printf("MACアドレス: %s\n", WiFi.softAPmacAddress().c_str());
        Serial.printf("チャンネル: %d\n", WiFi.channel());
    } else {
        Serial.println("WiFi AP: 非アクティブ");
    }
    
    Serial.printf("フリーヒープ: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("最大ブロック: %u bytes\n", ESP.getMaxAllocHeap());
    Serial.println("========================\n");
}