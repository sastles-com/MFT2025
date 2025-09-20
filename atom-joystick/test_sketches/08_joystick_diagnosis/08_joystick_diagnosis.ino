/**
 * @file 08_joystick_diagnosis.ino
 * @brief M5Stack Atom-JoyStick GPIO診断システム
 * @description 全てのADCピンをスキャンしてJoystick接続を発見
 * 
 * Phase 4.1: Joystick GPIO診断
 * - 複数ADCピン同時監視（GPIO0-39）
 * - リアルタイム値変化検出
 * - 物理Joystick操作との対応確認
 * 
 * @target M5Stack Atom-JoyStick (ESP32-S3)
 * @protocol GPIO診断・ADC全チャンネルスキャン
 */

#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiAP.h>

// WiFiアクセスポイント設定（診断用）
const char* AP_SSID = "IsolationSphere-Diagnosis";
const char* AP_PASSWORD = "joystick-diagnosis-2025";
const IPAddress AP_IP(192, 168, 100, 1);
const IPAddress AP_GATEWAY(192, 168, 100, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

// Joystick診断設定
const int SCAN_INTERVAL = 100;  // 100ms間隔スキャン
const int CHANGE_THRESHOLD = 50; // 変化検出閾値
const int SAMPLE_COUNT = 10;     // サンプリング回数

// 診断対象GPIOピン（ESP32-S3 ADC対応ピン）
const int GPIO_PINS[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 47, 48  // ESP32-S3特有ADCピン追加
};
const int PIN_COUNT = sizeof(GPIO_PINS) / sizeof(GPIO_PINS[0]);

// 各ピンの状態管理
struct PinState {
    int current_value;
    int baseline_value;
    int min_value;
    int max_value;
    int change_count;
    bool active;
    unsigned long last_change;
};

PinState pin_states[PIN_COUNT];
unsigned long last_scan = 0;
int scan_cycle = 0;

// デバッグモード
enum DiagnosisMode {
    DIAG_OVERVIEW,    // 全ピン概要
    DIAG_ACTIVE,      // アクティブピンのみ
    DIAG_DETAILED,    // 詳細数値表示
    DIAG_MODE_COUNT
};

DiagnosisMode current_mode = DIAG_OVERVIEW;
unsigned long last_update = 0;
unsigned long mode_change_time = 0;
bool mode_changed = false;

void setup() {
    // M5Unified初期化
    auto cfg = M5.config();
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("========================================");
    Serial.println("M5Stack Atom-JoyStick GPIO診断システム");
    Serial.println("========================================");
    Serial.println("Phase 4.1: Joystick GPIO診断");
    
    // LCD初期化
    M5.Display.clear(BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(WHITE);
    M5.Display.setRotation(0);
    
    // WiFi初期化（診断用）
    init_diagnosis_wifi();
    
    // GPIO診断初期化
    init_gpio_diagnosis();
    
    Serial.println("✅ GPIO診断システム初期化完了");
    Serial.println("操作: Joystickを動かして反応するピンを特定");
    Serial.println("      ボタンAで表示モード切り替え");
    
    last_scan = millis();
    last_update = millis();
}

void loop() {
    M5.update();
    
    // GPIO診断スキャン
    if (millis() - last_scan >= SCAN_INTERVAL) {
        perform_gpio_scan();
        last_scan = millis();
    }
    
    // ボタン入力処理
    handle_button_input();
    
    // 500ms間隔で表示更新
    if (millis() - last_update >= 500) {
        display_diagnosis_info();
        last_update = millis();
    }
    
    delay(10);
}

void init_diagnosis_wifi() {
    Serial.println("診断用WiFi AP設定開始...");
    
    WiFi.mode(WIFI_AP);
    delay(100);
    
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
    bool ap_result = WiFi.softAP(AP_SSID, AP_PASSWORD, 1, 0, 4);
    
    if (ap_result) {
        Serial.printf("✅ 診断用WiFi AP作成成功: %s\n", AP_SSID);
        Serial.printf("✅ IP アドレス: %s\n", WiFi.softAPIP().toString().c_str());
    } else {
        Serial.println("❌ 診断用WiFi AP作成失敗");
    }
    
    delay(1000);
}

void init_gpio_diagnosis() {
    Serial.println("GPIO診断システム初期化...");
    
    // 全診断ピンの初期化
    for (int i = 0; i < PIN_COUNT; i++) {
        int gpio_pin = GPIO_PINS[i];
        
        // ADC初期値取得（ベースライン）
        int initial_value = 0;
        for (int j = 0; j < SAMPLE_COUNT; j++) {
            initial_value += analogRead(gpio_pin);
            delay(1);
        }
        initial_value /= SAMPLE_COUNT;
        
        pin_states[i].current_value = initial_value;
        pin_states[i].baseline_value = initial_value;
        pin_states[i].min_value = initial_value;
        pin_states[i].max_value = initial_value;
        pin_states[i].change_count = 0;
        pin_states[i].active = false;
        pin_states[i].last_change = 0;
        
        Serial.printf("GPIO%d ベースライン: %d\n", gpio_pin, initial_value);
    }
    
    Serial.printf("✅ 診断対象: %dピン\n", PIN_COUNT);
    Serial.printf("✅ スキャン間隔: %dms\n", SCAN_INTERVAL);
    Serial.printf("✅ 変化検出閾値: %d\n", CHANGE_THRESHOLD);
}

void perform_gpio_scan() {
    scan_cycle++;
    int active_pins = 0;
    
    for (int i = 0; i < PIN_COUNT; i++) {
        int gpio_pin = GPIO_PINS[i];
        int new_value = analogRead(gpio_pin);
        
        // 変化検出
        int change_from_baseline = abs(new_value - pin_states[i].baseline_value);
        int change_from_current = abs(new_value - pin_states[i].current_value);
        
        if (change_from_baseline > CHANGE_THRESHOLD) {
            if (!pin_states[i].active) {
                pin_states[i].active = true;
                pin_states[i].last_change = millis();
                Serial.printf("📍 GPIO%d アクティブ検出! 値変化: %d -> %d (差分: %d)\n", 
                              gpio_pin, pin_states[i].current_value, new_value, change_from_baseline);
                M5.Speaker.tone(1000, 50);  // 検出音
            }
            pin_states[i].change_count++;
        }
        
        // 統計更新
        pin_states[i].current_value = new_value;
        if (new_value < pin_states[i].min_value) pin_states[i].min_value = new_value;
        if (new_value > pin_states[i].max_value) pin_states[i].max_value = new_value;
        
        if (pin_states[i].active) active_pins++;
        
        // アクティブピンのリアルタイム監視（10回に1回）
        if (pin_states[i].active && scan_cycle % 10 == 0) {
            Serial.printf("🕹️ GPIO%d: 現在値=%d (範囲: %d-%d) 変化回数:%d\n", 
                          gpio_pin, new_value, 
                          pin_states[i].min_value, pin_states[i].max_value,
                          pin_states[i].change_count);
        }
    }
    
    // 診断サマリー（100回に1回）
    if (scan_cycle % 100 == 0) {
        Serial.printf("📊 診断サマリー: サイクル%d アクティブピン数:%d\n", scan_cycle, active_pins);
    }
}

void handle_button_input() {
    static bool long_press_executed = false;
    
    // 短押し: モード切り替え
    if (M5.BtnA.wasPressed()) {
        current_mode = (DiagnosisMode)((current_mode + 1) % DIAG_MODE_COUNT);
        mode_changed = true;
        mode_change_time = millis();
        
        Serial.printf("診断表示モード変更: %s\n", get_mode_name(current_mode));
        M5.Speaker.tone(800, 50);
    }
    
    // 長押し: 詳細レポート出力（2秒）
    if (M5.BtnA.pressedFor(2000) && !long_press_executed) {
        Serial.println("DEBUG: ボタンA長押し検出 - 詳細診断レポート出力");
        print_detailed_diagnosis_report();
        M5.Speaker.tone(1500, 200);  // 長押し確認音
        long_press_executed = true;
    }
    
    // 長押し状態リセット
    if (!M5.BtnA.isPressed()) {
        long_press_executed = false;
    }
}

void display_diagnosis_info() {
    M5.Display.clear(BLACK);
    M5.Display.setCursor(2, 2);
    
    if (mode_changed && millis() - mode_change_time < 2000) {
        M5.Display.setTextColor(YELLOW);
        M5.Display.printf("->%s\n", get_mode_name(current_mode));
        M5.Display.setTextColor(WHITE);
        M5.Display.println("-----");
    }
    
    switch (current_mode) {
        case DIAG_OVERVIEW:
            display_overview_info();
            break;
        case DIAG_ACTIVE:
            display_active_pins_info();
            break;
        case DIAG_DETAILED:
            display_detailed_info();
            break;
        default:
            break;
    }
    
    if (mode_changed && millis() - mode_change_time >= 2000) {
        mode_changed = false;
    }
}

void display_overview_info() {
    M5.Display.setTextColor(GREEN);
    M5.Display.println("GPIO診断");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("-------");
    
    int active_count = 0;
    for (int i = 0; i < PIN_COUNT; i++) {
        if (pin_states[i].active) active_count++;
    }
    
    M5.Display.printf("スキャン: %d\n", scan_cycle);
    M5.Display.printf("対象: %dピン\n", PIN_COUNT);
    M5.Display.printf("アクティブ: %d\n", active_count);
    M5.Display.printf("閾値: %d\n", CHANGE_THRESHOLD);
}

void display_active_pins_info() {
    M5.Display.setTextColor(CYAN);
    M5.Display.println("アクティブ");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("--------");
    
    int displayed = 0;
    for (int i = 0; i < PIN_COUNT && displayed < 4; i++) {
        if (pin_states[i].active) {
            M5.Display.printf("GPIO%d:%d\n", GPIO_PINS[i], pin_states[i].current_value);
            displayed++;
        }
    }
    
    if (displayed == 0) {
        M5.Display.setTextColor(YELLOW);
        M5.Display.println("未検出");
        M5.Display.setTextColor(WHITE);
    }
}

void display_detailed_info() {
    M5.Display.setTextColor(ORANGE);
    M5.Display.println("詳細監視");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("-------");
    
    // 最も変化の多いピンを表示
    int most_active_index = 0;
    int max_changes = 0;
    
    for (int i = 0; i < PIN_COUNT; i++) {
        if (pin_states[i].change_count > max_changes) {
            max_changes = pin_states[i].change_count;
            most_active_index = i;
        }
    }
    
    if (max_changes > 0) {
        int gpio_pin = GPIO_PINS[most_active_index];
        M5.Display.printf("GPIO%d\n", gpio_pin);
        M5.Display.printf("値:%d\n", pin_states[most_active_index].current_value);
        M5.Display.printf("変化:%d回\n", pin_states[most_active_index].change_count);
        M5.Display.printf("範囲:%d-%d\n", 
                          pin_states[most_active_index].min_value,
                          pin_states[most_active_index].max_value);
    } else {
        M5.Display.setTextColor(YELLOW);
        M5.Display.println("未検出");
        M5.Display.setTextColor(WHITE);
    }
}

const char* get_mode_name(DiagnosisMode mode) {
    switch (mode) {
        case DIAG_OVERVIEW: return "OVERVIEW";
        case DIAG_ACTIVE: return "ACTIVE";
        case DIAG_DETAILED: return "DETAILED";
        default: return "UNKNOWN";
    }
}

void print_detailed_diagnosis_report() {
    Serial.println("\n========== 詳細GPIO診断レポート ==========");
    Serial.printf("診断開始からの経過時間: %lu秒\n", millis() / 1000);
    Serial.printf("総スキャン回数: %d回\n", scan_cycle);
    Serial.printf("診断対象ピン数: %d個\n", PIN_COUNT);
    
    Serial.println("\n--- 全GPIO状態一覧 ---");
    Serial.println("GPIO | 現在値 | ベース | 最小-最大  | 変化数 | アクティブ");
    Serial.println("-----|--------|--------|-----------|-------|----------");
    
    int active_count = 0;
    for (int i = 0; i < PIN_COUNT; i++) {
        int gpio_pin = GPIO_PINS[i];
        Serial.printf("%-4d | %-6d | %-6d | %-4d-%-4d | %-5d | %s\n",
                      gpio_pin,
                      pin_states[i].current_value,
                      pin_states[i].baseline_value,
                      pin_states[i].min_value,
                      pin_states[i].max_value,
                      pin_states[i].change_count,
                      pin_states[i].active ? "YES" : "NO");
        
        if (pin_states[i].active) active_count++;
    }
    
    Serial.printf("\n--- 診断結果サマリー ---\n");
    Serial.printf("アクティブピン数: %d/%d\n", active_count, PIN_COUNT);
    Serial.printf("変化検出閾値: %d\n", CHANGE_THRESHOLD);
    Serial.printf("スキャン間隔: %dms\n", SCAN_INTERVAL);
    
    if (active_count > 0) {
        Serial.println("\n✅ 推奨Joystick GPIOピン:");
        for (int i = 0; i < PIN_COUNT; i++) {
            if (pin_states[i].active && pin_states[i].change_count >= 5) {
                Serial.printf("   GPIO%d: 変化回数%d回 (範囲: %d-%d)\n",
                              GPIO_PINS[i], pin_states[i].change_count,
                              pin_states[i].min_value, pin_states[i].max_value);
            }
        }
    } else {
        Serial.println("\n⚠️ アクティブピンが検出されていません");
        Serial.println("   - Joystickを物理的に動かしてください");
        Serial.println("   - 配線・接続を確認してください");
        Serial.println("   - 閾値を下げて再テストしてください");
    }
    
    Serial.println("=====================================\n");
}