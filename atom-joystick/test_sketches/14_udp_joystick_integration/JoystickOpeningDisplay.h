/**
 * @file JoystickOpeningDisplay.h
 * @brief Atom-JoyStick起動時オープニング画像表示システム
 * @description LittleFS内JPEG画像を連続表示してオープニング演出
 */

#pragma once
#include <Arduino.h>
#include <M5Unified.h>
#include <LittleFS.h>
#include <TJpg_Decoder.h>

/**
 * @brief Atom-JoyStick オープニング表示設定
 */
struct JoystickOpeningConfig {
  bool enabled;                    // オープニング有効/無効
  uint16_t frame_duration_ms;      // 1フレーム表示時間（ミリ秒）
  uint8_t brightness;              // LCD明度 (0-255)
  bool show_progress;              // プログレス表示有効/無効
  uint8_t fade_steps;              // フェードステップ数（将来機能）
  
  // デフォルト設定
  JoystickOpeningConfig() 
    : enabled(true)
    , frame_duration_ms(350)  // 350ms/フレーム（少し高速）
    , brightness(200)
    , show_progress(true)     // プログレス表示有効
    , fade_steps(8) {}
};

/**
 * @brief オープニング統計
 */
struct JoystickOpeningStats {
  unsigned long total_play_time;   // 総再生時間
  uint8_t frames_displayed;        // 表示フレーム数
  unsigned long decode_time_avg;   // 平均デコード時間
  bool last_play_success;          // 最終再生成功/失敗
  unsigned long start_time;        // 開始時刻
};

/**
 * @brief Atom-JoyStick オープニング表示クラス
 */
class JoystickOpeningDisplay {
public:
  JoystickOpeningDisplay();
  ~JoystickOpeningDisplay();
  
  // 初期化・終了
  bool begin();
  void end();
  
  // オープニング再生
  bool playOpeningSequence();
  void skipOpening();
  
  // 設定更新
  void updateConfig(const JoystickOpeningConfig& config);
  JoystickOpeningConfig getConfig() const { return config_; }
  
  // 統計情報
  const JoystickOpeningStats& getStats() const { return stats_; }
  void printStats() const;

private:
  JoystickOpeningConfig config_;
  JoystickOpeningStats stats_;
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
  bool displayJpegFile(const char* filename, uint8_t frame_index);
  bool loadAndDecodeJpeg(const char* filename);
  void showProgressBar(uint8_t frame_index, uint8_t total_frames);
  void showStartupMessage();
  void showCompletionMessage();
  
  // ファイルシステム処理
  bool checkImageFiles();
  size_t getFileSize(const char* filename);
  
  // デバッグ・ログ
  void logFrameInfo(const char* filename, uint8_t frame_index, unsigned long decode_time);
  void printError(const char* message, const char* detail = nullptr);
};

// グローバル静的インスタンス（TJpg_Decoderコールバック用）
extern JoystickOpeningDisplay* g_joystick_opening_instance;