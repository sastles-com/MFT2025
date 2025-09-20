/**
 * @file 12_i2c_scanner.ino
 * @brief M5Stack Atom-JoyStick I2Cスキャナー
 * @description 複数のピン組み合わせでI2Cデバイスを検索
 * 
 * Phase 4.5: I2Cデバイス発見
 * - 複数のSDA/SCLピン組み合わせテスト
 * - 全I2Cアドレススキャン
 * - Joystickデバイス特定
 * 
 * @target M5Stack Atom-JoyStick (ESP32-S3)
 */

#include <M5Unified.h>
#include <Wire.h>

// テスト対象ピン組み合わせ
struct I2CPinConfig {
    int sda_pin;
    int scl_pin;
    const char* description;
};

const I2CPinConfig pin_configs[] = {
    {38, 39, "公式仕様(GitHub)"},
    {2, 1, "Grove端子A"},
    {32, 33, "Grove端子B"},
    {26, 25, "Grove端子C"},
    {21, 22, "標準I2C"},
    {18, 19, "代替I2C"},
    {5, 4, "SPI代替"},
    {16, 17, "追加候補"},
    {8, 9, "追加候補2"},
    {13, 14, "追加候補3"}
};

const int PIN_CONFIG_COUNT = sizeof(pin_configs) / sizeof(pin_configs[0]);
const uint32_t I2C_FREQUENCY = 100000U;  // 100kHz（互換性優先）

int current_config = 0;
unsigned long last_scan = 0;
const int SCAN_INTERVAL = 3000;  // 3秒間隔

void setup() {
    // M5Unified初期化
    auto cfg = M5.config();
    cfg.external_spk = false;
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("============================================");
    Serial.println("M5Stack Atom-JoyStick I2Cスキャナー");
    Serial.println("============================================");
    Serial.printf("テスト対象: %d種類のピン組み合わせ\n", PIN_CONFIG_COUNT);
    Serial.println("Joystickデバイス自動検出システム");
    Serial.println();
    
    // LCD初期化
    M5.Display.clear(BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setRotation(0);
    
    display_welcome_screen();
    delay(3000);
    
    last_scan = millis();
}

void loop() {
    M5.update();
    
    // ボタン入力処理
    if (M5.BtnA.wasPressed()) {
        current_config = (current_config + 1) % PIN_CONFIG_COUNT;
        Serial.printf("手動切り替え: 設定%d\n", current_config);
        scan_current_config();
    }
    
    // 自動スキャン
    if (millis() - last_scan >= SCAN_INTERVAL) {
        scan_current_config();
        current_config = (current_config + 1) % PIN_CONFIG_COUNT;
        last_scan = millis();
    }
    
    delay(100);
}

void display_welcome_screen() {
    M5.Display.clear(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextColor(GREEN);
    M5.Display.setTextSize(2);
    M5.Display.println("I2C");
    M5.Display.println("Scanner");
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(WHITE);
    M5.Display.println("");
    M5.Display.printf("%d pin configs\n", PIN_CONFIG_COUNT);
    M5.Display.println("Auto scanning...");
}

void scan_current_config() {
    const I2CPinConfig* config = &pin_configs[current_config];
    
    Serial.println("--------------------------------------------");
    Serial.printf("設定 %d: %s\n", current_config, config->description);
    Serial.printf("SDA: GPIO%d, SCL: GPIO%d\n", config->sda_pin, config->scl_pin);
    Serial.println("--------------------------------------------");
    
    // LCD表示更新
    update_lcd_display(config);
    
    // I2C初期化
    Wire.end();  // 前の設定をクリア
    delay(100);
    Wire.begin(config->sda_pin, config->scl_pin, I2C_FREQUENCY);
    delay(200);
    
    // アドレススキャン
    int device_count = 0;
    Serial.println("I2Cアドレススキャン開始...");
    
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            device_count++;
            Serial.printf("✅ デバイス発見: 0x%02X (%d)\n", addr, addr);
            
            // 特定のアドレスを詳細チェック
            if (addr == 0x38) {
                Serial.println("   🎯 疑似Joystickアドレス！");
                test_joystick_communication(addr);
            }
            
            // その他の既知デバイス
            identify_device(addr);
            
        } else if (error == 4) {
            Serial.printf("❌ 不明エラー: 0x%02X\n", addr);
        }
        
        delay(5);  // 安定性のため
    }
    
    if (device_count == 0) {
        Serial.println("❌ I2Cデバイスが見つかりません");
    } else {
        Serial.printf("✅ 合計 %d個のデバイスを発見\n", device_count);
    }
    
    Serial.println();
}

void test_joystick_communication(uint8_t addr) {
    Serial.printf("   Joystick通信テスト (0x%02X):\n", addr);
    
    // 3バイト読み取り試行
    Wire.requestFrom(addr, (uint8_t)3);
    
    if (Wire.available() >= 3) {
        uint8_t x_val = Wire.read();
        uint8_t y_val = Wire.read();
        uint8_t btn_val = Wire.read();
        
        Serial.printf("   📊 X=%d, Y=%d, Button=%d\n", x_val, y_val, btn_val);
        
        // 値の妥当性チェック
        if (x_val <= 255 && y_val <= 255) {
            Serial.println("   ✅ Joystickデータ形式正常！");
        }
    } else {
        Serial.println("   ❌ データ読み取り失敗");
    }
}

void identify_device(uint8_t addr) {
    switch (addr) {
        case 0x38:
            Serial.println("   → 候補: M5Stack Joystick");
            break;
        case 0x68:
            Serial.println("   → 候補: MPU6050/6500 IMU");
            break;
        case 0x28:
        case 0x29:
            Serial.println("   → 候補: BNO055 IMU");
            break;
        case 0x3C:
        case 0x3D:
            Serial.println("   → 候補: OLED Display");
            break;
        case 0x40:
            Serial.println("   → 候補: PCA9685 PWM");
            break;
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4B:
            Serial.println("   → 候補: ADS1115 ADC");
            break;
        default:
            Serial.println("   → 不明デバイス");
            break;
    }
}

void update_lcd_display(const I2CPinConfig* config) {
    M5.Display.clear(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(1);
    
    // ヘッダー
    M5.Display.setTextColor(CYAN);
    M5.Display.println("I2C Scanner");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("------------");
    
    // 現在の設定
    M5.Display.printf("Config: %d/%d\n", current_config + 1, PIN_CONFIG_COUNT);
    M5.Display.printf("SDA: GPIO%d\n", config->sda_pin);
    M5.Display.printf("SCL: GPIO%d\n", config->scl_pin);
    M5.Display.println("");
    
    // 説明
    M5.Display.setTextColor(YELLOW);
    String desc = String(config->description);
    if (desc.length() > 12) {
        desc = desc.substring(0, 12);
    }
    M5.Display.printf("%s\n", desc.c_str());
    
    // 操作説明
    M5.Display.setTextColor(GREEN);
    M5.Display.println("");
    M5.Display.println("BtnA: Manual");
    M5.Display.println("Auto: 3sec");
    
    // スキャン状態表示
    M5.Display.setTextColor(WHITE);
    M5.Display.println("");
    M5.Display.println("Scanning...");
    
    // 動作確認ドット
    static int dot_counter = 0;
    dot_counter++;
    M5.Display.setCursor(120, 120);
    M5.Display.setTextColor(dot_counter % 10 < 5 ? BLUE : BLACK);
    M5.Display.print("*");
}