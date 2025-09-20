/**
 * @file 09_simple_joystick_test.ino
 * @brief M5Stack Atom-JoyStick 簡単Joystick読み取りテスト
 * @description 最小限のコードでJoystick入力を確認
 * 
 * Phase 4.2: 安定動作Joystickテスト
 * - WiFi無効・最小限の機能のみ
 * - ADC読み取り・シリアル出力のみ
 * - リセットループ問題回避
 * 
 * @target M5Stack Atom-JoyStick (ESP32-S3)
 */

#include <M5Unified.h>

// テスト対象GPIOピン（M5Stack Atom-JoyStick推定ピン）
const int TEST_PINS[] = {0, 1, 2, 3, 5, 15, 16, 17, 18, 21};
const int TEST_PIN_COUNT = sizeof(TEST_PINS) / sizeof(TEST_PINS[0]);

// スキャン設定
const int SCAN_INTERVAL = 500;  // 500ms間隔（安定動作優先）
const int SAMPLE_COUNT = 5;     // 5回平均
unsigned long last_scan = 0;

void setup() {
    // M5Unified最小初期化
    auto cfg = M5.config();
    cfg.external_spk = false;  // スピーカー無効
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(2000);  // 安定待機
    
    Serial.println("===========================================");
    Serial.println("M5Stack Atom-JoyStick 簡単テスト");
    Serial.println("===========================================");
    Serial.println("WiFi無効・最小限Joystick読み取りテスト");
    Serial.println();
    
    // LCD基本表示
    M5.Display.clear(BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Joystick");
    M5.Display.println("Test");
    M5.Display.println("Ready!");
    
    Serial.println("✅ 初期化完了");
    Serial.printf("✅ テスト対象: %dピン\n", TEST_PIN_COUNT);
    Serial.println("✅ 操作: Joystickを動かしてください");
    Serial.println();
    
    last_scan = millis();
}

void loop() {
    M5.update();
    
    // 500ms間隔でスキャン
    if (millis() - last_scan >= SCAN_INTERVAL) {
        perform_simple_scan();
        last_scan = millis();
    }
    
    delay(10);  // 安定動作
}

void perform_simple_scan() {
    Serial.println("--- ADC読み取り結果 ---");
    
    for (int i = 0; i < TEST_PIN_COUNT; i++) {
        int pin = TEST_PINS[i];
        
        // 複数回サンプリングして平均化
        int total = 0;
        for (int j = 0; j < SAMPLE_COUNT; j++) {
            total += analogRead(pin);
            delay(1);
        }
        int average = total / SAMPLE_COUNT;
        
        Serial.printf("GPIO%02d: %4d", pin, average);
        
        // 特徴的な値の場合は強調表示
        if (average < 100) {
            Serial.print(" [LOW]");
        } else if (average > 3900) {
            Serial.print(" [HIGH]");
        } else if (average > 1800 && average < 2300) {
            Serial.print(" [MID]");
        }
        
        Serial.println();
    }
    
    Serial.println("------------------------");
    Serial.println();
    
    // LCD更新
    update_lcd_display();
}

void update_lcd_display() {
    static int update_count = 0;
    update_count++;
    
    M5.Display.clear(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    
    M5.Display.setTextColor(GREEN);
    M5.Display.println("Joystick");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("Test");
    
    M5.Display.printf("Scan: %d\n", update_count);
    
    // 最も特徴的なピンの値を表示
    int pin0_val = analogRead(TEST_PINS[0]);
    int pin1_val = analogRead(TEST_PINS[1]);
    
    M5.Display.printf("P0:%d\n", pin0_val);
    M5.Display.printf("P1:%d\n", pin1_val);
    
    // 動作確認用ドット
    M5.Display.setTextColor(update_count % 2 ? RED : BLUE);
    M5.Display.println("*");
}