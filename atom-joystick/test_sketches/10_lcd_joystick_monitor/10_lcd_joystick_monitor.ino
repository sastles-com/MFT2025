/**
 * @file 10_lcd_joystick_monitor.ino
 * @brief M5Stack Atom-JoyStick LCD中心Joystick監視システム
 * @description LCD表示でJoystick入力を確認・GPIO特定
 * 
 * Phase 4.3: LCD中心Joystick動作確認
 * - リアルタイムADC値LCD表示
 * - 変化検出・強調表示
 * - ボタン操作でピン切り替え
 * 
 * @target M5Stack Atom-JoyStick (ESP32-S3)
 */

#include <M5Unified.h>

// 候補GPIOピン（前回テストで有効なピン）
const int CANDIDATE_PINS[] = {2, 3, 5, 15, 16, 17, 18};
const int PIN_COUNT = sizeof(CANDIDATE_PINS) / sizeof(CANDIDATE_PINS[0]);

// ベースライン値（前回測定結果）
const int BASELINE_VALUES[] = {525, 200, 305, 320, 3675, 530, 240};

// 表示設定
const int UPDATE_INTERVAL = 100;  // 100ms更新
const int CHANGE_THRESHOLD = 50;  // 変化検出閾値
const int MAX_DISPLAYED_PINS = 6; // LCD表示可能ピン数

// 現在の状態
struct PinStatus {
    int current_value;
    int baseline;
    int change_amount;
    bool is_active;
};

PinStatus pin_status[PIN_COUNT];
unsigned long last_update = 0;
int display_offset = 0;  // 表示開始ピン番号
int max_change_pin = -1; // 最大変化ピン

void setup() {
    // M5Unified初期化
    auto cfg = M5.config();
    cfg.external_spk = false;
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("==========================================");
    Serial.println("M5Stack Atom-JoyStick LCD監視システム");
    Serial.println("==========================================");
    
    // LCD初期化
    M5.Display.clear(BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setRotation(0);
    
    // ピン状態初期化
    for (int i = 0; i < PIN_COUNT; i++) {
        pin_status[i].baseline = BASELINE_VALUES[i];
        pin_status[i].current_value = 0;
        pin_status[i].change_amount = 0;
        pin_status[i].is_active = false;
    }
    
    Serial.println("✅ LCD監視システム初期化完了");
    Serial.println("操作:");
    Serial.println("  - Joystickを動かしてピン変化を確認");
    Serial.println("  - ボタンAで表示ピン切り替え");
    
    display_welcome_screen();
    delay(2000);
    
    last_update = millis();
}

void loop() {
    M5.update();
    
    // ボタン入力処理
    if (M5.BtnA.wasPressed()) {
        display_offset = (display_offset + MAX_DISPLAYED_PINS) % PIN_COUNT;
        Serial.printf("表示切り替え: オフセット %d\n", display_offset);
    }
    
    // 定期更新
    if (millis() - last_update >= UPDATE_INTERVAL) {
        update_pin_status();
        update_lcd_display();
        last_update = millis();
    }
    
    delay(10);
}

void display_welcome_screen() {
    M5.Display.clear(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextColor(GREEN);
    M5.Display.setTextSize(2);
    M5.Display.println("Joystick");
    M5.Display.println("Monitor");
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(WHITE);
    M5.Display.println("");
    M5.Display.println("Ready to scan");
    M5.Display.println("Move joystick!");
}

void update_pin_status() {
    max_change_pin = -1;
    int max_change_amount = 0;
    
    for (int i = 0; i < PIN_COUNT; i++) {
        // ADC値読み取り
        pin_status[i].current_value = analogRead(CANDIDATE_PINS[i]);
        
        // 変化量計算
        pin_status[i].change_amount = abs(pin_status[i].current_value - pin_status[i].baseline);
        
        // アクティブ判定
        pin_status[i].is_active = (pin_status[i].change_amount >= CHANGE_THRESHOLD);
        
        // 最大変化ピン追跡
        if (pin_status[i].change_amount > max_change_amount) {
            max_change_amount = pin_status[i].change_amount;
            max_change_pin = i;
        }
        
        // アクティブピンのログ出力
        if (pin_status[i].is_active) {
            Serial.printf("🎯 GPIO%d: %d (変化:%d)\n", 
                          CANDIDATE_PINS[i], 
                          pin_status[i].current_value, 
                          pin_status[i].change_amount);
        }
    }
}

void update_lcd_display() {
    M5.Display.clear(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(1);
    
    // ヘッダー
    M5.Display.setTextColor(CYAN);
    M5.Display.println("Joystick Monitor");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("Pin  Value Change");
    M5.Display.println("----------------");
    
    // ピン情報表示（最大6ピン）
    for (int i = 0; i < MAX_DISPLAYED_PINS && (display_offset + i) < PIN_COUNT; i++) {
        int pin_index = display_offset + i;
        int gpio_num = CANDIDATE_PINS[pin_index];
        int value = pin_status[pin_index].current_value;
        int change = pin_status[pin_index].change_amount;
        bool active = pin_status[pin_index].is_active;
        
        // 色設定
        if (active) {
            if (pin_index == max_change_pin) {
                M5.Display.setTextColor(RED);    // 最大変化ピン: 赤
            } else {
                M5.Display.setTextColor(YELLOW); // アクティブピン: 黄
            }
        } else {
            M5.Display.setTextColor(WHITE);      // 通常ピン: 白
        }
        
        // 表示（フォーマット: "GPIO02 1234  +89"）
        M5.Display.printf("G%02d %4d %c%3d\n", 
                          gpio_num, value, 
                          active ? '+' : ' ', change);
    }
    
    // フッター情報
    M5.Display.setTextColor(GREEN);
    M5.Display.printf("\nBtn:Switch %d/%d\n", 
                      display_offset / MAX_DISPLAYED_PINS + 1, 
                      (PIN_COUNT + MAX_DISPLAYED_PINS - 1) / MAX_DISPLAYED_PINS);
    
    // 最大変化ピン強調表示
    if (max_change_pin >= 0 && pin_status[max_change_pin].is_active) {
        M5.Display.setTextColor(RED);
        M5.Display.printf("MAX: GPIO%d (%d)\n", 
                          CANDIDATE_PINS[max_change_pin], 
                          pin_status[max_change_pin].change_amount);
    }
    
    // 動作確認ドット
    static int dot_counter = 0;
    dot_counter++;
    M5.Display.setTextColor(dot_counter % 10 < 5 ? BLUE : BLACK);
    M5.Display.print("*");
}