#include <Arduino.h>
#include <LittleFS.h>
#include <M5Unified.h>

// 個別機能クラス (コンポジションベース設計)
#include "config/ConfigManager.h"
#include "wifi/WiFiManager.h" 
#include "mqtt/MqttBroker.h"
#include "buzzer/M5SpeakerBuzzer.h"
#include "buzzer/JoystickBuzzer.h"

// グローバルオブジェクト (個別機能クラス)
ConfigManager* gConfigManager = nullptr;
WiFiManager* gWiFiManager = nullptr;
MqttBroker* gMqttBroker = nullptr;
M5SpeakerBuzzer* gM5Buzzer = nullptr;
JoystickBuzzer* gJoystickBuzzer = nullptr;

// 設定
ConfigManager::Config gConfig;

// 音量テスト用グローバル変数
int gCurrentVolumeLevel = 0;  // 0: 低音量, 1: 中音量, 2: 高音量

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("==============================");
  Serial.println(" MFT2025 Joystick (Composition)");
  Serial.println("==============================");

  // M5Unified初期化
  auto cfg = M5.config();
  cfg.clear_display = true;
  cfg.output_power = false;
  cfg.internal_imu = false;
  cfg.internal_rtc = false;
  cfg.internal_spk = false;
  cfg.internal_mic = false;
  M5.begin(cfg);
  
  // LittleFS初期化
  if (!LittleFS.begin(true, "/littlefs", 5, "spiffs")) {
    Serial.println("[Main] LittleFS mount failed");
    return;
  }
  Serial.println("[Main] LittleFS mounted");

  // ConfigManager初期化
  gConfigManager = new ConfigManager(ConfigManager::makeLittleFsProvider());
  if (!gConfigManager->load()) {
    Serial.println("[Main] Config load failed - using defaults");
  } else {
    Serial.println("[Main] Config loaded successfully");
  }
  
  gConfig = gConfigManager->config();
  
  // AudioConfig設定値デバッグ出力
  Serial.printf("[Main] AudioConfig Debug - enabled:%s, master:%d%%, startup:%d%%, click:%d%%, error:%d%%, test:%d%%\n",
               gConfig.joystick.audio.enabled ? "true" : "false",
               gConfig.joystick.audio.masterVolume,
               gConfig.joystick.audio.volumes.startup,
               gConfig.joystick.audio.volumes.click,
               gConfig.joystick.audio.volumes.error,
               gConfig.joystick.audio.volumes.test);

  // WiFiManager初期化 (AP mode)
  gWiFiManager = new WiFiManager();
  if (!gWiFiManager->initialize(gConfig)) {
    Serial.println("[Main] WiFiManager initialization failed");
  } else {
    Serial.println("[Main] WiFiManager initialized");
  }

  // MqttBroker初期化 (WiFiManager後に実行)
  gMqttBroker = new MqttBroker();
  if (!gMqttBroker->applyConfig(gConfig)) {
    Serial.println("[Main] MqttBroker initialization failed");
  } else {
    Serial.println("[Main] MqttBroker initialized");
  }

  // ブザー初期化 - まずM5Speakerを試す
  ConfigManager::BuzzerConfig buzzerConfig;
  buzzerConfig.enabled = gConfig.joystick.system.buzzerEnabled;
  buzzerConfig.volume = gConfig.joystick.system.buzzerVolume;
  
  Serial.println("[Main] Testing M5 Speaker first...");
  gM5Buzzer = new M5SpeakerBuzzer(buzzerConfig);
  if (gM5Buzzer->initialize()) {
    Serial.println("[Main] M5SpeakerBuzzer initialized successfully");
  } else {
    Serial.println("[Main] M5SpeakerBuzzer failed, trying GPIO5 PWM...");
    delete gM5Buzzer;
    gM5Buzzer = nullptr;
    
    // M5Speakerが使えない場合はGPIO5 PWMを試す（詳細音響設定を使用）
    gJoystickBuzzer = new JoystickBuzzer(gConfig.joystick.audio);
    if (!gJoystickBuzzer->initialize()) {
      Serial.println("[Main] JoystickBuzzer (GPIO5) initialization failed");
      delete gJoystickBuzzer;
      gJoystickBuzzer = nullptr;
    } else {
      Serial.println("[Main] JoystickBuzzer (GPIO5) initialized");
    }
  }
  
  Serial.println("[Main] All components initialized");
  
  // 初期化後のテスト音（デバッグ用）
  if (gM5Buzzer) {
    Serial.println("[Main] Testing M5 Speaker buzzer...");
    gM5Buzzer->playTone(1000, 500);  // 1kHz, 500ms
    delay(1000);
    gM5Buzzer->playTone(2000, 300);  // 2kHz, 300ms  
    delay(500);
    Serial.println("[Main] M5 Speaker test completed");
  } else if (gJoystickBuzzer) {
    Serial.println("[Main] Testing Passive Buzzer on GPIO5...");
    
    // パッシブブザー専用テスト
    // gJoystickBuzzer->playScaleTest();        // ドレミファソラシド（開発時無効化）
    delay(1000);
    // gJoystickBuzzer->playFrequencySweep();   // 周波数スイープ（開発時無効化）
    delay(1000);
    // gJoystickBuzzer->playStartupMelody();    // 起動メロディ（開発時無効化）
    
    Serial.println("[Main] Passive Buzzer test completed");
  } else {
    Serial.println("[Main] No buzzer available for testing");
  }
}

void loop() {
  // WiFiManager のループ処理
  if (gWiFiManager) {
    gWiFiManager->loop();
  }
  
  // MqttBroker のループ処理
  if (gMqttBroker) {
    gMqttBroker->loop();
  }
  
  // M5ボタン処理 - パッシブブザーテスト切り替え
  M5.update();
  static int testMode = 0;
  
  if (M5.BtnA.wasPressed()) {
    Serial.printf("[Main] Button pressed - Test mode: %d\n", testMode);
    
    // ブザーテスト + 音種別音量テスト
    if (gJoystickBuzzer) {
      switch (testMode % 8) {  // 8モードに拡張
        case 0:
          // gJoystickBuzzer->playStartupMelody();  // 開発時無効化
          Serial.println("→ Startup melody (volume: startup)");
          break;
        case 1:
          gJoystickBuzzer->playClickTone();
          Serial.println("→ Click tone (volume: click)");
          break;
        case 2:
          gJoystickBuzzer->playErrorTone();
          Serial.println("→ Error tone (volume: error)");
          break;
        case 3:
          gJoystickBuzzer->playScaleTest();
          Serial.println("→ Musical scale test (volume: test)");
          break;
        case 4:
          gJoystickBuzzer->playFrequencySweep();
          Serial.println("→ Frequency sweep (volume: test)");
          break;
        case 5:
          gJoystickBuzzer->playConnectTone();
          Serial.println("→ Connect tone (volume: test)");
          break;
        case 6:
          // マスター音量テスト: 低→中→高
          {
            int volumes[] = {10, 50, 90};
            const char* levels[] = {"LOW", "MID", "HIGH"};
            
            gCurrentVolumeLevel = (gCurrentVolumeLevel + 1) % 3;
            int newMasterVolume = volumes[gCurrentVolumeLevel];
            
            // 一時的に新しいJoystickBuzzerを作成してテスト
            ConfigManager::JoystickConfig::AudioConfig tempAudio = gConfig.joystick.audio;
            tempAudio.masterVolume = newMasterVolume;
            
            JoystickBuzzer* tempBuzzer = new JoystickBuzzer(tempAudio);
            tempBuzzer->initialize();
            tempBuzzer->playClickTone();
            
            Serial.printf("→ Master Volume %s: %d%% (startup:%d, click:%d, error:%d, test:%d)\n", 
                         levels[gCurrentVolumeLevel], newMasterVolume,
                         (newMasterVolume * tempAudio.volumes.startup) / 100,
                         (newMasterVolume * tempAudio.volumes.click) / 100,
                         (newMasterVolume * tempAudio.volumes.error) / 100,
                         (newMasterVolume * tempAudio.volumes.test) / 100);
            
            delete tempBuzzer;
          }
          break;
        case 7:
          // 音種別音量比較テスト
          {
            Serial.println("→ Sound type volume comparison:");
            const char* soundTypes[] = {"startup", "click", "error", "test"};
            int frequencies[] = {523, 1000, 200, 800};  // 各音種の代表周波数
            
            for (int i = 0; i < 4; i++) {
              Serial.printf("  %s: ", soundTypes[i]);
              gJoystickBuzzer->playTone(frequencies[i], 300);
              delay(500);
            }
            Serial.println("  Comparison complete");
          }
          break;
      }
    } else if (gM5Buzzer) {
      gM5Buzzer->playClickTone();
    }
    
    // MQTTテストメッセージ送信
    if (gMqttBroker && gMqttBroker->isActive()) {
      char testMessage[128];
      int currentVolume = gJoystickBuzzer ? gJoystickBuzzer->getVolume() : 0;
      snprintf(testMessage, sizeof(testMessage), 
               "{\"timestamp\": %lu, \"test_mode\": %d, \"button\": \"pressed\", \"volume\": %d}", 
               millis(), testMode, currentVolume);
      gMqttBroker->publish("joystick/test", testMessage, false);
      
      // ジョイスティック状態もテスト送信 (ダミーデータ)
      gMqttBroker->publishJoystickState(0.0, 0.0, 0.0, 0.0, true, false, false, false);
      Serial.println("→ MQTT test messages published");
    }
    
    testMode++;
  }
  
  // 定期的なステータス表示とテスト音
  static unsigned long lastStatusTime = 0;
  if (millis() - lastStatusTime > 10000) { // 10秒間隔
    lastStatusTime = millis();
    Serial.println("[Main] Status check - playing completion tone");
    
    // MQTT状況送信
    if (gMqttBroker && gMqttBroker->isActive()) {
      char statusMessage[256];
      auto stats = gMqttBroker->getStats();
      snprintf(statusMessage, sizeof(statusMessage), 
               "{\"uptime\": %lu, \"clients\": %d, \"messages\": %d, \"topics\": %d}", 
               millis(), stats.connectedClients, stats.totalMessages, stats.activeTopics);
      gMqttBroker->publish("joystick/status", statusMessage, true);
      Serial.printf("→ MQTT Status: %d clients, %d messages\n", 
                   stats.connectedClients, stats.totalMessages);
    }
    
    // 開発時：完了音を無効化
    Serial.println("[Main] Completion tone DISABLED for development");
    // if (gM5Buzzer) {
    //   gM5Buzzer->playCompletionTone();
    // } else if (gJoystickBuzzer) {
    //   gJoystickBuzzer->playCompletionTone();
    // }
  }
  
  delay(10);
}