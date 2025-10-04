#pragma once

#include <Arduino.h>
#include <driver/rmt.h>

// FastLED互換のCRGB構造体
struct CRGB {
    uint8_t r, g, b;
    
    CRGB() : r(0), g(0), b(0) {}
    CRGB(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
    
    // FastLED互換の色定数（宣言のみ）
    static const CRGB Red;
    static const CRGB Green;
    static const CRGB Blue;
    static const CRGB White;
    static const CRGB Black;
    
    // 演算子オーバーロード
    bool operator==(const CRGB& other) const {
        return r == other.r && g == other.g && b == other.b;
    }
    
    bool operator!=(const CRGB& other) const {
        return !(*this == other);
    }
};

/**
 * @brief FastLED代替のWS2812B LED制御クラス
 * 
 * ESP32のRMT（Remote Control Transceiver）ペリフェラルを使用して
 * WS2812B LEDストリップを制御します。
 */
class SphereStripController {
private:
    CRGB* leds;                    // LEDカラーバッファ
    uint16_t num_leds;             // LED数
    gpio_num_t data_pin;           // データピン
    rmt_channel_t rmt_channel;     // RMTチャンネル
    bool initialized;              // 初期化フラグ
    uint8_t brightness;            // 輝度 (0-255)
    
    // WS2812Bタイミング設定（80MHz基準）
    static const uint32_t T0H_NS = 350;   // 0bit high time
    static const uint32_t T0L_NS = 800;   // 0bit low time  
    static const uint32_t T1H_NS = 700;   // 1bit high time
    static const uint32_t T1L_NS = 600;   // 1bit low time
    static const uint32_t RES_NS = 50000; // Reset time
    
    /**
     * @brief RMTチャンネルの設定
     * @return true 成功, false 失敗
     */
    bool configureRMT();
    
    /**
     * @brief CRGB色をWS2812B用RMTデータに変換
     * @param color RGB色
     * @param rmt_data 出力先RMTデータ配列（24要素）
     */
    void colorToRMT(const CRGB& color, rmt_item32_t* rmt_data);

public:
    /**
     * @brief コンストラクタ
     */
    SphereStripController();
    
    /**
     * @brief デストラクタ
     */
    ~SphereStripController();
    
    /**
     * @brief LED制御システムの初期化
     * @param pin データピン番号
     * @param num LED数
     * @return true 成功, false 失敗
     */
    bool initialize(gpio_num_t pin, uint16_t num);
    
    /**
     * @brief 指定LEDの色を設定
     * @param index LED番号 (0から始まる)
     * @param color RGB色
     * @return true 成功, false 失敗（範囲外）
     */
    bool setLedColor(uint16_t index, const CRGB& color);
    
    /**
     * @brief 指定LEDの色を取得
     * @param index LED番号
     * @return RGB色（範囲外の場合は黒）
     */
    CRGB getLedColor(uint16_t index) const;
    
    /**
     * @brief 全LEDをクリア（黒に設定）
     */
    void clear();
    
    /**
     * @brief 設定した色を物理LEDに反映
     * @return true 成功, false 失敗
     */
    bool show();
    
    /**
     * @brief 輝度設定
     * @param brightness 輝度値 (0-255)
     */
    void setBrightness(uint8_t brightness);
    
    /**
     * @brief 現在の輝度取得
     * @return 輝度値 (0-255)
     */
    uint8_t getBrightness() const;
    
    /**
     * @brief LED数取得
     * @return LED数
     */
    uint16_t getNumLeds() const;
    
    /**
     * @brief データピン取得
     * @return データピン番号
     */
    gpio_num_t getDataPin() const;
    
    /**
     * @brief 初期化状態確認
     * @return true 初期化済み, false 未初期化
     */
    bool isInitialized() const;
};