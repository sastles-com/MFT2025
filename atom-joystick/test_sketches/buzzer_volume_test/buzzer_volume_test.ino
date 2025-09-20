/**
 * @file buzzer_volume_test.ino
 * @brief M5Stack Atom-JoyStick ブザー音量設定テスト
 * @description config.json経由での音量制御機能検証
 * 
 * テスト内容:
 * 1. config.json音量値読み込み確認
 * 2. 音量レベル別ブザーテスト（25%、50%、75%、100%）
 * 3. リアルタイム音量変更テスト
 * 4. 音量設定保存・復旧テスト
 * 
 * @target M5Stack Atom-JoyStick (ESP32-S3)
 * @author Claude Code Assistant
 * @date 2025年9月4日
 */

#include <M5Unified.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "JoystickBuzzer.h"
#include "JoystickConfig.h"

// ========== テスト設定 ==========
JoystickBuzzer buzzer;
JoystickConfig config;

// テスト用音量レベル（0-255）
const int test_volumes[] = {0, 64, 127, 191, 255};  // 0%, 25%, 50%, 75%, 100%
const char* volume_names[] = {"0%(無音)", "25%", "50%", "75%", "100%"};
const int test_volume_count = 5;

int current_test_volume = 2;  // 開始は50%（127）
bool test_running = false;

// ========== 基本初期化・設定 ==========

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // M5Unified初期化
  auto cfg = M5.config();
  cfg.output_power = true;
  cfg.internal_imu = false;
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  M5.begin(cfg);
  
  // LCD初期化
  M5.Display.setRotation(2);
  M5.Display.setTextSize(2);
  M5.Display.clear(BLACK);
  M5.Display.setTextColor(WHITE);
  M5.Display.drawString("Buzzer Volume Test", 10, 10);
  M5.Display.drawString("Loading...", 10, 40);
  
  Serial.println();
  Serial.println("========================================");
  Serial.println("🎵 M5Stack Atom-JoyStick");
  Serial.println("   ブザー音量制御テスト");
  Serial.println("========================================");
  
  // LittleFS初期化
  if (!LittleFS.begin(true)) {
    Serial.println("❌ LittleFS初期化失敗");
    showError("LittleFS Error");
    while (true) { delay(1000); }
  }
  
  // 設定読み込み
  if (!config.begin()) {
    Serial.println("❌ 設定読み込み失敗");
    showError("Config Load Error");
    while (true) { delay(1000); }
  }
  
  // ブザー初期化
  if (!buzzer.begin()) {
    Serial.println("❌ ブザー初期化失敗");
    showError("Buzzer Init Error");
    while (true) { delay(1000); }
  }
  
  // config.jsonから音量を読み込んでブザーに設定
  SystemConfig system_config = config.getSystemConfig();
  buzzer.setEnabled(system_config.buzzer_enabled);
  buzzer.setVolume(system_config.buzzer_volume);
  
  Serial.println("✅ 初期化完了");
  Serial.printf("📋 設定読み込み完了: 音量=%d, 有効=%s\n", 
    system_config.buzzer_volume, system_config.buzzer_enabled ? "true" : "false");
  
  // テスト開始
  startVolumeTest();
}

void loop() {
  M5.update();
  
  // ボタンA: 次の音量レベルテスト
  if (M5.BtnA.wasPressed()) {
    nextVolumeTest();
  }
  
  // ボタンB: config.json音量値変更テスト
  if (M5.BtnB.wasPressed()) {
    testConfigVolumeChange();
  }
  
  delay(50);
}

// ========== 音量レベルテスト ==========

void startVolumeTest() {
  Serial.println();
  Serial.println("🎵 音量レベルテスト開始");
  Serial.println("   [ボタンA]: 次の音量レベル");
  Serial.println("   [ボタンB]: config.json変更テスト");
  Serial.println();
  
  showVolumeTestUI();
  playCurrentVolumeTest();
}

void nextVolumeTest() {
  current_test_volume = (current_test_volume + 1) % test_volume_count;
  showVolumeTestUI();
  playCurrentVolumeTest();
}

void playCurrentVolumeTest() {
  int volume = test_volumes[current_test_volume];
  const char* name = volume_names[current_test_volume];
  
  Serial.printf("🎵 音量テスト: %s (PWM値: %d)\n", name, volume);
  
  // 音量設定
  buzzer.setVolume(volume);
  
  // テスト音再生
  buzzer.start_tone();
  delay(1000);
  buzzer.beep();
  delay(500);
  buzzer.good_voltage_tone();
  
  Serial.printf("✅ 音量テスト完了: %s\n", name);
}

void showVolumeTestUI() {
  M5.Display.clear(BLACK);
  M5.Display.setTextColor(WHITE);
  M5.Display.drawString("Volume Test", 10, 10);
  
  // 現在の音量レベル表示
  M5.Display.setTextColor(YELLOW);
  M5.Display.drawString(volume_names[current_test_volume], 10, 40);
  
  // PWM値表示
  M5.Display.setTextColor(CYAN);
  char pwm_text[32];
  snprintf(pwm_text, sizeof(pwm_text), "PWM: %d/255", test_volumes[current_test_volume]);
  M5.Display.drawString(pwm_text, 10, 70);
  
  // 操作説明
  M5.Display.setTextColor(WHITE);
  M5.Display.drawString("A: Next Level", 10, 100);
  M5.Display.drawString("B: Config Test", 10, 115);
}

// ========== config.json変更テスト ==========

void testConfigVolumeChange() {
  Serial.println();
  Serial.println("🔧 config.json音量変更テスト開始");
  
  M5.Display.clear(BLACK);
  M5.Display.setTextColor(GREEN);
  M5.Display.drawString("Config Test", 10, 10);
  M5.Display.drawString("Running...", 10, 40);
  
  // 元の音量を保存
  SystemConfig original_config = config.getSystemConfig();
  int original_volume = original_config.buzzer_volume;
  
  // テスト音量値リスト
  int config_test_volumes[] = {32, 96, 160, 224, 255};
  
  for (int i = 0; i < 5; i++) {
    int new_volume = config_test_volumes[i];
    
    Serial.printf("📝 config.json音量変更: %d → %d\n", original_volume, new_volume);
    
    // config.json更新
    config.setBuzzerVolume(new_volume);
    config.saveConfig();
    
    // 設定再読み込み
    config.loadConfig();
    SystemConfig updated_config = config.getSystemConfig();
    
    // ブザーに適用
    buzzer.setVolume(updated_config.buzzer_volume);
    
    Serial.printf("✅ 設定更新完了: %d\n", updated_config.buzzer_volume);
    
    // LCD表示更新
    char volume_text[32];
    snprintf(volume_text, sizeof(volume_text), "Vol: %d", new_volume);
    M5.Display.fillRect(10, 70, 200, 20, BLACK);
    M5.Display.drawString(volume_text, 10, 70);
    
    // テスト音再生
    buzzer.completion_tone();
    delay(1500);
  }
  
  // 元の音量に復元
  Serial.printf("🔄 音量復元: %d\n", original_volume);
  config.setBuzzerVolume(original_volume);
  config.saveConfig();
  buzzer.setVolume(original_volume);
  
  Serial.println("✅ config.json音量変更テスト完了");
  
  // UI復元
  showVolumeTestUI();
}

// ========== エラー表示 ==========

void showError(const char* message) {
  M5.Display.clear(RED);
  M5.Display.setTextColor(WHITE);
  M5.Display.drawString("ERROR", 10, 10);
  M5.Display.drawString(message, 10, 40);
}

// ========== デバッグ情報表示 ==========

void printVolumeTestInfo() {
  Serial.println();
  Serial.println("========== 音量テスト情報 ==========");
  
  SystemConfig current_config = config.getSystemConfig();
  Serial.printf("config.json音量: %d\n", current_config.buzzer_volume);
  Serial.printf("ブザー有効: %s\n", current_config.buzzer_enabled ? "true" : "false");
  
  Serial.println("テスト音量レベル:");
  for (int i = 0; i < test_volume_count; i++) {
    Serial.printf("  %s: PWM値 %d\n", volume_names[i], test_volumes[i]);
  }
  
  buzzer.printStats();
  Serial.println("==================================");
}