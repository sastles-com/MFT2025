/**
 * @file atom_s3_receiver.ino
 * @brief M5Stack AtomS3 Joystick UDP受信・LED制御検証システム
 * @description Atom-JoyStickからのUDP受信でLED制御を実現
 * 
 * Phase 4.9: AtomS3検証用受信システム
 * - WiFi Client: IsolationSphere-Direct接続
 * - UDP受信: ポート1884、JSON形式
 * - LED制御: GPIO35、WS2812制御
 * - 応答性: 15-30ms目標達成
 * 
 * @target M5Stack AtomS3 (ESP32-S3)
 * @integration isolation-sphere分散制御システム検証
 */

#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <SPIFFS.h>
#include <TJpg_Decoder.h>

#include "config_manager.h"
#include "wifi_client.h"
#include "udp_receiver.h"
#include "led_controller.h"
#include "opening_display.h"

// ========== システム設定 ==========
#define SYSTEM_VERSION "1.0.0"
#define BUILD_DATE "2025-09-03"

// ========== グローバル変数 ==========
ConfigManager configManager;
WiFiManager wifiManager;
UDPReceiver udpReceiver;
LEDController ledController;
OpeningDisplay openingDisplay;

// ========== システム状態管理 ==========
struct SystemStatus {
  bool wifi_connected;
  bool udp_receiving; 
  bool led_active;
  unsigned long last_packet_time;
  unsigned long packet_count;
  float avg_response_time;
} systemStatus = {false, false, false, 0, 0, 0.0};

// ========== パフォーマンス測定 ==========
unsigned long last_stats_time = 0;
const unsigned long STATS_INTERVAL = 10000; // 10秒間隔

/**
 * @brief システム初期化
 */
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.println("████████████████████████████████████████████████████████");
  Serial.println("██                                                    ██");
  Serial.println("██    AtomS3 Joystick UDP受信検証システム開始           ██");
  Serial.println("██                                                    ██");
  Serial.println("████████████████████████████████████████████████████████");
  Serial.println();
  Serial.printf("██ バージョン: %s (%s) ██\n", SYSTEM_VERSION, BUILD_DATE);
  Serial.println();
  
  // M5Unified初期化
  auto cfg = M5.config();
  cfg.clear_display = true;
  cfg.output_power = true;
  cfg.internal_rtc = true;
  cfg.internal_spk = false;  // スピーカー無効
  cfg.internal_mic = false;  // マイク無効
  M5.begin(cfg);
  
  Serial.println("██ ✅ M5Unified初期化完了");
  Serial.println();
  
  // SPIFFS初期化
  if (!SPIFFS.begin(true)) {
    Serial.println("██ ❌❌❌ SPIFFS初期化失敗 ❌❌❌");
    return;
  }
  Serial.println("██ ✅ SPIFFS初期化完了");
  
  // 設定管理初期化
  if (!configManager.begin()) {
    Serial.println("██ ❌❌❌ 設定管理初期化失敗 ❌❌❌");
    return;
  }
  Serial.println("██ ✅ 設定管理初期化完了");
  
  // WiFi接続初期化
  if (!wifiManager.begin(configManager)) {
    Serial.println("██ ❌❌❌ WiFi接続初期化失敗 ❌❌❌");
    return;
  }
  Serial.println("██ ✅ WiFi接続初期化完了");
  
  // UDP受信システム初期化
  if (!udpReceiver.begin(configManager)) {
    Serial.println("██ ❌❌❌ UDP受信初期化失敗 ❌❌❌");
    return;
  }
  Serial.println("██ ✅ UDP受信初期化完了");
  
  // LED制御システム初期化
  if (!ledController.begin(configManager)) {
    Serial.println("██ ❌❌❌ LED制御初期化失敗 ❌❌❌");
    return;
  }
  Serial.println("██ ✅ LED制御初期化完了");
  
  Serial.println();
  Serial.println("████████████████████████████████████████████████████████");
  Serial.println("██                                                    ██");
  Serial.println("██       🚀🚀 システム初期化完了 - 動作開始 🚀🚀       ██");
  Serial.println("██                                                    ██");
  Serial.println("████████████████████████████████████████████████████████");
  Serial.println();
  
  // オープニング表示システム初期化
  if (!openingDisplay.begin(configManager)) {
    Serial.println("██ ❌❌❌ オープニング表示初期化失敗 ❌❌❌");
    // 失敗しても処理継続（オープニングはオプション機能）
  } else {
    Serial.println("██ ✅ オープニング表示初期化完了");
    
    // オープニング演出実行
    if (!openingDisplay.playOpeningSequence()) {
      Serial.println("██ ⚠️  オープニング演出実行失敗（処理継続）");
    }
  }
  
  // 初期化成功を示すLED点灯
  ledController.showInitializationComplete();
  
  // LCD初期化（オープニング後）
  M5.Display.setTextSize(2);  // joystick文字を大きくする要望対応
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextColor(WHITE);
  M5.Display.drawString("AtomS3 Receiver", 5, 10);
  M5.Display.drawString("Init Complete", 5, 40);
}

/**
 * @brief Joystick状態をLCDに表示
 */
void displayJoystickStatus(const JoystickData& data) {
  static unsigned long last_lcd_update = 0;
  
  // LCD更新頻度制限（200ms間隔）
  if (millis() - last_lcd_update < 200) {
    return;
  }
  last_lcd_update = millis();
  
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextSize(2);  // 大きな文字サイズ
  M5.Display.setTextColor(WHITE);
  
  // タイトル  
  M5.Display.drawString("Joystick", 5, 5);
  
  // 左スティック
  M5.Display.drawString("L:", 5, 25);
  M5.Display.printf("%.1f,%.1f", data.left_x, data.left_y);
  if (data.left_stick_button) {
    M5.Display.setTextColor(RED);
    M5.Display.drawString("●", 100, 25);
    M5.Display.setTextColor(WHITE);
  }
  
  // 右スティック  
  M5.Display.drawString("R:", 5, 45);
  M5.Display.printf("%.1f,%.1f", data.right_x, data.right_y);
  if (data.right_stick_button) {
    M5.Display.setTextColor(RED);
    M5.Display.drawString("●", 100, 45);
    M5.Display.setTextColor(WHITE);
  }
  
  // L/Rボタン
  M5.Display.drawString("BTN:", 5, 65);
  if (data.button_left) {
    M5.Display.setTextColor(RED);
    M5.Display.drawString("L", 50, 65);
    M5.Display.setTextColor(WHITE);
  } else {
    M5.Display.drawString("L", 50, 65);
  }
  
  if (data.button_right) {
    M5.Display.setTextColor(RED);
    M5.Display.drawString("R", 70, 65);
    M5.Display.setTextColor(WHITE);
  } else {
    M5.Display.drawString("R", 70, 65);
  }
  
  // バッテリー
  M5.Display.drawString("BAT:", 5, 85);
  M5.Display.printf("%.1fV", data.battery);
  
  // パケット数表示
  M5.Display.drawString("PKT:", 5, 105);
  M5.Display.printf("%lu", systemStatus.packet_count);
}

/**
 * @brief 信号なし表示
 */
void displayNoSignal() {
  static unsigned long last_update = 0;
  if (millis() - last_update < 1000) return; // 1秒間隔更新
  last_update = millis();
  
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(RED);
  M5.Display.drawString("No Signal", 10, 30);
  M5.Display.setTextColor(WHITE);
  M5.Display.drawString("Joystick", 10, 60);
  M5.Display.drawString("Waiting...", 10, 90);
}

/**
 * @brief WiFi未接続表示
 */
void displayWiFiDisconnected() {
  static unsigned long last_update = 0;
  if (millis() - last_update < 1000) return; // 1秒間隔更新
  last_update = millis();
  
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(YELLOW);
  M5.Display.drawString("WiFi", 10, 30);
  M5.Display.setTextColor(WHITE);
  M5.Display.drawString("Disconnected", 10, 60);
  M5.Display.drawString("Connecting...", 10, 90);
}

/**
 * @brief メインループ
 */
void loop() {
  unsigned long loop_start = millis();
  
  // WiFi接続状態監視・自動再接続
  systemStatus.wifi_connected = wifiManager.update();
  
  // UDP受信・処理
  if (systemStatus.wifi_connected) {
    JoystickData joystickData;
    if (udpReceiver.receiveData(joystickData)) {
      // パケット受信成功
      systemStatus.last_packet_time = millis();
      systemStatus.packet_count++;
      systemStatus.udp_receiving = true;
      
      // 実際の受信間隔測定（デバイス間タイムスタンプ差を回避）
      static unsigned long last_receive_time = 0;
      unsigned long current_receive_time = millis();
      unsigned long receive_interval = 0;
      
      if (last_receive_time > 0) {
        receive_interval = current_receive_time - last_receive_time;
        updateResponseTimeAverage(receive_interval);
        
        // UDP受信頻度警告（33Hz想定なので30ms間隔）
        if (receive_interval > 50) {
          Serial.println();
          Serial.println("██ ⚠️ ⚠️ ⚠️  受信間隔警告  ⚠️ ⚠️ ⚠️ ██");
          Serial.printf("██    受信間隔: %lums (目標<50ms)      ██\n", receive_interval);
          Serial.println("████████████████████████████████████████");
          Serial.println();
        }
      }
      last_receive_time = current_receive_time;
      
      // LED制御更新
      ledController.updateFromJoystick(joystickData);
      systemStatus.led_active = true;
      
      // LCD joystick状態表示更新
      displayJoystickStatus(joystickData);
    } else {
      // 受信タイムアウト判定
      if (millis() - systemStatus.last_packet_time > 5000) {
        systemStatus.udp_receiving = false;
        systemStatus.led_active = false;
        ledController.showNoSignal();
        displayNoSignal();
      }
    }
  } else {
    // WiFi未接続時の処理
    systemStatus.udp_receiving = false;
    systemStatus.led_active = false;
    ledController.showWiFiDisconnected();
    displayWiFiDisconnected();
  }
  
  // LED制御更新（常時）
  ledController.update();
  
  // 統計情報出力
  if (millis() - last_stats_time > STATS_INTERVAL) {
    printSystemStatistics();
    last_stats_time = millis();
  }
  
  // M5Unified更新
  M5.update();
  
  // ループ時間監視
  unsigned long loop_time = millis() - loop_start;
  if (loop_time > 50) { // 50ms超過警告
    Serial.printf("⚠️  ループ時間警告: %lums\n", loop_time);
  }
  
  delay(1); // システム安定性確保
}

/**
 * @brief 応答時間平均値更新
 */
void updateResponseTimeAverage(unsigned long response_time) {
  static const int AVERAGE_SAMPLES = 10;
  static float samples[AVERAGE_SAMPLES] = {0};
  static int sample_index = 0;
  
  samples[sample_index] = (float)response_time;
  sample_index = (sample_index + 1) % AVERAGE_SAMPLES;
  
  float sum = 0;
  for (int i = 0; i < AVERAGE_SAMPLES; i++) {
    sum += samples[i];
  }
  systemStatus.avg_response_time = sum / AVERAGE_SAMPLES;
}

/**
 * @brief システム統計情報出力
 */
void printSystemStatistics() {
  Serial.println();
  Serial.println("████████████████████████████████████████████████████████");
  Serial.println("██                                                    ██");
  Serial.println("██              📊 システム統計 📊                    ██");
  Serial.println("██                                                    ██");
  Serial.println("████████████████████████████████████████████████████████");
  
  Serial.printf("██ WiFi接続: %s                      ██\n", 
                systemStatus.wifi_connected ? "🟢🟢 接続中 🟢🟢" : "🔴🔴 切断 🔴🔴");
  Serial.printf("██ UDP受信: %s                       ██\n", 
                systemStatus.udp_receiving ? "🟢🟢 受信中 🟢🟢" : "🔴🔴 未受信 🔴🔴");
  Serial.printf("██ LED制御: %s                   ██\n", 
                systemStatus.led_active ? "🟢🟢 アクティブ 🟢🟢" : "🔴🔴 非アクティブ 🔴🔴");
  Serial.println("██                                                    ██");
  Serial.printf("██ 📦 受信パケット数: %lu                           ██\n", systemStatus.packet_count);
  
  if (systemStatus.udp_receiving) {
    Serial.printf("██ ⚡ 平均応答時間: %.1fms                         ██\n", systemStatus.avg_response_time);
    unsigned long uptime = (millis() - systemStatus.last_packet_time);
    Serial.printf("██ 🕐 最終受信: %lus前                            ██\n", uptime / 1000);
    
    if (systemStatus.avg_response_time <= 30.0f) {
      Serial.println("██ ✅✅ 応答性目標達成！(<30ms) ✅✅                ██");
    } else {
      Serial.println("██ ⚠️ ⚠️  応答性要改善 (>30ms) ⚠️ ⚠️                ██");
    }
  }
  
  Serial.println("██                                                    ██");
  Serial.printf("██ 💾 空きメモリ: %d bytes                         ██\n", ESP.getFreeHeap());
  Serial.printf("██ 📡 WiFi信号強度: %d dBm                        ██\n", WiFi.RSSI());
  Serial.println("██                                                    ██");
  Serial.println("████████████████████████████████████████████████████████");
  Serial.println();
}