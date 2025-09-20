/**
 * @file buzzer_unit_test.ino
 * @brief JoystickBuzzer単体テストスケッチ
 * @description ブザー機能のみを単独でテストするスケッチ
 */

#include <M5Unified.h>

// ブザーシステム（単体テスト用）
#include "../14_udp_joystick_integration/JoystickBuzzer.h"

// グローバル変数
JoystickBuzzer buzzer;
int test_phase = 0;
unsigned long last_test_time = 0;
const int TEST_INTERVAL = 2000;  // 2秒間隔

void setup() {
  // M5Unified初期化
  auto cfg = M5.config();
  cfg.external_spk = false;
  M5.begin(cfg);
  
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("============================================");
  Serial.println("JoystickBuzzer 単体テストスケッチ");
  Serial.println("============================================");
  Serial.println("テスト内容:");
  Serial.println("  Phase 0: ブザー初期化テスト");
  Serial.println("  Phase 1: 基本ビープ音テスト");
  Serial.println("  Phase 2: 起動音テスト");
  Serial.println("  Phase 3: WiFi接続音テスト");
  Serial.println("  Phase 4: エラー音テスト");
  Serial.println("  Phase 5: 完了音テスト");
  Serial.println("  Phase 6: オープニングメロディテスト");
  Serial.println("  Phase 7: 音量・有効無効テスト");
  Serial.println("  Phase 8: 統計表示テスト");
  Serial.println();
  
  // LCD初期化
  M5.Display.clear(BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setRotation(0);
  M5.Display.setCursor(0, 0);
  M5.Display.setTextColor(WHITE);
  M5.Display.println("Buzzer Unit Test");
  M5.Display.println("");
  M5.Display.println("Phase 0: Init");
  
  // ブザーシステム初期化
  Serial.println("Phase 0: ブザーシステム初期化テスト");
  if (!buzzer.begin()) {
    Serial.println("❌ ブザーシステム初期化失敗");
    M5.Display.println("Init FAILED");
  } else {
    Serial.println("✅ ブザーシステム初期化成功");
    M5.Display.println("Init SUCCESS");
  }
  
  last_test_time = millis();
  test_phase = 1;
}

void loop() {
  M5.update();
  
  // ボタンAでテストスキップ
  if (M5.BtnA.wasPressed()) {
    test_phase++;
    if (test_phase > 8) {
      test_phase = 1;  // テスト最初に戻る
    }
    last_test_time = millis() - TEST_INTERVAL; // 即座に実行
    Serial.printf("ボタン押下: Phase %d にスキップ\n", test_phase);
  }
  
  // テスト実行
  if (millis() - last_test_time >= TEST_INTERVAL) {
    executeTestPhase(test_phase);
    test_phase++;
    if (test_phase > 8) {
      test_phase = 1;  // テスト繰り返し
    }
    last_test_time = millis();
  }
  
  delay(50);
}

void executeTestPhase(int phase) {
  // LCD表示更新
  M5.Display.clear(BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.setTextColor(WHITE);
  M5.Display.println("Buzzer Unit Test");
  M5.Display.println("");
  M5.Display.printf("Phase %d\n", phase);
  
  Serial.printf("\n========== Phase %d ==========\n", phase);
  
  switch (phase) {
    case 1:
      Serial.println("Phase 1: 基本ビープ音テスト");
      M5.Display.println("Basic Beep");
      buzzer.beep();
      Serial.println("基本ビープ音再生完了");
      break;
      
    case 2:
      Serial.println("Phase 2: 起動音テスト");
      M5.Display.println("Startup Tone");
      buzzer.start_tone();
      Serial.println("起動音再生完了");
      break;
      
    case 3:
      Serial.println("Phase 3: WiFi接続音テスト");
      M5.Display.println("WiFi Connect");
      buzzer.wifi_connected_tone();
      Serial.println("WiFi接続音再生完了");
      break;
      
    case 4:
      Serial.println("Phase 4: エラー音テスト");
      M5.Display.println("Error Tone");
      buzzer.error_tone();
      Serial.println("エラー音再生完了");
      break;
      
    case 5:
      Serial.println("Phase 5: 完了音テスト");
      M5.Display.println("Completion");
      buzzer.completion_tone();
      Serial.println("完了音再生完了");
      break;
      
    case 6:
      Serial.println("Phase 6: オープニングメロディテスト");
      M5.Display.println("Opening Melody");
      buzzer.opening_startup_melody();
      delay(500);
      buzzer.opening_completion_melody();
      Serial.println("オープニングメロディ再生完了");
      break;
      
    case 7:
      Serial.println("Phase 7: 音量・有効無効テスト");
      M5.Display.println("Volume Test");
      
      // 音量50%テスト
      buzzer.setVolume(64);
      Serial.println("音量50%でビープ");
      buzzer.beep();
      delay(500);
      
      // 音量100%テスト
      buzzer.setVolume(255);
      Serial.println("音量100%でビープ");
      buzzer.beep();
      delay(500);
      
      // 無効化テスト
      buzzer.setEnabled(false);
      Serial.println("ブザー無効化してビープ（音なし）");
      buzzer.beep();
      delay(500);
      
      // 有効化テスト
      buzzer.setEnabled(true);
      buzzer.setVolume(127);  // 元に戻す
      Serial.println("ブザー有効化してビープ");
      buzzer.beep();
      
      Serial.println("音量・有効無効テスト完了");
      break;
      
    case 8:
      Serial.println("Phase 8: 統計表示テスト");
      M5.Display.println("Statistics");
      buzzer.printStats();
      Serial.println("統計表示完了");
      break;
  }
  
  Serial.printf("Phase %d 実行完了\n", phase);
}