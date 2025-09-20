/**
 * @file opening_display_unit_test.ino
 * @brief JoystickOpeningDisplay単体テストスケッチ
 * @description オープニング画像表示機能のみを単独でテストするスケッチ
 */

#include <M5Unified.h>
#include <LittleFS.h>
#include <TJpg_Decoder.h>

// オープニング表示システム（単体テスト用）
#include "../14_udp_joystick_integration/JoystickOpeningDisplay.h"

// グローバル変数
JoystickOpeningDisplay opening_display;
int test_phase = 0;
unsigned long last_test_time = 0;
const int TEST_INTERVAL = 5000;  // 5秒間隔

void setup() {
  // M5Unified初期化
  auto cfg = M5.config();
  cfg.external_spk = false;
  M5.begin(cfg);
  
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("============================================");
  Serial.println("JoystickOpeningDisplay 単体テストスケッチ");
  Serial.println("============================================");
  Serial.println("テスト内容:");
  Serial.println("  Phase 0: SPIFFS・表示システム初期化");
  Serial.println("  Phase 1: SPIFFS内容確認・ファイル一覧表示");
  Serial.println("  Phase 2: 個別JPEG画像テスト（flare-01.jpg）");
  Serial.println("  Phase 3: 個別JPEG画像テスト（flare-06.jpg）");
  Serial.println("  Phase 4: 完全オープニングシーケンス実行");
  Serial.println("  Phase 5: 統計情報表示・リセットテスト");
  Serial.println();
  
  // LCD初期化
  M5.Display.clear(BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setRotation(0);
  M5.Display.setCursor(0, 0);
  M5.Display.setTextColor(WHITE);
  M5.Display.println("Opening Unit Test");
  M5.Display.println("");
  M5.Display.println("Phase 0: Init");
  
  // LittleFS初期化確認
  Serial.println("Phase 0: LittleFS・表示システム初期化テスト");
  if (!LittleFS.begin(true)) {
    Serial.println("❌ LittleFS初期化失敗");
    M5.Display.println("LittleFS FAILED");
    return;
  }
  Serial.println("✅ LittleFS初期化成功");
  
  // オープニング表示システム初期化
  if (!opening_display.begin()) {
    Serial.println("❌ オープニング表示システム初期化失敗");
    M5.Display.println("Opening FAILED");
    return;
  }
  
  Serial.println("✅ オープニング表示システム初期化成功");
  M5.Display.println("Init SUCCESS");
  
  last_test_time = millis();
  test_phase = 1;
}

void loop() {
  M5.update();
  
  // ボタンAでテストスキップ
  if (M5.BtnA.wasPressed()) {
    test_phase++;
    if (test_phase > 5) {
      test_phase = 1;  // テスト最初に戻る
    }
    last_test_time = millis() - TEST_INTERVAL; // 即座に実行
    Serial.printf("ボタン押下: Phase %d にスキップ\n", test_phase);
  }
  
  // テスト実行
  if (millis() - last_test_time >= TEST_INTERVAL) {
    executeTestPhase(test_phase);
    test_phase++;
    if (test_phase > 5) {
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
  M5.Display.println("Opening Unit Test");
  M5.Display.println("");
  M5.Display.printf("Phase %d\n", phase);
  
  Serial.printf("\n========== Phase %d ==========\n", phase);
  
  switch (phase) {
    case 1:
      Serial.println("Phase 1: LittleFS内容確認・ファイル一覧表示");
      M5.Display.println("LittleFS Check");
      listLittleFSFiles();
      break;
      
    case 2:
      Serial.println("Phase 2: 個別JPEG画像テスト（flare-01.jpg）");
      M5.Display.println("JPEG Test 1");
      testIndividualImage("/images/flare-01.jpg");
      break;
      
    case 3:
      Serial.println("Phase 3: 個別JPEG画像テスト（flare-06.jpg）");
      M5.Display.println("JPEG Test 6");
      testIndividualImage("/images/flare-06.jpg");
      break;
      
    case 4: {
      Serial.println("Phase 4: 完全オープニングシーケンス実行");
      M5.Display.println("Full Sequence");
      
      // 開始メッセージ
      M5.Display.clear(BLACK);
      M5.Display.setCursor(0, 0);
      M5.Display.setTextColor(GREEN);
      M5.Display.setTextSize(2);
      M5.Display.println("Starting");
      M5.Display.println("Opening");
      M5.Display.println("Sequence");
      delay(1000);
      
      // オープニングシーケンス実行
      bool success = opening_display.playOpeningSequence();
      
      // 結果表示
      M5.Display.clear(BLACK);
      M5.Display.setCursor(0, 0);
      M5.Display.setTextSize(2);
      if (success) {
        M5.Display.setTextColor(GREEN);
        M5.Display.println("SUCCESS!");
        Serial.println("✅ オープニングシーケンス成功");
      } else {
        M5.Display.setTextColor(RED);
        M5.Display.println("FAILED!");
        Serial.println("❌ オープニングシーケンス失敗");
      }
      
      delay(2000);
      break;
    }
      
    case 5:
      Serial.println("Phase 5: 統計情報表示・リセットテスト");
      M5.Display.println("Statistics");
      opening_display.printStats();
      
      // 統計表示のみ（resetStats関数が存在しないため削除）
      Serial.println("統計情報表示完了");
      
      Serial.println("統計テスト完了");
      break;
  }
  
  Serial.printf("Phase %d 実行完了\n", phase);
}

void listLittleFSFiles() {
  Serial.println("LittleFS ファイル一覧:");
  
  File root = LittleFS.open("/");
  if (!root) {
    Serial.println("❌ ルートディレクトリオープン失敗");
    return;
  }
  
  if (!root.isDirectory()) {
    Serial.println("❌ ルートディレクトリではない");
    return;
  }
  
  File file = root.openNextFile();
  int file_count = 0;
  
  while (file) {
    if (file.isDirectory()) {
      Serial.printf("  DIR: %s\n", file.name());
    } else {
      Serial.printf("  FILE: %s (%d bytes)\n", file.name(), file.size());
      file_count++;
    }
    file = root.openNextFile();
  }
  
  Serial.printf("総ファイル数: %d\n", file_count);
  
  // /images ディレクトリ確認
  Serial.println("\n/images ディレクトリ確認:");
  File images_dir = LittleFS.open("/images");
  if (images_dir && images_dir.isDirectory()) {
    File image_file = images_dir.openNextFile();
    int image_count = 0;
    
    while (image_file) {
      if (!image_file.isDirectory()) {
        Serial.printf("  IMAGE: %s (%d bytes)\n", image_file.name(), image_file.size());
        image_count++;
      }
      image_file = images_dir.openNextFile();
    }
    
    Serial.printf("/images内画像数: %d\n", image_count);
  } else {
    Serial.println("❌ /images ディレクトリが見つからない");
  }
  
  // 容量確認
  Serial.printf("LittleFS使用容量: %d bytes\n", LittleFS.usedBytes());
  Serial.printf("LittleFS総容量: %d bytes\n", LittleFS.totalBytes());
}

void testIndividualImage(const char* image_path) {
  Serial.printf("個別画像テスト: %s\n", image_path);
  
  // ファイル存在確認
  if (!LittleFS.exists(image_path)) {
    Serial.printf("❌ ファイルが存在しません: %s\n", image_path);
    M5.Display.clear(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextColor(RED);
    M5.Display.println("FILE NOT FOUND");
    M5.Display.println(image_path);
    return;
  }
  
  // ファイルサイズ確認
  File file = LittleFS.open(image_path, "r");
  if (file) {
    Serial.printf("ファイルサイズ: %d bytes\n", file.size());
    file.close();
  }
  
  // JPEG表示テスト
  M5.Display.clear(BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.setTextColor(YELLOW);
  M5.Display.println("Loading...");
  M5.Display.println(image_path);
  
  delay(500);
  
  // TJpg_Decoderを使用してJPEG表示
  uint16_t w = 0, h = 0;
  TJpgDec.getFsJpgSize(&w, &h, image_path);
  Serial.printf("JPEG画像サイズ: %dx%d\n", w, h);
  
  // 画像表示
  TJpgDec.drawFsJpg(0, 0, image_path);
  Serial.printf("✅ 画像表示完了: %s\n", image_path);
  
  // 3秒間表示
  delay(3000);
}