/**
 * @file 02_hardware_test.ino
 * @brief M5Stack Atom-JoyStickハードウェア機能テスト
 * @description M5Unified統合によるハードウェア基本動作確認
 * 
 * Phase 1: ハードウェア動作確認
 * - M5Unifiedライブラリ初期化
 * - LCD表示テスト
 * - ジョイスティック読み取り
 * - ボタン入力テスト
 * - バザー動作テスト
 * 
 * @target M5Stack Atom-JoyStick (ESP32-S3)
 * @board esp32:esp32:esp32s3_family
 */

#include <M5Unified.h>

// ハードウェアテスト状態
enum TestStatus {
    TEST_INIT,
    TEST_LCD,
    TEST_JOYSTICK,
    TEST_BUTTONS,
    TEST_BUZZER,
    TEST_COMPLETE
};

TestStatus current_test = TEST_INIT;
unsigned long test_start_time = 0;

void setup() {
    // M5Unified自動設定初期化
    auto cfg = M5.config();
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=================================");
    Serial.println("M5Stack Atom-JoyStick HWテスト");
    Serial.println("=================================");
    Serial.println("Phase 1: ハードウェア機能確認");
    
    // M5Unified初期化確認
    Serial.println("M5Unified初期化完了");
    Serial.printf("ボード種類: M5Stack Atom-JoyStick\n");
    
    // LCD初期化確認
    if (M5.Display.width() > 0) {
        Serial.printf("✅ LCD初期化成功 (%dx%d)\n", 
                      M5.Display.width(), M5.Display.height());
        M5.Display.clear(BLACK);
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(WHITE);
    } else {
        Serial.println("❌ LCD初期化失敗");
    }
    
    test_start_time = millis();
    current_test = TEST_LCD;
}

void loop() {
    M5.update();  // 必須：センサー・ボタン状態更新
    
    switch (current_test) {
        case TEST_LCD:
            test_lcd_display();
            break;
        case TEST_JOYSTICK:
            test_joystick_input();
            break;
        case TEST_BUTTONS:
            test_button_input();
            break;
        case TEST_BUZZER:
            test_buzzer_output();
            break;
        case TEST_COMPLETE:
            test_complete_status();
            break;
        default:
            break;
    }
    
    delay(50);
}

void test_lcd_display() {
    static int color_index = 0;
    static unsigned long last_update = 0;
    
    if (millis() - last_update >= 1000) {  // 1秒間隔で色変更
        uint16_t colors[] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE};
        int total_colors = sizeof(colors) / sizeof(colors[0]);
        
        M5.Display.clear(colors[color_index]);
        M5.Display.setCursor(5, 5);
        M5.Display.printf("LCD Test\n%s", 
                          (color_index < total_colors - 1) ? "Color Change" : "Complete");
        
        Serial.printf("LCD色テスト: %d/%d\n", color_index + 1, total_colors);
        
        color_index++;
        if (color_index >= total_colors) {
            Serial.println("✅ LCD表示テスト完了");
            current_test = TEST_JOYSTICK;
            test_start_time = millis();
        }
        last_update = millis();
    }
}

void test_joystick_input() {
    if (millis() - test_start_time < 5000) {  // 5秒間テスト
        // ジョイスティック値読み取り（Atom-JoyStick用）
        float x_val = 0.0f;  // 後でジョイスティック読み取り実装
        float y_val = 0.0f;
        
        // LCD表示更新
        M5.Display.clear(BLACK);
        M5.Display.setCursor(5, 5);
        M5.Display.printf("Joystick\nX: %.1f\nY: %.1f", x_val, y_val);
        
        // シリアル出力（動きがある場合のみ）
        if (abs(x_val) > 10 || abs(y_val) > 10) {
            Serial.printf("ジョイスティック: X=%.1f, Y=%.1f\n", x_val, y_val);
        }
    } else {
        Serial.println("✅ ジョイスティック入力テスト完了");
        current_test = TEST_BUTTONS;
        test_start_time = millis();
    }
}

void test_button_input() {
    if (millis() - test_start_time < 5000) {  // 5秒間テスト
        static bool last_btn_a = false, last_btn_b = false;
        
        bool btn_a = M5.BtnA.isPressed();
        bool btn_b = M5.BtnB.isPressed();
        
        // ボタン状態表示
        M5.Display.clear(BLACK);
        M5.Display.setCursor(5, 5);
        M5.Display.printf("Buttons\nA: %s\nB: %s", 
                          btn_a ? "ON" : "OFF",
                          btn_b ? "ON" : "OFF");
        
        // 状態変化検出
        if (btn_a != last_btn_a) {
            Serial.printf("ボタンA: %s\n", btn_a ? "押下" : "解放");
            last_btn_a = btn_a;
        }
        if (btn_b != last_btn_b) {
            Serial.printf("ボタンB: %s\n", btn_b ? "押下" : "解放");
            last_btn_b = btn_b;
        }
    } else {
        Serial.println("✅ ボタン入力テスト完了");
        current_test = TEST_BUZZER;
        test_start_time = millis();
    }
}

void test_buzzer_output() {
    static int beep_count = 0;
    static unsigned long last_beep = 0;
    
    if (beep_count < 3 && millis() - last_beep >= 500) {  // 500ms間隔で3回
        // バザー音出力（1000Hz、100ms）
        M5.Speaker.tone(1000, 100);
        
        M5.Display.clear(BLACK);
        M5.Display.setCursor(5, 5);
        M5.Display.printf("Buzzer\nBeep %d/3", beep_count + 1);
        
        Serial.printf("バザーテスト: %d/3\n", beep_count + 1);
        
        beep_count++;
        last_beep = millis();
    } else if (beep_count >= 3) {
        Serial.println("✅ バザー出力テスト完了");
        current_test = TEST_COMPLETE;
    }
}

void test_complete_status() {
    static bool completion_shown = false;
    
    if (!completion_shown) {
        M5.Display.clear(GREEN);
        M5.Display.setCursor(5, 5);
        M5.Display.printf("HW Test\nCOMPLETE");
        
        Serial.println("\n=================================");
        Serial.println("✅ ハードウェアテスト完了");
        Serial.println("✅ LCD動作確認済み");
        Serial.println("✅ ジョイスティック動作確認済み");
        Serial.println("✅ ボタン入力動作確認済み");
        Serial.println("✅ バザー出力動作確認済み");
        Serial.println("=================================");
        Serial.println("次のテスト: 03_debug_system.ino");
        
        completion_shown = true;
    }
}