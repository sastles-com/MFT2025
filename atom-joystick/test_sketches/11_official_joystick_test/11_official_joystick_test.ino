/**
 * @file 11_official_joystick_test.ino
 * @brief M5Stack Atom-JoyStick 公式ピン仕様テスト
 * @description GPIO38/39のI2C通信によるJoystick読み取り確認
 * 
 * Phase 4.4: 公式仕様準拠テスト
 * - GPIO38(SDA), GPIO39(SCL)のI2C通信
 * - M5公式ライブラリ活用
 * - 正確なJoystick値取得
 * 
 * @target M5Stack Atom-JoyStick (ESP32-S3)
 * @reference https://github.com/m5stack/Atom-JoyStick/examples/GetValue
 */

#include <M5Unified.h>
#include <Wire.h>

// I2C設定（公式仕様）
const uint8_t ATOM_JOYSTICK_ADDR = 0x38;  // I2Cアドレス
const int I2C_SDA_PIN = 38;               // SDAピン
const int I2C_SCL_PIN = 39;               // SCLピン
const uint32_t I2C_FREQUENCY = 400000U;   // 400kHz

// Joystickデータ
struct JoystickData {
    uint8_t x_8bit;         // X軸 8bit値
    uint8_t y_8bit;         // Y軸 8bit値
    bool button_pressed;    // ボタン状態
    uint16_t x_12bit;       // X軸 12bit値
    uint16_t y_12bit;       // Y軸 12bit値
    bool valid;             // データ有効性
};

JoystickData joystick_data;
unsigned long last_update = 0;
const int UPDATE_INTERVAL = 100;  // 100ms更新

// 表示関連
int display_mode = 0;
const int DISPLAY_MODE_COUNT = 3;
unsigned long last_button_press = 0;
const int BUTTON_DEBOUNCE = 200;

void setup() {
    // M5Unified初期化
    auto cfg = M5.config();
    cfg.external_spk = false;
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("===========================================");
    Serial.println("M5Stack Atom-JoyStick 公式仕様テスト");
    Serial.println("===========================================");
    Serial.printf("I2C: SDA=GPIO%d, SCL=GPIO%d, Addr=0x%02X\n", 
                  I2C_SDA_PIN, I2C_SCL_PIN, ATOM_JOYSTICK_ADDR);
    
    // I2C初期化
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
    
    // LCD初期化
    M5.Display.clear(BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setRotation(0);
    
    // 初期データ
    joystick_data.valid = false;
    
    Serial.println("✅ 公式仕様テスト初期化完了");
    Serial.println("操作:");
    Serial.println("  - Joystickを動かしてI2C通信確認");
    Serial.println("  - ボタンAでLCD表示モード切り替え");
    
    display_welcome_screen();
    delay(2000);
    
    last_update = millis();
}

void loop() {
    M5.update();
    
    // ボタン入力処理
    handle_button_input();
    
    // 定期更新
    if (millis() - last_update >= UPDATE_INTERVAL) {
        read_joystick_data();
        update_display();
        last_update = millis();
    }
    
    delay(10);
}

void display_welcome_screen() {
    M5.Display.clear(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextColor(GREEN);
    M5.Display.setTextSize(2);
    M5.Display.println("Official");
    M5.Display.println("Joystick");
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(WHITE);
    M5.Display.println("");
    M5.Display.printf("I2C: %d/%d\n", I2C_SDA_PIN, I2C_SCL_PIN);
    M5.Display.printf("Addr: 0x%02X\n", ATOM_JOYSTICK_ADDR);
}

void read_joystick_data() {
    // I2Cデバイス接続確認
    Wire.beginTransmission(ATOM_JOYSTICK_ADDR);
    uint8_t i2c_error = Wire.endTransmission();
    
    if (i2c_error == 0) {
        // I2C通信成功 - データ読み取り
        Wire.requestFrom(ATOM_JOYSTICK_ADDR, (uint8_t)3);  // 3バイト要求
        
        if (Wire.available() >= 3) {
            joystick_data.x_8bit = Wire.read();
            joystick_data.y_8bit = Wire.read();
            uint8_t button_data = Wire.read();
            joystick_data.button_pressed = (button_data == 0);
            
            // 8bit → 12bit変換
            joystick_data.x_12bit = joystick_data.x_8bit << 4;
            joystick_data.y_12bit = joystick_data.y_8bit << 4;
            
            joystick_data.valid = true;
            
            // シリアルログ出力（10回に1回）
            static int log_counter = 0;
            if (++log_counter >= 10) {
                Serial.printf("🕹️ X=%d Y=%d Btn=%s (I2C: OK)\n", 
                              joystick_data.x_8bit, joystick_data.y_8bit,
                              joystick_data.button_pressed ? "ON" : "OFF");
                log_counter = 0;
            }
            
        } else {
            joystick_data.valid = false;
            Serial.println("⚠️ I2Cデータ不足");
        }
    } else {
        joystick_data.valid = false;
        static int error_counter = 0;
        if (++error_counter >= 20) {  // 2秒に1回エラーログ
            Serial.printf("❌ I2C通信エラー: %d (アドレス: 0x%02X)\n", 
                          i2c_error, ATOM_JOYSTICK_ADDR);
            error_counter = 0;
        }
    }
}

void handle_button_input() {
    if (M5.BtnA.wasPressed() && (millis() - last_button_press) > BUTTON_DEBOUNCE) {
        display_mode = (display_mode + 1) % DISPLAY_MODE_COUNT;
        last_button_press = millis();
        Serial.printf("表示モード変更: %d\n", display_mode);
    }
}

void update_display() {
    M5.Display.clear(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(1);
    
    switch (display_mode) {
        case 0:
            display_main_info();
            break;
        case 1:
            display_raw_values();
            break;
        case 2:
            display_i2c_status();
            break;
    }
    
    // 動作確認ドット
    static int dot_counter = 0;
    dot_counter++;
    M5.Display.setCursor(120, 120);
    M5.Display.setTextColor(dot_counter % 10 < 5 ? BLUE : BLACK);
    M5.Display.print("*");
}

void display_main_info() {
    M5.Display.setTextColor(CYAN);
    M5.Display.println("Official Joystick");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("----------------");
    
    if (joystick_data.valid) {
        // X軸表示
        M5.Display.setTextColor(GREEN);
        M5.Display.printf("X: %3d ", joystick_data.x_8bit);
        draw_bar(joystick_data.x_8bit, 255);
        M5.Display.println();
        
        // Y軸表示
        M5.Display.printf("Y: %3d ", joystick_data.y_8bit);
        draw_bar(joystick_data.y_8bit, 255);
        M5.Display.println();
        
        // ボタン状態
        M5.Display.setTextColor(joystick_data.button_pressed ? RED : WHITE);
        M5.Display.printf("Btn: %s\n", joystick_data.button_pressed ? "PRESSED" : "Released");
        
        // 正規化値
        M5.Display.setTextColor(YELLOW);
        float x_norm = (joystick_data.x_8bit - 128) / 128.0f;
        float y_norm = (joystick_data.y_8bit - 128) / 128.0f;
        M5.Display.printf("Norm X: %+.2f\n", x_norm);
        M5.Display.printf("Norm Y: %+.2f\n", y_norm);
        
    } else {
        M5.Display.setTextColor(RED);
        M5.Display.println("I2C ERROR");
        M5.Display.setTextColor(WHITE);
        M5.Display.printf("Check connection\n");
        M5.Display.printf("Addr: 0x%02X\n", ATOM_JOYSTICK_ADDR);
    }
}

void display_raw_values() {
    M5.Display.setTextColor(ORANGE);
    M5.Display.println("Raw Values");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("----------");
    
    if (joystick_data.valid) {
        M5.Display.printf("X_8bit : %3d\n", joystick_data.x_8bit);
        M5.Display.printf("Y_8bit : %3d\n", joystick_data.y_8bit);
        M5.Display.printf("X_12bit: %4d\n", joystick_data.x_12bit);
        M5.Display.printf("Y_12bit: %4d\n", joystick_data.y_12bit);
        M5.Display.printf("Button : %d\n", joystick_data.button_pressed ? 1 : 0);
        
        // 中心からの距離
        int center_x = abs(joystick_data.x_8bit - 128);
        int center_y = abs(joystick_data.y_8bit - 128);
        int distance = sqrt(center_x * center_x + center_y * center_y);
        M5.Display.printf("Distance: %d\n", distance);
    } else {
        M5.Display.setTextColor(RED);
        M5.Display.println("No valid data");
    }
}

void display_i2c_status() {
    M5.Display.setTextColor(MAGENTA);
    M5.Display.println("I2C Status");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("----------");
    
    M5.Display.printf("SDA: GPIO%d\n", I2C_SDA_PIN);
    M5.Display.printf("SCL: GPIO%d\n", I2C_SCL_PIN);
    M5.Display.printf("Addr: 0x%02X\n", ATOM_JOYSTICK_ADDR);
    M5.Display.printf("Freq: %dkHz\n", I2C_FREQUENCY / 1000);
    
    // 接続状態
    Wire.beginTransmission(ATOM_JOYSTICK_ADDR);
    uint8_t error = Wire.endTransmission();
    
    M5.Display.setTextColor(error == 0 ? GREEN : RED);
    M5.Display.printf("Status: %s\n", error == 0 ? "CONNECTED" : "ERROR");
    M5.Display.setTextColor(WHITE);
    M5.Display.printf("Error: %d\n", error);
    
    if (joystick_data.valid) {
        M5.Display.setTextColor(GREEN);
        M5.Display.println("Data: VALID");
    } else {
        M5.Display.setTextColor(RED);
        M5.Display.println("Data: INVALID");
    }
}

void draw_bar(int value, int max_value) {
    int bar_width = map(value, 0, max_value, 0, 50);
    M5.Display.print("[");
    for (int i = 0; i < 6; i++) {
        if (i < bar_width / 8) {
            M5.Display.print("=");
        } else {
            M5.Display.print(" ");
        }
    }
    M5.Display.print("]");
}