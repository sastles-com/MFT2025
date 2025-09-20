/**
 * @file config_unit_test.ino
 * @brief JoystickConfig単体テストスケッチ
 * @description 設定ファイル管理機能のみを単独でテストするスケッチ
 */

#include <M5Unified.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// 設定管理システム（単体テスト用）
#include "../14_udp_joystick_integration/JoystickConfig.h"

// グローバル変数
JoystickConfig config;
int test_phase = 0;
unsigned long last_test_time = 0;
const int TEST_INTERVAL = 3000;  // 3秒間隔

void setup() {
  // M5Unified初期化
  auto cfg = M5.config();
  cfg.external_spk = false;
  M5.begin(cfg);
  
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("============================================");
  Serial.println("JoystickConfig 単体テストスケッチ");
  Serial.println("============================================");
  Serial.println("テスト内容:");
  Serial.println("  Phase 0: SPIFFS・設定システム初期化");
  Serial.println("  Phase 1: デフォルト設定生成・表示");
  Serial.println("  Phase 2: 設定ファイル読み込みテスト");
  Serial.println("  Phase 3: 設定変更・保存テスト");
  Serial.println("  Phase 4: 設定検証・エラーハンドリング");
  Serial.println("  Phase 5: 統計情報表示");
  Serial.println("  Phase 6: 設定リセット・復元テスト");
  Serial.println();
  
  // LCD初期化
  M5.Display.clear(BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setRotation(0);
  M5.Display.setCursor(0, 0);
  M5.Display.setTextColor(WHITE);
  M5.Display.println("Config Unit Test");
  M5.Display.println("");
  M5.Display.println("Phase 0: Init");
  
  // SPIFFS初期化確認
  Serial.println("Phase 0: SPIFFS・設定システム初期化テスト");
  if (!SPIFFS.begin(true)) {
    Serial.println("❌ SPIFFS初期化失敗");
    M5.Display.println("SPIFFS FAILED");
    return;
  }
  Serial.println("✅ SPIFFS初期化成功");
  
  // 設定管理システム初期化
  if (!config.begin()) {
    Serial.println("❌ 設定管理システム初期化失敗");
    M5.Display.println("Config FAILED");
    return;
  }
  
  Serial.println("✅ 設定管理システム初期化成功");
  M5.Display.println("Init SUCCESS");
  
  last_test_time = millis();
  test_phase = 1;
}

void loop() {
  M5.update();
  
  // ボタンAでテストスキップ
  if (M5.BtnA.wasPressed()) {
    test_phase++;
    if (test_phase > 6) {
      test_phase = 1;  // テスト最初に戻る
    }
    last_test_time = millis() - TEST_INTERVAL; // 即座に実行
    Serial.printf("ボタン押下: Phase %d にスキップ\n", test_phase);
  }
  
  // テスト実行
  if (millis() - last_test_time >= TEST_INTERVAL) {
    executeTestPhase(test_phase);
    test_phase++;
    if (test_phase > 6) {
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
  M5.Display.println("Config Unit Test");
  M5.Display.println("");
  M5.Display.printf("Phase %d\n", phase);
  
  Serial.printf("\n========== Phase %d ==========\n", phase);
  
  switch (phase) {
    case 1:
      Serial.println("Phase 1: デフォルト設定生成・表示");
      M5.Display.println("Default Config");
      
      // 現在の設定表示
      config.printConfig();
      
      // 設定詳細確認
      WiFiAPConfig wifi_config = config.getWiFiAPConfig();
      UDPConfig udp_config = config.getUDPConfig();
      SystemConfig system_config = config.getSystemConfig();
      
      Serial.printf("WiFi SSID: %s\n", wifi_config.ssid);
      Serial.printf("UDP Port: %d\n", udp_config.port);
      Serial.printf("Target IP: %s\n", udp_config.target_ip.toString().c_str());
      Serial.printf("Device Name: %s\n", system_config.device_name);
      Serial.println("デフォルト設定表示完了");
      break;
      
    case 2:
      Serial.println("Phase 2: 設定ファイル読み込みテスト");
      M5.Display.println("Load Config");
      
      // 設定ファイル存在確認
      if (config.isConfigFileExists()) {
        Serial.println("✅ 設定ファイル存在確認");
        
        // 設定読み込み
        if (config.loadConfig()) {
          Serial.println("✅ 設定読み込み成功");
          config.printConfig();
        } else {
          Serial.println("❌ 設定読み込み失敗");
        }
      } else {
        Serial.println("ℹ️ 設定ファイル未存在（初回起動）");
      }
      
      Serial.println("設定読み込みテスト完了");
      break;
      
    case 3:
      Serial.println("Phase 3: 設定変更・保存テスト");
      M5.Display.println("Modify & Save");
      
      // SSID変更テスト
      Serial.println("SSID変更テスト:");
      if (config.setSSID("TestNetwork-Modified")) {
        Serial.println("✅ SSID変更・保存成功");
      } else {
        Serial.println("❌ SSID変更失敗");
      }
      
      // ターゲットIP変更テスト
      Serial.println("ターゲットIP変更テスト:");
      if (config.setTargetIP(IPAddress(192, 168, 100, 99))) {
        Serial.println("✅ ターゲットIP変更・保存成功");
      } else {
        Serial.println("❌ ターゲットIP変更失敗");
      }
      
      // ブザー音量変更テスト
      Serial.println("ブザー音量変更テスト:");
      if (config.setBuzzerVolume(200)) {
        Serial.println("✅ ブザー音量変更・保存成功");
      } else {
        Serial.println("❌ ブザー音量変更失敗");
      }
      
      // 変更後設定表示
      Serial.println("変更後設定:");
      config.printConfig();
      
      Serial.println("設定変更・保存テスト完了");
      break;
      
    case 4:
      Serial.println("Phase 4: 設定検証・エラーハンドリング");
      M5.Display.println("Validation");
      
      // 不正SSID設定テスト
      Serial.println("不正SSID設定テスト:");
      char long_ssid[50];
      for (int i = 0; i < 40; i++) {
        long_ssid[i] = 'A';
      }
      long_ssid[40] = '\0';
      
      if (!config.setSSID(long_ssid)) {
        Serial.println("✅ 長すぎるSSID拒否（正常）");
      } else {
        Serial.println("❌ 長すぎるSSID受け入れ（異常）");
      }
      
      // 不正音量設定テスト
      Serial.println("不正音量設定テスト:");
      if (!config.setBuzzerVolume(300)) {
        Serial.println("✅ 範囲外音量拒否（正常）");
      } else {
        Serial.println("❌ 範囲外音量受け入れ（異常）");
      }
      
      // 設定検証テスト
      Serial.println("設定検証実行:");
      if (config.validateConfig()) {
        Serial.println("✅ 設定検証成功");
      } else {
        Serial.println("❌ 設定検証失敗");
      }
      
      Serial.println("設定検証・エラーハンドリングテスト完了");
      break;
      
    case 5:
      Serial.println("Phase 5: 統計情報表示");
      M5.Display.println("Statistics");
      
      // 統計情報表示
      config.printStats();
      
      const ConfigStats& stats = config.getStats();
      Serial.printf("読み込み回数: %lu\n", stats.load_count);
      Serial.printf("保存回数: %lu\n", stats.save_count);
      Serial.printf("エラー回数: %lu\n", stats.error_count);
      
      Serial.println("統計情報表示完了");
      break;
      
    case 6:
      Serial.println("Phase 6: 設定リセット・復元テスト");
      M5.Display.println("Reset & Restore");
      
      // デフォルト設定リセット
      Serial.println("デフォルト設定リセット:");
      if (config.resetToDefaults()) {
        Serial.println("✅ デフォルト設定リセット成功");
        
        // リセット後設定表示
        Serial.println("リセット後設定:");
        config.printConfig();
        
        // 設定確認
        WiFiAPConfig reset_wifi = config.getWiFiAPConfig();
        if (strcmp(reset_wifi.ssid, "IsolationSphere-Direct") == 0) {
          Serial.println("✅ SSID正常復元確認");
        } else {
          Serial.println("❌ SSID復元失敗");
        }
        
        UDPConfig reset_udp = config.getUDPConfig();
        if (reset_udp.target_ip == IPAddress(192, 168, 100, 100)) {
          Serial.println("✅ ターゲットIP正常復元確認");
        } else {
          Serial.println("❌ ターゲットIP復元失敗");
        }
        
      } else {
        Serial.println("❌ デフォルト設定リセット失敗");
      }
      
      Serial.println("設定リセット・復元テスト完了");
      break;
  }
  
  Serial.printf("Phase %d 実行完了\n", phase);
}