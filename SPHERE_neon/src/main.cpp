#include <M5Unified.h>
#include <Wire.h>
#include <SPIFFS.h>
// #include "led/SphereStripController.h"  // 4ストリップテストのため一時的にコメントアウト
#include "LED.h"  // 4ストリップ対応LEDクラス
// #include "imu/SphereIMUManager.h"

// BMI270用の定義
#define IMU_SENSOR_BMI270
#include "IMU.h"
// グローバル変数
// SphereStripController ledController;  // 一時的にコメントアウト
neon::LED led4Strip;  // 4ストリップ対応LED
neon::IMU sensor;



// SPIFFSファイルシステムテスト
void testSPIFFS() {
  Serial.println("[SPIFFS] ファイルシステムテスト開始");
  
  if (!SPIFFS.begin(true)) {
    Serial.println("[SPIFFS] 初期化失敗");
    return;
  }
  
  Serial.println("[SPIFFS] 初期化成功");
  
  // ファイル一覧表示
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  
  while (file) {
    Serial.printf("[SPIFFS] ファイル: %s (サイズ: %u バイト)\n", 
                 file.name(), file.size());
    file = root.openNextFile();
  }
  
  Serial.println("[SPIFFS] ファイルシステムテスト完了");
}

// LED制御システムテスト
/*
void testLEDController() {
  Serial.println("[LED] SphereStripController テスト開始");
  
  // 初期化テスト
  if (ledController.initialize(LED_PIN, NUM_LEDS)) {
    Serial.printf("[LED] 初期化成功 - %d LEDs on GPIO%d\n", NUM_LEDS, LED_PIN);
  } else {
    Serial.println("[LED] 初期化失敗");
    return;
  }
  
  // 基本色テスト
  Serial.println("[LED] 基本色テスト実行中...");
  
  // 最初の10個のLEDを赤色に設定
  for (int i = 0; i < 10 && i < NUM_LEDS; i++) {
    ledController.setLedColor(i, CRGB::Red);
  }
  ledController.show();
  delay(1000);
  
  // 次の10個を緑色に設定
  for (int i = 10; i < 20 && i < NUM_LEDS; i++) {
    ledController.setLedColor(i, CRGB::Green);
  }
  ledController.show();
  delay(1000);
  
  // 次の10個を青色に設定
  for (int i = 20; i < 30 && i < NUM_LEDS; i++) {
    ledController.setLedColor(i, CRGB::Blue);
  }
  ledController.show();
  delay(1000);
  
  // 全LED消去
  ledController.clear();
  ledController.show();
  
  // 明度テスト
  Serial.println("[LED] 明度テスト実行中...");
  ledController.setBrightness(64); // 25%明度
  
  for (int i = 0; i < 5 && i < NUM_LEDS; i++) {
    ledController.setLedColor(i, CRGB::White);
  }
  ledController.show();
  delay(1000);
  
  ledController.setBrightness(255); // 100%明度に戻す
  ledController.clear();
  ledController.show();
  
  Serial.println("[LED] SphereStripController テスト完了");
}
*/

// 4ストリップLEDテスト関数
void test4StripLED() {
  Serial.println("[LED] 4ストリップLED テスト開始");
  
  // 4ストリップLED初期化
  led4Strip.init(&Wire, &Serial);
  
  // 各ストリップを個別にテスト
  Serial.println("[LED] ストリップ1テスト (赤)");
  led4Strip.testStrip(0, ::CRGB::Red, 1000);
  
  Serial.println("[LED] ストリップ2テスト (緑)");
  led4Strip.testStrip(1, ::CRGB::Green, 1000);
  
  Serial.println("[LED] ストリップ3テスト (青)");
  led4Strip.testStrip(2, ::CRGB::Blue, 1000);
  
  Serial.println("[LED] ストリップ4テスト (白)");
  led4Strip.testStrip(3, ::CRGB::White, 1000);
  
  // 全LEDを使った論理インデックステスト
  Serial.println("[LED] 論理インデックステスト (0-799)");
  led4Strip.black();
  
  // 最初の100LED（ストリップ1の半分）を赤に
  for(int i = 0; i < 100; i++) {
    led4Strip.setPixel(i, ::CRGB::Red);
  }
  
  // 200-299LED（ストリップ2の最初の100LED）を緑に
  for(int i = 200; i < 300; i++) {
    led4Strip.setPixel(i, ::CRGB::Green);
  }
  
  // 400-499LED（ストリップ3の最初の100LED）を青に
  for(int i = 400; i < 500; i++) {
    led4Strip.setPixel(i, ::CRGB::Blue);
  }
  
  // 600-699LED（ストリップ4の最初の100LED）を紫に
  for(int i = 600; i < 700; i++) {
    led4Strip.setPixel(i, ::CRGB(128, 0, 128)); // 紫色
  }
  
  led4Strip.update();
  delay(3000);
  
  led4Strip.black();
  
  Serial.println("[LED] 4ストリップLED テスト完了");
}



// IMU初期化テスト関数
void testIMUInit() {
  Serial.println("[IMU] sensor.init テスト開始");
  
  // BMI270用初期化（wire, serialパラメータは使用されない）
  sensor.init(&Wire, &Serial);
  
  Serial.println("[IMU] sensor.init 完了");
  Serial.println("[IMU] BMI270センサーがSphereIMUManager経由で初期化されました");
}

// I2Cスキャン関数
void scanInternalI2C(const char* label) {
  Serial.printf("[%s] Scanning I2C devices...\n", label);
  int deviceCount = 0;
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[%s] I2C device found at address 0x%02X\n", label, address);
      deviceCount++;
    }
  }
  if (deviceCount == 0) {
    Serial.printf("[%s] No I2C devices found\n", label);
  } else {
    Serial.printf("[%s] Found %d I2C devices\n", label, deviceCount);
  }
}

void setup() {
    Serial.begin(115200);
    while (!Serial)
        delay(10);
  
    Serial.println("=== SPHERE_neon BMI270 IMU sensor.init テスト ===");
    
    // M5Unified初期化
    auto cfg = M5.config();
    cfg.external_spk = false;
    cfg.output_power = true;
    cfg.internal_imu = true;
    cfg.internal_rtc = true;
    cfg.fallback_board = m5::board_t::board_M5AtomS3R;
    M5.begin(cfg);
    
    Serial.println("[M5] M5Unified初期化完了");
    
    // I2C初期化
    Wire.begin(SDA, SCL);
    Wire.setPins(SDA, SCL);
    

    
    // I2Cデバイススキャン
    scanInternalI2C("Internal");    
    // IMU sensor.init テスト
    testIMUInit();
    // SPIFFS テスト
    testSPIFFS();
    
    // 4ストリップLED テスト
    test4StripLED();
    
    Serial.println("=== Setup完了 - 全テスト完了 ===");
}

void loop() {
    M5.update();
    
    // M5Stack内蔵ボタンテスト
    if (M5.BtnA.wasPressed()) {
        Serial.println("[Button] M5Stack BtnA pressed");
        
        // IMU sensor.update() テスト
        Serial.println("[IMU] sensor.update() テスト開始");
        sensor.update();
        
        // BNO055と同じ方法でベクトル回転テスト
        auto rotated_vector = sensor.rotate(imu::Vector<3>(0, 0, 1));
        Serial.printf("[IMU] Rotated Vector: x=%.3f, y=%.3f, z=%.3f\n", 
                     rotated_vector.x(), rotated_vector.y(), rotated_vector.z());
    }
    
    sensor.update();
    // image.update();
    // // 定期的なIMU更新とデータ表示
    // static unsigned long lastImuUpdate = 0;
    // if (millis() - lastImuUpdate > 3000) { // 3秒に1回
    //     sensor.update();
        
    //     // BNO055互換のベクトル回転テスト
    //     auto test_vector = sensor.rotate(imu::Vector<3>(1, 0, 0));
    //     Serial.printf("[IMU] Test rotation (1,0,0) -> (%.3f,%.3f,%.3f)\n", 
    //                  test_vector.x(), test_vector.y(), test_vector.z());
        
    //     lastImuUpdate = millis();
    // }
    
    delay(10);
}
