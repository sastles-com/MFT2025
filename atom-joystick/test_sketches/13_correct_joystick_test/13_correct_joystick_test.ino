/**
 * @file 13_correct_joystick_test.ino
 * @brief M5Stack Atom-JoyStick 正しいI2C仕様テスト
 * @description I2Cアドレス0x59・レジスタアクセスによるJoystick制御
 * 
 * Phase 4.6: 正しいI2C仕様準拠
 * - I2Cアドレス: 0x59
 * - レジスタベースアクセス
 * - LEFT/RIGHT スティック対応
 * 
 * @target M5Stack Atom-JoyStick (ESP32-S3)
 * @specification 正確なハードウェア仕様準拠
 */

#include <M5Unified.h>
#include <Wire.h>

// 正しいI2C仕様（atoms3joy.h準拠）
#define I2C_ADDRESS                (0x59)
#define LEFT_STICK_X_ADDRESS       (0x00)
#define LEFT_STICK_Y_ADDRESS       (0x02)
#define RIGHT_STICK_X_ADDRESS      (0x20)
#define RIGHT_STICK_Y_ADDRESS      (0x22)
#define LEFT_STICK_BUTTON_ADDRESS  (0x70)
#define RIGHT_STICK_BUTTON_ADDRESS (0x71)
#define LEFT_BUTTON_ADDRESS        (0x72)
#define RIGHT_BUTTON_ADDRESS       (0x73)
#define BATTERY_VOLTAGE1_ADDRESS   (0x60)
#define BATTERY_VOLTAGE2_ADDRESS   (0x62)

// 正しいI2Cピン設定（atoms3joy.h準拠）
const int I2C_SDA_PIN = 38;  // 公式仕様
const int I2C_SCL_PIN = 39;  // 公式仕様
const uint32_t I2C_FREQUENCY = 100000U;  // 100kHz

// テスト用追加ピン設定（バックアップ）
struct I2CPinConfig {
    int sda_pin;
    int scl_pin;
    const char* name;
};

const I2CPinConfig pin_configs[] = {
    {38, 39, "Official"},
    {2, 1, "Grove-A"},
    {32, 33, "Grove-B"},
    {26, 25, "Grove-C"},
    {21, 22, "Standard"}
};

const int PIN_CONFIG_COUNT = sizeof(pin_configs) / sizeof(pin_configs[0]);

// Joystickデータ
struct JoystickData {
    uint16_t left_x;
    uint16_t left_y;
    uint16_t right_x;
    uint16_t right_y;
    bool left_stick_button;
    bool right_stick_button;
    bool left_button;
    bool right_button;
    uint16_t battery_voltage1;
    uint16_t battery_voltage2;
    bool valid;
    unsigned long timestamp;
};

JoystickData joystick_data;
int current_pin_config = 0;
unsigned long last_update = 0;
unsigned long last_pin_change = 0;
const int UPDATE_INTERVAL = 100;
const int PIN_CHANGE_INTERVAL = 5000;  // 5秒間隔で自動切り替え

void setup() {
    // M5Unified初期化
    auto cfg = M5.config();
    cfg.external_spk = false;
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("============================================");
    Serial.println("M5Stack Atom-JoyStick 正しいI2C仕様テスト");
    Serial.println("============================================");
    Serial.printf("I2Cアドレス: 0x%02X\n", I2C_ADDRESS);
    Serial.println("レジスタマップ:");
    Serial.printf("  LEFT_X : 0x%02X\n", LEFT_STICK_X_ADDRESS);
    Serial.printf("  LEFT_Y : 0x%02X\n", LEFT_STICK_Y_ADDRESS);
    Serial.printf("  RIGHT_X: 0x%02X\n", RIGHT_STICK_X_ADDRESS);
    Serial.println();
    
    // LCD初期化
    M5.Display.clear(BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setRotation(0);
    
    // 初期データ
    joystick_data.valid = false;
    
    display_welcome_screen();
    delay(3000);
    
    // I2C初期化（公式ピン設定から開始）
    current_pin_config = 0;  // Official設定
    initialize_i2c();
    
    // 初期接続テスト
    test_i2c_connection();
    
    Serial.println("✅ 正しいI2C仕様テスト初期化完了");
    Serial.println("操作:");
    Serial.println("  - Joystickを動かして値変化確認");
    Serial.println("  - ボタンAで手動ピン切り替え");
    Serial.println("  - 自動ピン切り替え: 5秒間隔（公式設定で接続失敗時のみ）");
    
    last_update = millis();
    last_pin_change = millis();
}

void loop() {
    M5.update();
    
    // ボタン入力処理
    if (M5.BtnA.wasPressed()) {
        change_pin_config();
    }
    
    // 自動ピン切り替え（公式設定で接続失敗時のみ）
    if (current_pin_config == 0) {
        // 公式設定でI2C接続確認
        Wire.beginTransmission(I2C_ADDRESS);
        uint8_t error = Wire.endTransmission();
        if (error != 0 && millis() - last_pin_change >= PIN_CHANGE_INTERVAL) {
            Serial.println("公式設定で接続失敗、代替ピンを試行");
            change_pin_config();
            last_pin_change = millis();
        }
    } else {
        // 代替設定で定期切り替え
        if (millis() - last_pin_change >= PIN_CHANGE_INTERVAL) {
            change_pin_config();
            last_pin_change = millis();
        }
    }
    
    // 定期データ読み取り
    if (millis() - last_update >= UPDATE_INTERVAL) {
        read_joystick_registers();
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
    M5.Display.println("Correct");
    M5.Display.println("I2C Test");
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(WHITE);
    M5.Display.println("");
    M5.Display.printf("Addr: 0x%02X\n", I2C_ADDRESS);
    M5.Display.println("Register based");
}

void initialize_i2c() {
    const I2CPinConfig* config = &pin_configs[current_pin_config];
    
    Wire.end();
    delay(100);
    
    // M5Stack Atom-JoyStick公式仕様準拠
    if (current_pin_config == 0) {
        // 公式ピン設定使用
        Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
    } else {
        // テスト用代替ピン
        Wire.begin(config->sda_pin, config->scl_pin, I2C_FREQUENCY);
    }
    delay(200);
    
    Serial.printf("I2C初期化: %s (SDA=%d, SCL=%d)\n", 
                  config->name, config->sda_pin, config->scl_pin);
}

void change_pin_config() {
    current_pin_config = (current_pin_config + 1) % PIN_CONFIG_COUNT;
    Serial.printf("ピン設定変更: %d/%d\n", current_pin_config + 1, PIN_CONFIG_COUNT);
    initialize_i2c();
    
    // 新しい設定でテスト
    test_i2c_connection();
}

void test_i2c_connection() {
    Wire.beginTransmission(I2C_ADDRESS);
    uint8_t error = Wire.endTransmission();
    
    const I2CPinConfig* config = &pin_configs[current_pin_config];
    
    if (error == 0) {
        Serial.printf("✅ I2C接続成功: %s\n", config->name);
        M5.Speaker.tone(1000, 100);  // 成功音
    } else {
        Serial.printf("❌ I2C接続失敗: %s (エラー: %d)\n", config->name, error);
    }
}

void read_joystick_registers() {
    bool success = true;
    
    // LEFT STICK X
    joystick_data.left_x = read_register_16bit(LEFT_STICK_X_ADDRESS);
    if (joystick_data.left_x == 0xFFFF) success = false;
    
    // LEFT STICK Y
    joystick_data.left_y = read_register_16bit(LEFT_STICK_Y_ADDRESS);
    if (joystick_data.left_y == 0xFFFF) success = false;
    
    // RIGHT STICK X
    joystick_data.right_x = read_register_16bit(RIGHT_STICK_X_ADDRESS);
    if (joystick_data.right_x == 0xFFFF) success = false;
    
    // RIGHT STICK Y
    joystick_data.right_y = read_register_16bit(RIGHT_STICK_Y_ADDRESS);
    if (joystick_data.right_y == 0xFFFF) success = false;
    
    // ボタン読み取り
    joystick_data.left_stick_button = read_register_8bit(LEFT_STICK_BUTTON_ADDRESS) > 0;
    joystick_data.right_stick_button = read_register_8bit(RIGHT_STICK_BUTTON_ADDRESS) > 0;
    joystick_data.left_button = read_register_8bit(LEFT_BUTTON_ADDRESS) > 0;
    joystick_data.right_button = read_register_8bit(RIGHT_BUTTON_ADDRESS) > 0;
    
    // バッテリー電圧読み取り
    joystick_data.battery_voltage1 = read_register_16bit(BATTERY_VOLTAGE1_ADDRESS);
    joystick_data.battery_voltage2 = read_register_16bit(BATTERY_VOLTAGE2_ADDRESS);
    
    joystick_data.valid = success;
    joystick_data.timestamp = millis();
    
    // データログ出力（成功時のみ）
    if (success) {
        static int log_counter = 0;
        if (++log_counter >= 10) {
            Serial.printf("🕹️ L_X=%4d L_Y=%4d R_X=%4d R_Y=%4d\n", 
                          joystick_data.left_x, joystick_data.left_y,
                          joystick_data.right_x, joystick_data.right_y);
            log_counter = 0;
        }
    }
}

uint16_t read_register_16bit(uint8_t reg_addr) {
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(reg_addr);
    uint8_t result = Wire.endTransmission(false);  // Repeated start
    
    if (result != 0) {
        return 0xFFFF;  // エラー値
    }
    
    Wire.requestFrom(I2C_ADDRESS, (uint8_t)2);
    
    if (Wire.available() >= 2) {
        uint8_t low_byte = Wire.read();
        uint8_t high_byte = Wire.read();
        return (high_byte << 8) | low_byte;
    }
    
    return 0xFFFF;  // エラー値
}

uint8_t read_register_8bit(uint8_t reg_addr) {
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(reg_addr);
    uint8_t result = Wire.endTransmission(false);  // Repeated start
    
    if (result != 0) {
        return 0xFF;  // エラー値
    }
    
    Wire.requestFrom(I2C_ADDRESS, (uint8_t)1);
    
    if (Wire.available() >= 1) {
        return Wire.read();
    }
    
    return 0xFF;  // エラー値
}

void update_display() {
    M5.Display.clear(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(1);
    
    // ヘッダー
    M5.Display.setTextColor(CYAN);
    M5.Display.println("Correct I2C Test");
    M5.Display.setTextColor(WHITE);
    M5.Display.println("---------------");
    
    // ピン設定表示
    const I2CPinConfig* config = &pin_configs[current_pin_config];
    M5.Display.printf("Pin: %s (%d/%d)\n", config->name, 
                      current_pin_config + 1, PIN_CONFIG_COUNT);
    M5.Display.printf("SDA:%d SCL:%d\n", config->sda_pin, config->scl_pin);
    
    // I2C接続状態
    Wire.beginTransmission(I2C_ADDRESS);
    uint8_t error = Wire.endTransmission();
    
    M5.Display.setTextColor(error == 0 ? GREEN : RED);
    M5.Display.printf("I2C: %s\n", error == 0 ? "CONNECTED" : "ERROR");
    M5.Display.setTextColor(WHITE);
    
    if (joystick_data.valid) {
        // Joystickデータ表示
        M5.Display.println("");
        M5.Display.setTextColor(GREEN);
        M5.Display.println("LEFT STICK:");
        M5.Display.setTextColor(WHITE);
        M5.Display.printf("  X: %4d\n", joystick_data.left_x);
        M5.Display.printf("  Y: %4d\n", joystick_data.left_y);
        
        M5.Display.setTextColor(YELLOW);
        M5.Display.println("RIGHT STICK:");
        M5.Display.setTextColor(WHITE);
        M5.Display.printf("  X: %4d\n", joystick_data.right_x);
        M5.Display.printf("  Y: %4d\n", joystick_data.right_y);
        
    } else {
        M5.Display.setTextColor(RED);
        M5.Display.println("");
        M5.Display.println("No valid data");
        M5.Display.setTextColor(WHITE);
        M5.Display.printf("Addr: 0x%02X\n", I2C_ADDRESS);
    }
    
    // 動作確認ドット
    static int dot_counter = 0;
    dot_counter++;
    M5.Display.setCursor(120, 120);
    M5.Display.setTextColor(dot_counter % 10 < 5 ? BLUE : BLACK);
    M5.Display.print("*");
}