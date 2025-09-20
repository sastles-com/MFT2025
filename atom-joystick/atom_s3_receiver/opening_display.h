/**
 * @file opening_display.h
 * @brief 起動時オープニング画像表示システム
 * @description SPIFFS内JPEG画像を連続表示してオープニング演出
 */

#pragma once
#include <Arduino.h>
#include <M5Unified.h>
#include <SPIFFS.h>
#include <TJpg_Decoder.h>
#include "config_manager.h"

/**
 * @brief オープニング表示設定
 */
struct OpeningConfig {
  bool enabled;                    // オープニング有効/無効
  uint16_t frame_duration_ms;      // 1フレーム表示時間（ミリ秒）
  uint8_t brightness;              // LCD明度 (0-255)
  bool fade_effect;                // フェード効果有効/無効
  uint8_t fade_steps;              // フェードステップ数
  
  // デフォルト設定
  OpeningConfig() 
    : enabled(true)
    , frame_duration_ms(400)  // 400ms/フレーム
    , brightness(200)
    , fade_effect(false)      // 初期実装ではフェード無し
    , fade_steps(10) {}
};

/**
 * @brief オープニング表示統計
 */
struct OpeningStats {
  unsigned long total_play_time;   // 総再生時間
  uint8_t frames_displayed;        // 表示フレーム数
  unsigned long decode_time_avg;   // 平均デコード時間
  unsigned long display_time_avg;  // 平均表示時間
  bool last_play_success;          // 最終再生成功/失敗
};

/**
 * @brief オープニング表示クラス
 */
class OpeningDisplay {
public:
  OpeningDisplay();
  ~OpeningDisplay();
  
  // 初期化・終了
  bool begin(const ConfigManager& config);
  void end();
  
  // オープニング再生
  bool playOpeningSequence();
  void skipOpening();
  
  // 設定更新
  void updateConfig(const OpeningConfig& config);
  OpeningConfig getConfig() const { return config_; }
  
  // 統計情報
  const OpeningStats& getStats() const { return stats_; }
  void printStats() const;
  void resetStats();

private:
  OpeningConfig config_;
  OpeningStats stats_;
  bool initialized_;
  
  // フレーム管理
  static const uint8_t MAX_FRAMES = 10;
  const char* frame_filenames_[MAX_FRAMES] = {
    "/images/flare-01.jpg",
    "/images/flare-02.jpg", 
    "/images/flare-03.jpg",
    "/images/flare-04.jpg",
    "/images/flare-05.jpg",
    "/images/flare-06.jpg"
  };
  uint8_t frame_count_;
  
  // JPEG decoder設定
  static bool tjpgOutputCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
  bool setupJpegDecoder();
  
  // 内部処理メソッド
  bool displayJpegFile(const char* filename);
  bool loadAndDecodeJpeg(const char* filename);
  void displayFrame(uint16_t frame_index);
  void applyFadeEffect(uint8_t alpha);
  
  // ファイルシステム処理
  bool checkImageFiles();
  size_t getFileSize(const char* filename);
  
  // デバッグ・ログ
  void logFrameInfo(const char* filename, unsigned long decode_time, unsigned long display_time);
  void printError(const char* message, const char* detail = nullptr);
};

// グローバル静的インスタンス（TJpg_Decoderコールバック用）
extern OpeningDisplay* g_opening_display_instance;