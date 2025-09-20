/**
 * @file opening_display.cpp
 * @brief 起動時オープニング画像表示システム実装
 */

#include "opening_display.h"

// グローバル静的インスタンス（コールバック用）
OpeningDisplay* g_opening_display_instance = nullptr;

/**
 * @brief コンストラクタ
 */
OpeningDisplay::OpeningDisplay() 
  : initialized_(false)
  , frame_count_(6)  // flare-01.jpg ~ flare-06.jpg
  , stats_({0, 0, 0, 0, false}) {
  g_opening_display_instance = this;
}

/**
 * @brief デストラクタ
 */
OpeningDisplay::~OpeningDisplay() {
  end();
  if (g_opening_display_instance == this) {
    g_opening_display_instance = nullptr;
  }
}

/**
 * @brief 初期化
 */
bool OpeningDisplay::begin(const ConfigManager& config) {
  Serial.println("OpeningDisplay: 初期化開始");
  
  // 設定読み込み（config.jsonから読み取り）
  const auto& cfg = config.getOpeningConfig();
  config_.enabled = cfg.enabled;
  config_.frame_duration_ms = cfg.frame_duration_ms;
  config_.brightness = cfg.brightness;
  config_.fade_effect = cfg.fade_effect;
  config_.fade_steps = cfg.fade_steps;
  
  // SPIFFS確認
  if (!SPIFFS.begin()) {
    printError("SPIFFS初期化失敗");
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
  Serial.println("✅ OpeningDisplay: 初期化完了");
  
  return true;
}

/**
 * @brief 終了処理
 */
void OpeningDisplay::end() {
  if (initialized_) {
    initialized_ = false;
    Serial.println("OpeningDisplay: 終了完了");
  }
}

/**
 * @brief オープニングシーケンス再生
 */
bool OpeningDisplay::playOpeningSequence() {
  if (!initialized_ || !config_.enabled) {
    return false;
  }
  
  Serial.println();
  Serial.println("████████████████████████████████████████████████████████");
  Serial.println("██                                                    ██");
  Serial.println("██        🎬🎬 オープニング演出開始 🎬🎬              ██");
  Serial.println("██                                                    ██");
  Serial.println("████████████████████████████████████████████████████████");
  Serial.println();
  
  unsigned long sequence_start = millis();
  bool success = true;
  
  // 統計リセット
  stats_.frames_displayed = 0;
  stats_.decode_time_avg = 0;
  stats_.display_time_avg = 0;
  
  // LCD明度設定
  // M5.Display.setBrightness(config_.brightness);  // M5Unifiedでサポートされていない場合はコメントアウト
  
  // フレーム順次表示
  for (uint8_t i = 0; i < frame_count_; i++) {
    unsigned long frame_start = millis();
    
    if (!displayJpegFile(frame_filenames_[i])) {
      printError("フレーム表示失敗", frame_filenames_[i]);
      success = false;
      break;
    }
    
    stats_.frames_displayed++;
    
    // フレーム表示時間調整
    unsigned long frame_time = millis() - frame_start;
    if (frame_time < config_.frame_duration_ms) {
      delay(config_.frame_duration_ms - frame_time);
    }
    
    // フレーム統計更新
    unsigned long total_frame_time = millis() - frame_start;
    logFrameInfo(frame_filenames_[i], frame_time, total_frame_time);
  }
  
  stats_.total_play_time = millis() - sequence_start;
  stats_.last_play_success = success;
  
  if (success) {
    Serial.println();
    Serial.println("████████████████████████████████████████████████████████");
    Serial.println("██                                                    ██");
    Serial.println("██       ✅✅ オープニング演出完了 ✅✅               ██");
    Serial.println("██                                                    ██");
    Serial.println("████████████████████████████████████████████████████████");
    Serial.printf("██ 総再生時間: %lums | 表示フレーム数: %d              ██\n", 
                  stats_.total_play_time, stats_.frames_displayed);
    Serial.println("████████████████████████████████████████████████████████");
    Serial.println();
  }
  
  // 最終フレーム後の小休止
  delay(500);
  
  return success;
}

/**
 * @brief オープニングスキップ
 */
void OpeningDisplay::skipOpening() {
  // 現在は特別な処理なし（将来的にはスキップ処理追加可能）
  Serial.println("OpeningDisplay: スキップ実行");
}

/**
 * @brief JPEG decoder初期化
 */
bool OpeningDisplay::setupJpegDecoder() {
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
bool OpeningDisplay::displayJpegFile(const char* filename) {
  if (!SPIFFS.exists(filename)) {
    printError("ファイル未発見", filename);
    return false;
  }
  
  unsigned long decode_start = millis();
  
  // JPEG decode & display
  uint16_t result = TJpgDec.drawSdJpg(0, 0, filename, SPIFFS);
  
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
bool OpeningDisplay::tjpgOutputCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (g_opening_display_instance == nullptr) {
    return false;
  }
  
  // M5Display に直接描画
  M5.Display.pushImage(x, y, w, h, bitmap);
  
  return true;
}

/**
 * @brief 画像ファイル存在確認
 */
bool OpeningDisplay::checkImageFiles() {
  Serial.println("OpeningDisplay: 画像ファイル確認中...");
  
  uint8_t found_files = 0;
  
  for (uint8_t i = 0; i < frame_count_; i++) {
    if (SPIFFS.exists(frame_filenames_[i])) {
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
    // 部分的でも動作継続
  }
  
  Serial.printf("✅ 画像ファイル確認完了: %d/%d見つかった\n", found_files, frame_count_);
  return true;
}

/**
 * @brief ファイルサイズ取得
 */
size_t OpeningDisplay::getFileSize(const char* filename) {
  File file = SPIFFS.open(filename, "r");
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
void OpeningDisplay::logFrameInfo(const char* filename, unsigned long decode_time, unsigned long display_time) {
  Serial.printf("🎬 フレーム表示: %s | decode: %lums | total: %lums\n", 
                filename, decode_time, display_time);
}

/**
 * @brief エラー出力
 */
void OpeningDisplay::printError(const char* message, const char* detail) {
  Serial.printf("❌ OpeningDisplay: %s", message);
  if (detail != nullptr) {
    Serial.printf(" - %s", detail);
  }
  Serial.println();
}

/**
 * @brief 統計情報出力
 */
void OpeningDisplay::printStats() const {
  Serial.println();
  Serial.println("========== オープニング統計 ==========");
  Serial.printf("総再生時間: %lums\n", stats_.total_play_time);
  Serial.printf("表示フレーム数: %d\n", stats_.frames_displayed);
  Serial.printf("平均デコード時間: %lums\n", stats_.decode_time_avg);
  Serial.printf("平均表示時間: %lums\n", stats_.display_time_avg);
  Serial.printf("最終再生結果: %s\n", stats_.last_play_success ? "成功" : "失敗");
  Serial.println("====================================");
  Serial.println();
}

/**
 * @brief 統計リセット
 */
void OpeningDisplay::resetStats() {
  stats_ = {0, 0, 0, 0, false};
  Serial.println("OpeningDisplay: 統計リセット完了");
}