/**
 * @file JoystickOpeningDisplay.cpp
 * @brief Atom-JoyStick起動時オープニング画像表示システム実装
 */

#include "JoystickOpeningDisplay.h"

// グローバル静的インスタンス（コールバック用）
JoystickOpeningDisplay* g_joystick_opening_instance = nullptr;

/**
 * @brief コンストラクタ
 */
JoystickOpeningDisplay::JoystickOpeningDisplay() 
  : initialized_(false)
  , frame_count_(6)  // flare-01.jpg ~ flare-06.jpg
  , stats_({0, 0, 0, false, 0}) {
  g_joystick_opening_instance = this;
}

/**
 * @brief デストラクタ
 */
JoystickOpeningDisplay::~JoystickOpeningDisplay() {
  end();
  if (g_joystick_opening_instance == this) {
    g_joystick_opening_instance = nullptr;
  }
}

/**
 * @brief 初期化
 */
bool JoystickOpeningDisplay::begin() {
  Serial.println("🎬 JoystickOpeningDisplay: 初期化開始");
  
  // デフォルト設定使用
  config_ = JoystickOpeningConfig();
  
  // LittleFS確認
  if (!LittleFS.begin()) {
    printError("LittleFS初期化失敗");
    return false;
  }
  
  // 画像ファイル存在確認
  if (!checkImageFiles()) {
    printError("画像ファイル確認失敗");
    return false;
  }
  
  // JPEG decoder初期化
  if (!setupJpegDecoder()) {
    printError("JPEG decoder初期化失敗");
    return false;
  }
  
  initialized_ = true;
  Serial.println("✅ JoystickOpeningDisplay: 初期化完了");
  
  return true;
}

/**
 * @brief 終了処理
 */
void JoystickOpeningDisplay::end() {
  if (initialized_) {
    initialized_ = false;
    Serial.println("JoystickOpeningDisplay: 終了完了");
  }
}

/**
 * @brief オープニングシーケンス再生
 */
bool JoystickOpeningDisplay::playOpeningSequence() {
  if (!initialized_ || !config_.enabled) {
    Serial.println("⚠️  オープニング無効またはシステム未初期化");
    return false;
  }
  
  Serial.println();
  Serial.println("🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬");
  Serial.println("🎬                                                      🎬");
  Serial.println("🎬        ✨ ISOLATION-SPHERE OPENING ✨               🎬");
  Serial.println("🎬        🎮 Atom-JoyStick System Start 🎮             🎬");
  Serial.println("🎬                                                      🎬");
  Serial.println("🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬");
  Serial.println();
  
  unsigned long sequence_start = millis();
  bool success = true;
  
  // 統計リセット
  stats_.frames_displayed = 0;
  stats_.decode_time_avg = 0;
  stats_.start_time = sequence_start;
  
  // 開始メッセージ表示
  showStartupMessage();
  delay(500);
  
  // フレーム順次表示
  for (uint8_t i = 0; i < frame_count_; i++) {
    unsigned long frame_start = millis();
    
    if (!displayJpegFile(frame_filenames_[i], i)) {
      printError("フレーム表示失敗", frame_filenames_[i]);
      success = false;
      break;
    }
    
    stats_.frames_displayed++;
    
    // プログレス表示
    if (config_.show_progress) {
      showProgressBar(i + 1, frame_count_);
    }
    
    // フレーム表示時間調整
    unsigned long frame_time = millis() - frame_start;
    if (frame_time < config_.frame_duration_ms) {
      delay(config_.frame_duration_ms - frame_time);
    }
    
    // フレーム統計更新
    unsigned long total_frame_time = millis() - frame_start;
    logFrameInfo(frame_filenames_[i], i + 1, frame_time);
  }
  
  stats_.total_play_time = millis() - sequence_start;
  stats_.last_play_success = success;
  
  if (success) {
    showCompletionMessage();
    delay(800);
    
    Serial.println();
    Serial.println("🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬");
    Serial.println("🎬                                                      🎬");
    Serial.println("🎬        ✅ OPENING SEQUENCE COMPLETE ✅              🎬");
    Serial.println("🎬                                                      🎬");
    Serial.printf("🎬        総再生時間: %lums | フレーム数: %d              🎬\n", 
                  stats_.total_play_time, stats_.frames_displayed);
    Serial.println("🎬                                                      🎬");
    Serial.println("🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬🎬");
    Serial.println();
  }
  
  return success;
}

/**
 * @brief JPEG decoder初期化
 */
bool JoystickOpeningDisplay::setupJpegDecoder() {
  // TJpg_Decoder初期化
  TJpgDec.setJpgScale(1);  // スケール1:1
  TJpgDec.setSwapBytes(false);  // M5Display用設定
  TJpgDec.setCallback(tjpgOutputCallback);  // 出力コールバック設定
  
  Serial.println("✅ JPEG decoder初期化完了");
  return true;
}

/**
 * @brief JPEG画像ファイル表示
 */
bool JoystickOpeningDisplay::displayJpegFile(const char* filename, uint8_t frame_index) {
  if (!LittleFS.exists(filename)) {
    printError("ファイル未発見", filename);
    return false;
  }
  
  unsigned long decode_start = millis();
  
  // JPEG decode & display
  uint16_t result = TJpgDec.drawFsJpg(0, 0, filename);
  
  unsigned long decode_time = millis() - decode_start;
  
  if (result != JDR_OK) {
    printError("JPEG decode失敗", filename);
    Serial.printf("エラーコード: %d\n", result);
    return false;
  }
  
  return true;
}

/**
 * @brief TJpg_Decoder出力コールバック
 */
bool JoystickOpeningDisplay::tjpgOutputCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (g_joystick_opening_instance == nullptr) {
    return false;
  }
  
  // M5Display に直接描画
  M5.Display.pushImage(x, y, w, h, bitmap);
  
  return true;
}

/**
 * @brief 開始メッセージ表示
 */
void JoystickOpeningDisplay::showStartupMessage() {
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(CYAN);
  M5.Display.drawCentreString("ISOLATION", 64, 30);
  M5.Display.drawCentreString("SPHERE", 64, 50);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(WHITE);
  M5.Display.drawCentreString("Starting...", 64, 80);
  
  // 小さなアニメーション効果
  for (int i = 0; i < 3; i++) {
    M5.Display.drawCentreString(".", 90 + i * 8, 100);
    delay(100);
  }
}

/**
 * @brief 完了メッセージ表示
 */
void JoystickOpeningDisplay::showCompletionMessage() {
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(GREEN);
  M5.Display.drawCentreString("READY", 64, 40);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(WHITE);
  M5.Display.drawCentreString("System Online", 64, 70);
  M5.Display.drawCentreString("Joystick Active", 64, 85);
}

/**
 * @brief プログレス表示
 */
void JoystickOpeningDisplay::showProgressBar(uint8_t current, uint8_t total) {
  // プログレスバー描画（画面下部）
  int progress_width = (128 * current) / total;
  
  // プログレスバー背景
  M5.Display.drawRect(10, 110, 108, 8, WHITE);
  
  // プログレスバー進行
  M5.Display.fillRect(12, 112, progress_width - 4, 4, CYAN);
  
  // パーセンテージ表示
  int percentage = (current * 100) / total;
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(WHITE);
  String progress_text = String(percentage) + "%";
  M5.Display.drawCentreString(progress_text, 64, 95);
}

/**
 * @brief 画像ファイル存在確認
 */
bool JoystickOpeningDisplay::checkImageFiles() {
  Serial.println("🎬 画像ファイル確認中...");
  
  uint8_t found_files = 0;
  
  for (uint8_t i = 0; i < frame_count_; i++) {
    if (LittleFS.exists(frame_filenames_[i])) {
      size_t file_size = getFileSize(frame_filenames_[i]);
      Serial.printf("  ✅ %s (%d bytes)\n", frame_filenames_[i], file_size);
      found_files++;
    } else {
      Serial.printf("  ❌ %s (ファイル未発見)\n", frame_filenames_[i]);
    }
  }
  
  if (found_files == 0) {
    Serial.println("❌ 画像ファイル一個も見つからない");
    return false;
  } else if (found_files < frame_count_) {
    Serial.printf("⚠️  画像ファイル一部欠如: %d/%d見つかった\n", found_files, frame_count_);
  }
  
  Serial.printf("✅ 画像ファイル確認完了: %d/%d見つかった\n", found_files, frame_count_);
  return true;
}

/**
 * @brief ファイルサイズ取得
 */
size_t JoystickOpeningDisplay::getFileSize(const char* filename) {
  File file = LittleFS.open(filename, "r");
  if (!file) {
    return 0;
  }
  size_t size = file.size();
  file.close();
  return size;
}

/**
 * @brief フレーム情報ログ出力
 */
void JoystickOpeningDisplay::logFrameInfo(const char* filename, uint8_t frame_index, unsigned long decode_time) {
  Serial.printf("🎬 フレーム %d/%d: %s | decode: %lums\n", 
                frame_index, frame_count_, filename, decode_time);
}

/**
 * @brief エラー出力
 */
void JoystickOpeningDisplay::printError(const char* message, const char* detail) {
  Serial.printf("❌ JoystickOpeningDisplay: %s", message);
  if (detail != nullptr) {
    Serial.printf(" - %s", detail);
  }
  Serial.println();
}

/**
 * @brief 統計情報出力
 */
void JoystickOpeningDisplay::printStats() const {
  Serial.println();
  Serial.println("========== Joystick オープニング統計 ==========");
  Serial.printf("総再生時間: %lums\n", stats_.total_play_time);
  Serial.printf("表示フレーム数: %d\n", stats_.frames_displayed);
  Serial.printf("平均デコード時間: %lums\n", stats_.decode_time_avg);
  Serial.printf("最終再生結果: %s\n", stats_.last_play_success ? "成功" : "失敗");
  Serial.printf("開始時刻: %lums\n", stats_.start_time);
  Serial.println("=============================================");
  Serial.println();
}