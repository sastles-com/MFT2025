/**
 * @file FastLEDController.h
 * @brief FastLED I2S DMA制御システム
 * 
 * 4本WS2812ストリップの並列高速出力制御・輝度管理・同期制御
 */

#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include <vector>

namespace LEDSphere {

/**
 * @brief FastLED出力制御クラス
 * 
 * I2S DMA並列出力・フレームバッファ管理・同期制御を担当
 */
class FastLEDController {
public:
    static constexpr size_t LED_COUNT = 800;
    static constexpr size_t STRIP_COUNT = 4;
    static constexpr size_t LEDS_PER_STRIP = 200;
    
    // GPIO設定
    static constexpr uint8_t LED_PINS[STRIP_COUNT] = {5, 6, 7, 8};

private:
    // LEDフレームバッファ
    CRGB leds_[LED_COUNT];
    
    // 制御パラメータ
    uint8_t globalBrightness_;
    uint8_t targetFPS_;
    bool initialized_;
    bool isDirty_;              // フレームバッファ更新フラグ
    bool enableGammaCorrection_;
    bool enableDithering_;
    
    // I2S DMA設定
    bool i2sEnabled_;
    bool dmaBurstMode_;
    
    // フレーム制御
    uint32_t lastShowTime_;
    uint32_t frameInterval_;    // フレーム間隔（ミリ秒）
    uint32_t showCount_;
    
    // パフォーマンス統計
    uint32_t totalShowTime_;
    uint32_t maxShowTime_;
    uint32_t minShowTime_;
    
    // 色補正パラメータ
    CRGB colorCorrection_;
    float temperatureK_;        // 色温度補正

public:
    FastLEDController();
    ~FastLEDController();
    
    /**
     * @brief FastLED初期化（I2S DMA設定）
     * @return 初期化成功フラグ
     */
    bool initialize();
    
    /**
     * @brief I2S DMA有効化
     * @param enable I2S使用フラグ
     * @return 設定成功フラグ
     */
    bool setI2SEnabled(bool enable);
    
    /**
     * @brief 個別LED設定
     * @param index LED番号 [0-799]
     * @param color RGB色
     */
    void setLED(uint16_t index, CRGB color);
    
    /**
     * @brief LED範囲設定
     * @param startIndex 開始LED番号
     * @param count LED数
     * @param color RGB色
     */
    void setLEDRange(uint16_t startIndex, uint16_t count, CRGB color);
    
    /**
     * @brief ストリップ別LED設定
     * @param strip ストリップ番号 [0-3]
     * @param stripIndex ストリップ内番号 [0-199]
     * @param color RGB色
     */
    void setStripLED(uint8_t strip, uint8_t stripIndex, CRGB color);
    
    /**
     * @brief 全LED消去
     */
    void clear();
    
    /**
     * @brief 全体輝度設定
     * @param brightness 輝度 [0-255]
     */
    void setBrightness(uint8_t brightness);
    
    /**
     * @brief 目標FPS設定
     * @param fps 目標フレームレート
     */
    void setTargetFPS(uint8_t fps);
    
    /**
     * @brief LED出力実行（FastLED.show()）
     * @return 出力実行時間（マイクロ秒）
     */
    uint32_t show();
    
    /**
     * @brief 同期出力実行（フレームレート制限付き）
     * @return 実際の出力実行フラグ
     */
    bool showSynced();
    
    /**
     * @brief フレームバッファ更新チェック
     * @return 更新が必要か
     */
    bool isDirty() const { return isDirty_; }
    
    /**
     * @brief フレームバッファ直接アクセス
     * @return CRGB配列へのポインタ
     */
    CRGB* getFrameBuffer() { return leds_; }
    const CRGB* getFrameBuffer() const { return leds_; }
    
    // ========== 色補正・品質制御 ==========
    
    /**
     * @brief ガンマ補正有効化
     * @param enable ガンマ補正フラグ
     */
    void setGammaCorrectionEnabled(bool enable) { enableGammaCorrection_ = enable; }
    
    /**
     * @brief ディザリング有効化
     * @param enable ディザリングフラグ
     */
    void setDitheringEnabled(bool enable) { enableDithering_ = enable; }
    
    /**
     * @brief 色温度補正設定
     * @param temperatureK 色温度（ケルビン）
     */
    void setColorTemperature(float temperatureK);
    
    /**
     * @brief 色補正マトリクス設定
     * @param correction RGB補正値
     */
    void setColorCorrection(CRGB correction) { colorCorrection_ = correction; }
    
    // ========== パフォーマンス監視 ==========
    
    /**
     * @brief 現在のFPS取得
     * @return FPS値
     */
    float getCurrentFPS() const;
    
    /**
     * @brief 最後の出力時間取得
     * @return 出力時間（マイクロ秒）
     */
    uint32_t getLastShowTime() const;
    
    /**
     * @brief 平均出力時間取得
     * @return 平均出力時間（マイクロ秒）
     */
    float getAverageShowTime() const;
    
    /**
     * @brief パフォーマンス統計リセット
     */
    void resetPerformanceStats();
    
    /**
     * @brief 統計情報出力
     */
    void printPerformanceStats() const;
    
    // ========== アクティブLED管理 ==========
    
    /**
     * @brief アクティブLED数取得
     * @return 点灯LED数（黒以外）
     */
    uint16_t getActiveLEDCount() const;
    
    /**
     * @brief 輝度分布取得
     * @param histogram 輝度ヒストグラム出力（256要素）
     */
    void getBrightnessHistogram(uint32_t histogram[256]) const;
    
    /**
     * @brief 色分布取得
     * @param redHist,greenHist,blueHist RGB各成分ヒストグラム（256要素）
     */
    void getColorHistogram(uint32_t redHist[256], uint32_t greenHist[256], uint32_t blueHist[256]) const;
    
    // ========== デバッグ・テスト ==========
    
    /**
     * @brief テストパターン表示
     * @param pattern テストパターン種別
     */
    void showTestPattern(const char* pattern);
    
    /**
     * @brief LED個別テスト
     * @param ledIndex LED番号
     * @param duration 点灯時間（ミリ秒）
     */
    void testSingleLED(uint16_t ledIndex, uint32_t duration = 500);
    
    /**
     * @brief ストリップテスト
     * @param strip ストリップ番号
     * @param duration 点灯時間（ミリ秒）
     */
    void testStrip(uint8_t strip, uint32_t duration = 1000);
    
    /**
     * @brief 連続点灯テスト（LED配置確認用）
     * @param delay LED間遅延（ミリ秒）
     */
    void runContinuityTest(uint32_t delay = 50);
    
    /**
     * @brief システム設定出力
     */
    void printSystemInfo() const;
    
    /**
     * @brief メモリ使用量取得
     * @return メモリ使用量（バイト）
     */
    size_t getMemoryUsage() const;

private:
    /**
     * @brief I2S DMA設定適用
     */
    bool configureI2S();
    
    /**
     * @brief GPIO設定
     */
    bool configureGPIO();
    
    /**
     * @brief 色補正適用
     * @param color 入力色
     * @return 補正後色
     */
    CRGB applyColorCorrection(CRGB color) const;
    
    /**
     * @brief ガンマ補正適用
     * @param color 入力色
     * @return 補正後色
     */
    CRGB applyGammaCorrection(CRGB color) const;
    
    /**
     * @brief LED番号→ストリップ情報変換
     * @param ledIndex LED番号
     * @param strip 出力ストリップ番号
     * @param stripIndex 出力ストリップ内番号
     * @return 変換成功フラグ
     */
    bool ledIndexToStrip(uint16_t ledIndex, uint8_t& strip, uint8_t& stripIndex) const;
    
    /**
     * @brief ストリップ情報→LED番号変換
     * @param strip ストリップ番号
     * @param stripIndex ストリップ内番号
     * @return LED番号
     */
    uint16_t stripToLedIndex(uint8_t strip, uint8_t stripIndex) const;
    
    /**
     * @brief フレーム間隔更新
     */
    void updateFrameInterval();
};

/**
 * @brief FastLEDテストパターン定義
 */
namespace LEDTestPatterns {
    void rainbow(CRGB* leds, size_t count, uint8_t phase = 0);
    void solid(CRGB* leds, size_t count, CRGB color);
    void gradient(CRGB* leds, size_t count, CRGB color1, CRGB color2);
    void strobe(CRGB* leds, size_t count, CRGB color, bool on);
    void breathing(CRGB* leds, size_t count, CRGB color, uint8_t phase);
    void knight_rider(CRGB* leds, size_t count, CRGB color, uint16_t position, uint8_t width = 5);
    void matrix_rain(CRGB* leds, size_t count, uint8_t phase);
    void fire_simulation(CRGB* leds, size_t count, uint8_t phase);
}

} // namespace LEDSphere