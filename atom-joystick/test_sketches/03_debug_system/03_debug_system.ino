/**
 * @file 03_debug_system.ino
 * @brief M5Stack Atom-JoyStickデバッグシステムテスト
 * @description 統合デバッグ環境・情報表示システム確認
 * 
 * Phase 1: デバッグシステム確認
 * - リアルタイム情報表示
 * - システム監視機能
 * - モード切り替えUI基盤
 * - エラーハンドリング
 * 
 * @target M5Stack Atom-JoyStick (ESP32-S3)
 * @board esp32:esp32:esp32s3_family
 */

#include <M5Unified.h>

// デバッグ表示モード
enum DebugMode {
    DEBUG_SYSTEM,    // システム情報
    DEBUG_INPUT,     // 入力情報
    DEBUG_NETWORK,   // ネットワーク情報（準備）
    DEBUG_MQTT,      // MQTT情報（準備）
    DEBUG_MODE_COUNT
};

DebugMode current_mode = DEBUG_SYSTEM;
unsigned long last_update = 0;
unsigned long mode_change_time = 0;
bool mode_changed = false;

// システム監視用
struct SystemStatus {
    float cpu_usage;
    uint32_t free_heap;
    uint32_t uptime_sec;
    int wifi_strength;
    bool mqtt_connected;
} system_status;

void setup() {
    // M5Unified自動設定初期化
    auto cfg = M5.config();
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=================================");
    Serial.println("M5Stack Atom-JoyStick デバッグ");
    Serial.println("=================================");
    Serial.println("Phase 1: デバッグシステム確認");
    
    // LCD初期化
    M5.Display.clear(BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(WHITE);
    M5.Display.setRotation(0);  // 画面方向設定
    
    Serial.println("✅ デバッグシステム初期化完了");
    Serial.println("操作: ボタンAでモード切り替え、ボタンBで詳細表示");
    
    // 初期システム状態設定
    update_system_status();
    last_update = millis();
}

void loop() {
    M5.update();
    
    // ボタン入力処理
    handle_button_input();
    
    // 500ms間隔で表示更新
    if (millis() - last_update >= 500) {
        update_system_status();
        display_debug_info();
        last_update = millis();
    }
    
    delay(50);
}

void handle_button_input() {
    // ボタンA: モード切り替え
    if (M5.BtnA.wasPressed()) {
        current_mode = (DebugMode)((current_mode + 1) % DEBUG_MODE_COUNT);
        mode_changed = true;
        mode_change_time = millis();
        
        Serial.printf("デバッグモード変更: %d\n", current_mode);
        
        // モード変更効果音
        M5.Speaker.tone(800, 50);
    }
    
    // ボタンB: 詳細情報表示
    if (M5.BtnB.wasPressed()) {
        print_detailed_info();
        
        // 詳細表示効果音
        M5.Speaker.tone(1200, 100);
    }
}

void update_system_status() {
    system_status.free_heap = ESP.getFreeHeap();
    system_status.uptime_sec = millis() / 1000;
    system_status.cpu_usage = 0.0f;  // 将来実装
    system_status.wifi_strength = -99;  // 未接続
    system_status.mqtt_connected = false;  // 未実装
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
        case DEBUG_INPUT:
            display_input_info();
            break;
        case DEBUG_NETWORK:
            display_network_info();
            break;
        case DEBUG_MQTT:
            display_mqtt_info();
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
    
    M5.Display.printf("Uptime: %lus\n", system_status.uptime_sec);
    M5.Display.printf("Heap: %uKB\n", system_status.free_heap / 1024);
    M5.Display.printf("CPU: %dMHz\n", ESP.getCpuFreqMHz());
    
    // ヘルスインジケーター
    uint16_t health_color = (system_status.free_heap > 100000) ? GREEN : RED;
    M5.Display.setTextColor(health_color);
    M5.Display.println("System: OK");
    M5.Display.setTextColor(WHITE);
}

void display_input_info() {
    M5.Display.setTextColor(CYAN);
    M5.Display.println("Input Status");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("----------");
    
    // ジョイスティック値（Atom-JoyStick用 - 後で実装）
    float x_val = 0.0f;  // アナログ読み取り実装予定
    float y_val = 0.0f;
    
    M5.Display.printf("Joy X: %6.1f\n", x_val);
    M5.Display.printf("Joy Y: %6.1f\n", y_val);
    
    // ボタン状態
    uint16_t btn_color_a = M5.BtnA.isPressed() ? RED : WHITE;
    uint16_t btn_color_b = M5.BtnB.isPressed() ? RED : WHITE;
    
    M5.Display.setTextColor(btn_color_a);
    M5.Display.printf("Btn A: %s\n", M5.BtnA.isPressed() ? "ON " : "OFF");
    M5.Display.setTextColor(btn_color_b);
    M5.Display.printf("Btn B: %s\n", M5.BtnB.isPressed() ? "ON " : "OFF");
    M5.Display.setTextColor(WHITE);
}

void display_network_info() {
    M5.Display.setTextColor(MAGENTA);
    M5.Display.println("Network");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("----------");
    
    M5.Display.println("WiFi: OFF");
    M5.Display.println("RSSI: ---");
    M5.Display.println("IP: None");
    M5.Display.setTextColor(YELLOW);
    M5.Display.println("準備中...");
    M5.Display.setTextColor(WHITE);
}

void display_mqtt_info() {
    M5.Display.setTextColor(ORANGE);
    M5.Display.println("MQTT Status");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("----------");
    
    M5.Display.println("Broker: OFF");
    M5.Display.println("Clients: 0");
    M5.Display.println("Topics: 0");
    M5.Display.setTextColor(YELLOW);
    M5.Display.println("準備中...");
    M5.Display.setTextColor(WHITE);
}

const char* get_mode_name(DebugMode mode) {
    switch (mode) {
        case DEBUG_SYSTEM: return "SYSTEM";
        case DEBUG_INPUT: return "INPUT";
        case DEBUG_NETWORK: return "NETWORK";
        case DEBUG_MQTT: return "MQTT";
        default: return "UNKNOWN";
    }
}

void print_detailed_info() {
    Serial.println("\n=== 詳細システム情報 ===");
    Serial.printf("現在モード: %s\n", get_mode_name(current_mode));
    Serial.printf("アップタイム: %lu秒\n", system_status.uptime_sec);
    Serial.printf("フリーヒープ: %u bytes (%u KB)\n", 
                  system_status.free_heap, system_status.free_heap / 1024);
    Serial.printf("チップ情報: %s Rev.%d\n", 
                  ESP.getChipModel(), ESP.getChipRevision());
    Serial.printf("フラッシュサイズ: %u KB\n", ESP.getFlashChipSize() / 1024);
    Serial.printf("CPU周波数: %d MHz\n", ESP.getCpuFreqMHz());
    
    // ジョイスティック詳細（Atom-JoyStick用 - 後で実装）
    float x_val = 0.0f;  // アナログ読み取り実装予定
    float y_val = 0.0f;
    Serial.printf("ジョイスティック: X=%.3f, Y=%.3f\n", x_val, y_val);
    
    // ボタン状態
    Serial.printf("ボタンA: %s, ボタンB: %s\n", 
                  M5.BtnA.isPressed() ? "押下中" : "解放",
                  M5.BtnB.isPressed() ? "押下中" : "解放");
    
    Serial.println("========================\n");
}