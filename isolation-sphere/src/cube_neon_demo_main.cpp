/**
 * @file cube_neon_demo_main.cpp
 * @brief CUBE-neon + RGBバッファデモ用メインファイル
 * 
 * atoms3r_bmi270環境専用のmain.cpp代替
 * cube_neon_led_demo.cppと同じ機能を提供
 */

#include <Arduino.h>
#include "math/fast_math.h"
#include <M5Unified.h>
#include <FastLED.h>
#include <LittleFS.h>

using namespace FastMath;

// RGBバッファ設定 (AGENTS.md準拠: 320x160)
#define RGB_BUFFER_WIDTH 320   // RGBバッファ幅 (ピクセル)
#define RGB_BUFFER_HEIGHT 160  // RGBバッファ高 (ピクセル)
#define RED_LINE_HEIGHT 80     // 赤ラインの高さ (ピクセル) - 中央

// RGB ピクセルデータ構造体
struct RGBPixel {
    uint8_t r, g, b;
    RGBPixel(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0) 
        : r(red), g(green), b(blue) {}
};

// LEDハードウェア設定（config.json準拠 - 4ストリップ構成）
#define LED_DATA_PIN_1 5   // GPIO 5: ストリップ1（180個LED）
#define LED_DATA_PIN_2 6   // GPIO 6: ストリップ2（220個LED）
#define LED_DATA_PIN_3 7   // GPIO 7: ストリップ3（180個LED）
#define LED_DATA_PIN_4 8   // GPIO 8: ストリップ4（220個LED）

// 各ストリップのLED数（config.json準拠）
#define LEDS_STRIP_1 180   // ストリップ1
#define LEDS_STRIP_2 220   // ストリップ2
#define LEDS_STRIP_3 220   // ストリップ3
#define LEDS_STRIP_4 180   // ストリップ4
#define TOTAL_LEDS (LEDS_STRIP_1 + LEDS_STRIP_2 + LEDS_STRIP_3 + LEDS_STRIP_4) // 合計800個

// FastLED配列
CRGB leds[TOTAL_LEDS];

// グローバル変数（setup()で使用）
bool ledCoordsLoaded = false;
bool useTestPanorama = true;  // テスト配列を使用するかのフラグ

// 前方宣言（setup()で使用）
bool loadLEDLayout(const char* csvPath);
void initializePanorama();
void initializeTestPanorama();
void useEmbeddedCoordinates();

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("========================================");
    Serial.println("CUBE-neon + RGBバッファデモ (atoms3r_bmi270)");
    Serial.println("========================================");
    
    // M5Stack初期化
    M5.begin();
    
    // FastLED初期化（config.json準拠：GPIO 5,6,7,8、可変長LED数）
    Serial.println("\n[1] FastLED初期化（config.json準拠構成）");
    
    // ストリップごとのオフセット計算
    int offset = 0;
    FastLED.addLeds<WS2812B, LED_DATA_PIN_1, GRB>(leds, offset, LEDS_STRIP_1);
    offset += LEDS_STRIP_1;
    FastLED.addLeds<WS2812B, LED_DATA_PIN_2, GRB>(leds, offset, LEDS_STRIP_2);
    offset += LEDS_STRIP_2;
    FastLED.addLeds<WS2812B, LED_DATA_PIN_3, GRB>(leds, offset, LEDS_STRIP_3);
    offset += LEDS_STRIP_3;
    FastLED.addLeds<WS2812B, LED_DATA_PIN_4, GRB>(leds, offset, LEDS_STRIP_4);
    
    FastLED.setBrightness(32);  // config.json準拠（12.5%）
    FastLED.clear();
    FastLED.show();
    
    Serial.printf("FastLED初期化完了:\n");
    Serial.printf("  ストリップ1 (GPIO %d): %d LED\n", LED_DATA_PIN_1, LEDS_STRIP_1);
    Serial.printf("  ストリップ2 (GPIO %d): %d LED\n", LED_DATA_PIN_2, LEDS_STRIP_2);
    Serial.printf("  ストリップ3 (GPIO %d): %d LED\n", LED_DATA_PIN_3, LEDS_STRIP_3);
    Serial.printf("  ストリップ4 (GPIO %d): %d LED\n", LED_DATA_PIN_4, LEDS_STRIP_4);
    Serial.printf("  合計: %d LED\n", TOTAL_LEDS);
    
    // // 簡単なLED動作テスト
    // Serial.println("\n[2] LED動作テスト");
    // for (int i = 0; i < TOTAL_LEDS; i += 10) {
    //     leds[i] = CRGB::Red;
    // }
    // FastLED.show();
    // delay(1000);
    
    FastLED.clear();
    FastLED.show();
    
    // 🎯 CUBE_neon準拠: LED座標データの読み込み
    Serial.println("\n[3] LED座標データ読み込み（CUBE_neon準拠）");
    // ledCoordsLoaded = loadLEDLayout("/data/led_layout.csv");
    ledCoordsLoaded = loadLEDLayout("led_layout.csv");
    if (!ledCoordsLoaded) {
        Serial.println("⚠️ LED座標データの読み込みに失敗しました");
        Serial.println("   パノラマサンプリングは仮想座標で動作します");
    }
    
    // 🎯 パノラマシステム初期化
    Serial.println("\n[4] パノラマシステム初期化");
    
    // テスト配列を使用する場合の初期化
    if (useTestPanorama) {
        initializeTestPanorama();
    } else {
        initializePanorama();
    }
    
    Serial.println("初期化完了 - メインループ開始");
}

// パノラマバッファのグローバル変数
uint8_t* panoramaBuffer = nullptr;
const int PANORAMA_WIDTH = 320;
const int PANORAMA_HEIGHT = 160;

// 🎯 u=0.25, u=0.75の位置計算
#define U_025_PX 80   // (int)(0.25f * 320)
#define U_075_PX 240  // (int)(0.75f * 320)

// 🎯 ピクセルが縦線上にあるかチェックするマクロ
#define IS_VERTICAL_LINE(x) (((x) >= (U_025_PX - 1) && (x) <= (U_025_PX + 1)) || \
                            ((x) >= (U_075_PX - 1) && (x) <= (U_075_PX + 1)))

// 🎯 1行分のRGBデータを生成するマクロ（320ピクセル×3バイト=960バイト）
#define ROW_DATA(y) \
    /* x=0-78: 黒 */ \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    /* x=79-81: u=0.25縦線 (0.5緑) */ \
    GREEN_HALF_R,GREEN_HALF_G,GREEN_HALF_B, GREEN_HALF_R,GREEN_HALF_G,GREEN_HALF_B, GREEN_HALF_R,GREEN_HALF_G,GREEN_HALF_B, \
    /* x=82-238: 黒 */ \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    /* x=239-241: u=0.75縦線 (0.5緑) */ \
    GREEN_HALF_R,GREEN_HALF_G,GREEN_HALF_B, GREEN_HALF_R,GREEN_HALF_G,GREEN_HALF_B, GREEN_HALF_R,GREEN_HALF_G,GREEN_HALF_B, \
    /* x=242-319: 黒 */ \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B, \
    BLACK_R,BLACK_G,BLACK_B, BLACK_R,BLACK_G,BLACK_B

// 🎯 テスト用320x160 RGB配列（for文初期化方式）
// 153,600バイト = 51,200ピクセル × 3バイト(RGB)
uint8_t testPanoramaRGB[PANORAMA_WIDTH * PANORAMA_HEIGHT * 3];

// 🎯 代替案：CRGB形式での明示的初期化（同じ内容をCRGB配列で表現）
// CRGB testPanoramaCRGB[PANORAMA_WIDTH * PANORAMA_HEIGHT] = {
//     // 320x160 = 51,200ピクセル分のCRGB::Black
//     CRGB::Black  // 最初の要素をCRGB::Blackで初期化すると、残りも自動的にCRGB::Blackになる
// };

// アニメーション用変数
float animationPhase = 0.0f;  // アニメーション位相（0-1）
const float ANIMATION_SPEED = 0.01f;  // 1フレームあたりの回転速度

// X=0付近のLED FaceID配列
const int xNearZeroFaceIDs[] = {
    7, 79, 81, 82, 87, 88, 89, 164, 165, 172, 175, 177,
    186, 187, 189, 190, 199, 381, 396, 397, 407, 479, 481, 482,
    487, 488, 489, 564, 565, 572, 575, 577, 586, 587, 589, 590,
    599, 781, 796, 797
};
const int xNearZeroCount = sizeof(xNearZeroFaceIDs) / sizeof(xNearZeroFaceIDs[0]);

// 🎯 CUBE_neon準拠: 全LED座標データ（led_layout.csvベース）
struct LEDCoord {
    int faceID;
    int strip;
    int strip_num;
    float x, y, z;
};

// 全800個のLED座標データ（実際のled_layout.csvから読み込み）
LEDCoord allLEDCoords[TOTAL_LEDS];

// IMU/オフセット回転パラメータ（CUBE_neon準拠）
struct RotationParams {
    float quaternionW, quaternionX, quaternionY, quaternionZ;  // IMU姿勢
    float latitudeOffset, longitudeOffset;                     // UI制御オフセット（度）
    
    RotationParams() : quaternionW(1.0f), quaternionX(0.0f), quaternionY(0.0f), quaternionZ(0.0f),
                      latitudeOffset(0.0f), longitudeOffset(0.0f) {}
};

RotationParams rotationParams;

// 🎯 CUBE_neon準拠: 高速計算用定数（近似計算最適化）
const float CUBE_NEON_PI = 3.14159265f;
const float CUBE_NEON_HALF_PI = 1.57079632f;
const float CUBE_NEON_TWO_PI = 6.28318530f;
const float CUBE_NEON_INV_PI = 0.31830988f;        // 1/π
const float CUBE_NEON_INV_TWO_PI = 0.15915494f;    // 1/(2π)
const float CUBE_NEON_LINEAR_THRESHOLD = 0.7f;     // 線形近似の閾値

// X=0付近のLED座標データ（参考用）
LEDCoord xNearZeroCoords[] = {
    {7, 0, 7, 0.030549123f, 0.094021168f, -0.995101387f},
    {79, 0, 79, 0.030549126f, -0.094021175f, -0.995101387f},
    {81, 0, 81, -0.02274537f, -0.291095547f, -0.956423562f},
    {82, 0, 82, 0.01352336f, -0.413422178f, -0.910439027f},
    {87, 0, 87, 0.026891631f, -0.801619553f, -0.59722938f},
    {88, 0, 88, -0.009428102f, -0.718389689f, -0.695577002f},
    {89, 0, 89, -0.047752107f, -0.616040131f, -0.786266045f},
    {164, 0, 164, -0.022745379f, 0.291095547f, -0.956423562f},
    {165, 0, 165, 0.013523353f, 0.413422149f, -0.91043904f},
    {172, 0, 172, -0.047752128f, 0.616040106f, -0.786266063f},
    {175, 0, 175, -0.009428109f, 0.718389651f, -0.695577041f},
    {177, 0, 177, 0.026891632f, 0.801619526f, -0.597229416f},
    {186, 0, 186, -0.009363985f, -0.913207209f, -0.40738791f},
    {187, 0, 187, 0.048756564f, -0.95543504f, -0.291147181f},
    {189, 0, 189, 0.036591272f, -0.997574814f, -0.059207847f},
    {190, 0, 190, -0.023806654f, -0.983374153f, -0.180023663f},
    {199, 0, 199, -0.036591265f, -0.997574814f, 0.059207847f},
    {381, 1, 181, -0.009364001f, 0.913207209f, -0.40738791f},
    {396, 1, 196, -0.023806662f, 0.983374157f, -0.180023636f},
    {397, 1, 197, 0.048756545f, 0.955435041f, -0.291147181f},
    {407, 2, 7, -0.030549123f, -0.094021168f, 0.995101387f},
    {479, 2, 79, -0.030549126f, 0.094021175f, 0.995101387f},
    {481, 2, 81, 0.02274537f, 0.291095547f, 0.956423562f},
    {482, 2, 82, -0.01352336f, 0.413422178f, 0.910439027f},
    {487, 2, 87, -0.026891631f, 0.801619553f, 0.59722938f},
    {488, 2, 88, 0.009428102f, 0.718389689f, 0.695577002f},
    {489, 2, 89, 0.047752107f, 0.616040131f, 0.786266045f},
    {564, 2, 164, 0.022745379f, -0.291095547f, 0.956423562f},
    {565, 2, 165, -0.013523353f, -0.413422149f, 0.91043904f},
    {572, 2, 172, 0.047752128f, -0.616040106f, 0.786266063f},
    {575, 2, 175, 0.009428109f, -0.718389651f, 0.695577041f},
    {577, 2, 177, -0.026891632f, -0.801619526f, 0.597229416f},
    {586, 2, 186, 0.009363985f, -0.913207209f, 0.40738791f},
    {587, 2, 187, -0.048756564f, -0.95543504f, 0.291147181f},
    {589, 2, 189, -0.036591272f, -0.997574814f, 0.059207847f},
    {590, 2, 190, 0.023806654f, -0.983374153f, 0.180023663f},
    {599, 2, 199, 0.036591265f, -0.997574814f, -0.059207847f},
    {781, 3, 181, 0.009364001f, 0.913207209f, 0.40738791f},
    {796, 3, 196, 0.023806662f, 0.983374157f, 0.180023636f},
    {797, 3, 197, -0.048756545f, 0.955435041f, 0.291147181f}
};

bool coordsInitialized = false;

// 🎯 テストパノラマ配列の初期化（for文でu=0.25/u=0.75縦線描画）
void initializeTestPanorama() {
    Serial.println("🎯 テストパノラマ配列初期化開始...");
    
    // 配列全体を黒で初期化
    memset(testPanoramaRGB, 0, sizeof(testPanoramaRGB));
    
    // � 太いライン版: LEDの実際の分布範囲に合わせて複数ピクセル幅で描画
    // 実際のLED分布データに基づいた範囲設定
    int u25_start = 74;   // u≈0.25 LED分布の開始位置
    int u25_end = 85;     // u≈0.25 LED分布の終了位置 (12ピクセル幅)
    int u75_start = 233;  // u≈0.75 LED分布の開始位置  
    int u75_end = 245;    // u≈0.75 LED分布の終了位置 (13ピクセル幅)
    
    Serial.printf("🟢 u≈0.25太いライン: x=%d～%d (%dピクセル幅)\n", u25_start, u25_end, u25_end - u25_start + 1);
    Serial.printf("🔴 u≈0.75太いライン: x=%d～%d (%dピクセル幅)\n", u75_start, u75_end, u75_end - u75_start + 1);
    
    // 全高さに渡って太い縦線を描画
    for (int y = 0; y < PANORAMA_HEIGHT; y++) {
        // u≈0.25太い緑ライン（X074～X085）
        for (int x = u25_start; x <= u25_end && x < PANORAMA_WIDTH; x++) {
            int idx = (y * PANORAMA_WIDTH + x) * 3;
            testPanoramaRGB[idx + 0] = 0;    // R
            testPanoramaRGB[idx + 1] = 255;  // G (full green)
            testPanoramaRGB[idx + 2] = 0;    // B
        }
        
        // u≈0.75太い赤ライン（X233～X245）
        for (int x = u75_start; x <= u75_end && x < PANORAMA_WIDTH; x++) {
            int idx = (y * PANORAMA_WIDTH + x) * 3;
            testPanoramaRGB[idx + 0] = 255;  // R (full red)
            testPanoramaRGB[idx + 1] = 0;    // G
            testPanoramaRGB[idx + 2] = 0;    // B
        }
    }
    
    Serial.println("✅ テストパノラマ配列初期化完了（太いライン版・100%LEDカバレッジ）");
}

// 🎯 CUBE_neon準拠: led_layout.csvから全LED座標を読み込み
bool loadLEDLayout(const char* csvPath) {
    // 通常の初期化を試行（フォーマットなし）
    if (!LittleFS.begin(false, "/littlefs", 10, "littlefs")) {
        Serial.println("LittleFS通常初期化失敗");
        Serial.println("⚠️ LittleFSが初期化されていない可能性があります");
        Serial.println("   手動でデータをアップロードしてください: pio run -e atoms3r_bmi270 --target uploadfs");
        Serial.println("⚠️ CSVファイル読み込み失敗 - 埋め込み座標データを使用");
        useEmbeddedCoordinates();
        return true;
    }
    
    File file = LittleFS.open(csvPath, "r");
    if (!file) {
        Serial.printf("LEDレイアウトファイル読み込み失敗: %s\n", csvPath);
        Serial.println("⚠️ CSVファイル読み込み失敗 - 埋め込み座標データを使用");
        useEmbeddedCoordinates();
        return true;
    }
    
    int loadedCount = 0;
    String line;
    bool firstLine = true;
    
    while (file.available() && loadedCount < TOTAL_LEDS) {
        line = file.readStringUntil('\n');
        line.trim();
        
        // ヘッダー行をスキップ
        if (firstLine) {
            firstLine = false;
            continue;
        }
        
        // CSV解析: FaceID,strip,strip_num,x,y,z
        int commaCount = 0;
        int commaPositions[5];
        for (int i = 0; i < line.length(); i++) {
            if (line[i] == ',' && commaCount < 5) {
                commaPositions[commaCount++] = i;
            }
        }
        
        if (commaCount >= 5) {
            int faceID = line.substring(0, commaPositions[0]).toInt();
            int strip = line.substring(commaPositions[0] + 1, commaPositions[1]).toInt();
            int strip_num = line.substring(commaPositions[1] + 1, commaPositions[2]).toInt();
            float x = line.substring(commaPositions[2] + 1, commaPositions[3]).toFloat();
            float y = line.substring(commaPositions[3] + 1, commaPositions[4]).toFloat();
            float z = line.substring(commaPositions[4] + 1).toFloat();
            
            if (faceID >= 0 && faceID < TOTAL_LEDS) {
                allLEDCoords[faceID] = {faceID, strip, strip_num, x, y, z};
                loadedCount++;
            }
        }
    }
    
    file.close();
    ledCoordsLoaded = (loadedCount == TOTAL_LEDS);
    
    Serial.printf("✅ LEDレイアウト読み込み: %d/%d個\n", loadedCount, TOTAL_LEDS);
    
    // CSV読み込み失敗時は埋め込み座標データを使用
    if (!ledCoordsLoaded) {
        Serial.println("⚠️ CSVファイル読み込み失敗 - 埋め込み座標データを使用");
        useEmbeddedCoordinates();
        ledCoordsLoaded = true;
    }
    
    return ledCoordsLoaded;
}

// 🎯 埋め込み座標データの使用（CSV読み込み失敗時のfallback）
void useEmbeddedCoordinates() {

    Serial.println("useEmbeddedCoordinates :::::: 🎯 埋め込み座標データを使用して全LED座標を初期化...");
    // まず全LEDを識別可能な値で初期化 (faceID=-1で未設定をマーク)
    for (int i = 0; i < TOTAL_LEDS; i++) {
        allLEDCoords[i] = {-1, 0, i, 0.0f, 0.0f, 0.0f};
    }
    
    // 埋め込み座標データを適用
    int embeddedCount = sizeof(xNearZeroCoords) / sizeof(xNearZeroCoords[0]);
    for (int i = 0; i < embeddedCount; i++) {
        LEDCoord coord = xNearZeroCoords[i];
        if (coord.faceID >= 0 && coord.faceID < TOTAL_LEDS) {
            allLEDCoords[coord.faceID] = coord;
        }
    }
    
    // デモ用に他のLEDにランダムな球面座標を生成
    for (int i = 0; i < TOTAL_LEDS; i++) {
        if (allLEDCoords[i].faceID == -1) {  // 未設定のLEDのみ
            // 球面上のランダムな点を生成
            float theta = random(0, 3600) * 0.001f;  // 0-3.6ラジアン (0-206度相当)
            float phi = random(0, 6283) * 0.001f;    // 0-6.283ラジアン (0-360度)
            
            float x = sin(theta) * cos(phi);
            float y = sin(theta) * sin(phi);
            float z = cos(theta);
            
            allLEDCoords[i] = {i, i/200, i%200, x, y, z};
        }
    }
    
    Serial.printf("✅ 埋め込み座標データ使用: %d個の既知座標 + %d個の生成座標\n", embeddedCount, TOTAL_LEDS - embeddedCount);
}

// 🎯 CUBE_neon準拠: IMU/オフセット回転を適用
void applyRotation(float& x, float& y, float& z, const RotationParams& params) {
    // クォータニオン回転（IMU姿勢）
    float qw = params.quaternionW, qx = params.quaternionX, qy = params.quaternionY, qz = params.quaternionZ;
    
    // クォータニオン → 回転行列変換
    float xx = qx * qx, yy = qy * qy, zz = qz * qz;
    float xy = qx * qy, xz = qx * qz, yz = qy * qz;
    float wx = qw * qx, wy = qw * qy, wz = qw * qz;
    
    float rotX = x * (1 - 2 * (yy + zz)) + y * 2 * (xy - wz) + z * 2 * (xz + wy);
    float rotY = x * 2 * (xy + wz) + y * (1 - 2 * (xx + zz)) + z * 2 * (yz - wx);
    float rotZ = x * 2 * (xz - wy) + y * 2 * (yz + wx) + z * (1 - 2 * (xx + yy));
    
    // 緯度・経度オフセット回転（度→ラジアン変換）
    float latRad = params.latitudeOffset * PI / 180.0f;
    float lonRad = params.longitudeOffset * PI / 180.0f;
    
    // Y軸回転（緯度オフセット）
    float tempX = rotX * cosf(latRad) + rotZ * sinf(latRad);
    float tempZ = -rotX * sinf(latRad) + rotZ * cosf(latRad);
    rotX = tempX;
    rotZ = tempZ;
    
    // Z軸回転（経度オフセット）
    tempX = rotX * cosf(lonRad) - rotY * sinf(lonRad);
    float tempY = rotX * sinf(lonRad) + rotY * cosf(lonRad);
    rotX = tempX;
    rotY = tempY;
    
    x = rotX;
    y = rotY;
    z = rotZ;
}

// 🎯 CUBE_neon準拠: 高速計算ヘルパー関数
// 高速平方根近似（Newton-Raphson 1回反復）
inline float fastSqrt(float x) {
    if (x <= 0.0f) return 0.0f;
    
    // 初期推定値（bit manipulation）
    union { float f; uint32_t i; } u;
    u.f = x;
    u.i = (u.i >> 1) + 0x1fbb67a8;  // Magic number for sqrt approximation
    
    // Newton-Raphson 1回反復で精度向上
    u.f = 0.5f * (u.f + x / u.f);
    
    return u.f;
}

// 高速逆平方根（1/sqrt(x)）- CUBE_neonでベクトル正規化に使用
inline float fastInvSqrt(float x) {
    if (x <= 0.0f) return 0.0f;
    
    union { float f; uint32_t i; } u;
    u.f = x;
    u.i = 0x5f3759df - (u.i >> 1);  // Quake III algorithm
    
    // Newton-Raphson 1回反復
    u.f = u.f * (1.5f - 0.5f * x * u.f * u.f);
    
    return u.f;
}

// 🎯 CUBE_neon準拠: 球面座標→UV座標変換（高速近似計算）
// 標準的な球面座標変換（検証用）
void sphericalToUV_Standard(float x, float y, float z, float& u, float& v) {
    // 座標正規化
    float length = sqrt(x*x + y*y + z*z);
    if (length == 0) {
        u = v = 0.5f;
        return;
    }
    
    x /= length;
    y /= length;
    z /= length;
    
    // 標準的な球面座標変換
    float longitude = atan2(z, x);          // -π to π
    float latitude = asin(y);               // -π/2 to π/2
    
    // UV正規化 [0, 1]
    u = (longitude + PI) / (2.0f * PI);     // 0 to 1
    v = (latitude + PI/2.0f) / PI;          // 0 to 1
    
    // 境界クランプ
    u = constrain(u, 0.0f, 1.0f);
    v = constrain(v, 0.0f, 1.0f);
}

void sphericalToUV(float x, float y, float z, float& u, float& v) {
    // 🚨 DEBUGGING: 近似版から標準版に切り替え
    sphericalToUV_Standard(x, y, z, u, v);
    return;
    
    // CUBE_neonの近似計算手法：
    // 1. atan2の代わりに高速近似を使用
    // 2. 正規化済み座標(x,y,z)前提（||(x,y,z)|| = 1）
    // 3. パノラマ360度展開に最適化
    // 4. 高速平方根・逆三角関数近似
    
    // 🔹 経度計算（X-Z平面投影）- CUBE_neon近似版
    // atan2(z, x)の高速近似：符号判定 + 線形補間
    float longitude;
    float abs_x = (x >= 0) ? x : -x;
    float abs_z = (z >= 0) ? z : -z;
    
    if (abs_x > abs_z) {
        // X軸寄り：atan(z/x)近似
        float ratio = z / x;
        longitude = ratio * CUBE_NEON_PI * 0.25f;  // π/4近似
        if (x < 0) longitude += CUBE_NEON_PI;      // 第2,3象限補正
    } else {
        // Z軸寄り：π/2 - atan(x/z)近似
        float ratio = (abs_z > 0.001f) ? (x / z) : 0.0f;
        longitude = CUBE_NEON_HALF_PI - ratio * CUBE_NEON_PI * 0.25f;
        if (z < 0) longitude += CUBE_NEON_PI;      // 第3,4象限補正
    }
    
    // 🔹 緯度計算（Y軸投影）- CUBE_neon高速近似版
    // atan2(y, sqrt(x*x + z*z))の高速近似
    float xz_length_sq = x*x + z*z;
    float latitude;
    
    if (xz_length_sq > 0.000001f) {
        // 高速平方根近似使用
        float xz_length = fastSqrt(xz_length_sq);
        float y_ratio = y / xz_length;
        
        // 小角度近似 vs 正確計算の判定
        float abs_y_ratio = (y_ratio >= 0) ? y_ratio : -y_ratio;
        if (abs_y_ratio < CUBE_NEON_LINEAR_THRESHOLD) {
            // 線形近似: sin(θ) ≈ θ for small θ
            latitude = y_ratio * CUBE_NEON_HALF_PI;
        } else {
            // 大角度：正確な計算
            latitude = atan2(y, xz_length);
        }
    } else {
        // 極点処理
        latitude = (y > 0) ? CUBE_NEON_HALF_PI : -CUBE_NEON_HALF_PI;
    }
    
    // 🔹 UV正規化 [0, 1] - CUBE_neon最適化版
    u = (longitude + CUBE_NEON_PI) * CUBE_NEON_INV_TWO_PI;  // 高速除算回避
    v = (latitude + CUBE_NEON_HALF_PI) * CUBE_NEON_INV_PI;  // 高速除算回避
    
    // 境界クランプ（CUBE_neonでの安全措置）
    u = constrain(u, 0.0f, 1.0f);
    v = constrain(v, 0.0f, 1.0f);
}

// X=0付近判定用：より適切な経度チェック
bool isNearXZero(float x, float y, float z, float threshold = 0.1f) {
    // 正規化
    float length = sqrt(x*x + y*y + z*z);
    if (length == 0) return false;
    
    x /= length; y /= length; z /= length;
    
    // X=0平面（YZ平面）からの距離
    return abs(x) < threshold;
}

// パノラマ画像に緑色ピクセルを描画（3x3）
void drawGreenPixelAt(float u, float v, int width, int height) {
    if (!panoramaBuffer) return;
    
    int px = (int)(u * (width - 1));
    int py = (int)(v * (height - 1));
    
    px = constrain(px, 0, width - 1);
    py = constrain(py, 0, height - 1);
    
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int drawX = px + dx;
            int drawY = py + dy;
            
            if (drawX >= 0 && drawX < width && drawY >= 0 && drawY < height) {
                int pixelIndex = (drawY * width + drawX) * 3;
                panoramaBuffer[pixelIndex + 0] = 0;    // R
                panoramaBuffer[pixelIndex + 1] = 255;  // G（緑色）
                panoramaBuffer[pixelIndex + 2] = 0;    // B
            }
        }
    }
}

// パノラマ画像に青色ピクセルを描画（理論位置用）
void drawBluePixelAt(float u, float v, int width, int height) {
    if (!panoramaBuffer) return;
    
    int px = (int)(u * (width - 1));
    int py = (int)(v * (height - 1));
    
    px = constrain(px, 0, width - 1);
    py = constrain(py, 0, height - 1);
    
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int drawX = px + dx;
            int drawY = py + dy;
            
            if (drawX >= 0 && drawX < width && drawY >= 0 && drawY < height) {
                int pixelIndex = (drawY * width + drawX) * 3;
                // 既に他の色が設定されていない場合のみ青色を設定
                if (panoramaBuffer[pixelIndex + 1] == 0) {  // 緑でない場合
                    panoramaBuffer[pixelIndex + 0] = 0;    // R
                    panoramaBuffer[pixelIndex + 1] = 0;    // G
                    panoramaBuffer[pixelIndex + 2] = 255;  // B（青色）
                }
            }
        }
    }
}

// 指定緯度にリング状の緑線を描画
void drawGreenRingAtLatitude(float v, int width, int height, int thickness) {
    if (!panoramaBuffer) return;
    
    int py = (int)(v * (height - 1));
    py = constrain(py, 0, height - 1);
    
    // 緯度線全体（U=0からU=1）に緑色のリングを描画
    for (int px = 0; px < width; px++) {
        for (int dy = -(thickness/2); dy <= (thickness/2); dy++) {
            int drawY = py + dy;
            if (drawY >= 0 && drawY < height) {
                int pixelIndex = (drawY * width + px) * 3;
                
                // 緑色で描画（他の色を上書き）
                panoramaBuffer[pixelIndex + 0] = 0;    // R
                panoramaBuffer[pixelIndex + 1] = 255;  // G（緑色）
                panoramaBuffer[pixelIndex + 2] = 0;    // B
            }
        }
    }
}

// 緑リングのFaceID座標をパノラマ画像に描画（リング形状）
void drawFaceIDCoordinatesToPanorama() {
    if (!panoramaBuffer) return;
    
    // Serial.println("=== テスト用RGB配列: 全て黒(0,0,0)で初期化 ===");
    
    // 🎯 テスト配列を黒で初期化（背景色設定をスキップ）
    /*
    if (useTestPanorama) {
        // 静的配列なので直接アクセス可能
        for (int i = 0; i < PANORAMA_WIDTH * PANORAMA_HEIGHT; i++) {
            int pixelIndex = i * 3;
            testPanoramaRGB[pixelIndex + 0] = 10;   // R
            testPanoramaRGB[pixelIndex + 1] = 5;    // G  
            testPanoramaRGB[pixelIndex + 2] = 15;   // B
        }
    } else {
        // 従来の初期化方法
        for (int i = 0; i < PANORAMA_WIDTH * PANORAMA_HEIGHT; i++) {
            int pixelIndex = i * 3;
            panoramaBuffer[pixelIndex + 0] = 10;
            panoramaBuffer[pixelIndex + 1] = 5;
            panoramaBuffer[pixelIndex + 2] = 15;
        }
    }
    */
    
    // 🎯 u=0.25とu=0.75の縦線描画もスキップ
    /*
    int u_positions[] = {
        (int)(0.25f * PANORAMA_WIDTH),  // u=0.25 → ピクセル位置
        (int)(0.75f * PANORAMA_WIDTH)   // u=0.75 → ピクセル位置
    };
    
    for (int i = 0; i < 2; i++) {
        int px = u_positions[i];
        px = constrain(px, 0, PANORAMA_WIDTH - 1);
        
        // 縦線全体（v=0からv=1）に0.5緑色を描画
        for (int py = 0; py < PANORAMA_HEIGHT; py++) {
            // 3ピクセル幅の太い縦線
            for (int dx = -1; dx <= 1; dx++) {
                int drawX = px + dx;
                if (drawX >= 0 && drawX < PANORAMA_WIDTH) {
                    int pixelIndex = (py * PANORAMA_WIDTH + drawX) * 3;
                    
                    // 0.5緑色で描画（RGB: 0, 127, 0）
                    panoramaBuffer[pixelIndex + 0] = 0;    // R
                    panoramaBuffer[pixelIndex + 1] = 127;  // G（0.5緑色）
                    panoramaBuffer[pixelIndex + 2] = 0;    // B
                }
            }
        }
        
        Serial.printf("縦線[%d]: u=%.2f → px=%d (0.5緑色)\n", 
                     i, (i == 0) ? 0.25f : 0.75f, px);
    }
    
    // 📷 パノラマ画像作成：0.5緑色のリング（X軸大円に対応する緯度リング）- アニメーション版
    //
    // 🆚 CUBE_neonとの比較：
    // ┌─────────────────┬────────────────────┬──────────────────────┐
    // │ 項目             │ CUBE_neon          │ このコード           │
    // ├─────────────────┼────────────────────┼──────────────────────┤
    // │ 描画パターン     │ 縦線（経度線）     │ 水平線（緯度線）     │
    // │ アニメーション   │ U座標回転          │ V座標振動            │
    // │ サンプリング軸   │ 経度方向           │ 緯度方向             │
    // │ リング形状       │ 大円（縦）         │ 小円（横）           │
    // │ 色               │ 赤色               │ 0.5緑色             │
    // └─────────────────┴────────────────────┴──────────────────────┘
    //
    // 🔄 パノラマ描画手順：
    // Step 1: アニメーション位相更新
    // Step 2: V座標（緯度）計算 
    // Step 3: 水平線全体に0.5緑色描画
    // Step 4: 太さ調整（7ピクセル太）
    
    // Y軸周りに回転するリングのアニメーション
    // 基準リング位置: 赤道周辺の緯度帯
    // アニメーション: V座標（緯度）を連続的に変化させる
    
    // アニメーション位相を更新（0-1の範囲で循環）
    animationPhase += ANIMATION_SPEED;
    if (animationPhase >= 1.0f) {
        animationPhase = 0.0f;
    }
    
    // 回転による V座標の計算（緯度アニメーション）
    // animationPhase=0: 赤道周辺（V=0.5）
    // animationPhase=0.5: 北極・南極周辺（V=0.0, V=1.0）
    float v1 = 0.5f + 0.3f * sinf(animationPhase * 2.0f * PI);  // 第1のリング（赤道周辺を振動）
    float v2 = 0.5f + 0.3f * sinf((animationPhase + 0.5f) * 2.0f * PI);  // 第2のリング（位相差180度）
    
    // リング描画（水平線）
    for (int i = 0; i < 2; i++) {
        float v_ring = (i == 0) ? v1 : v2;
        int py = (int)(v_ring * (PANORAMA_HEIGHT - 1));
        py = constrain(py, 0, PANORAMA_HEIGHT - 1);
        
        // 水平リング全体に0.5緑色を描画（太い線）
        for (int px = 0; px < PANORAMA_WIDTH; px++) {
            for (int dy = -3; dy <= 3; dy++) {  // 7ピクセル太さ（緑リングより太く）
                int test_py = py + dy;
                if (test_py >= 0 && test_py < PANORAMA_HEIGHT) {
                    int pixelIndex = (test_py * PANORAMA_WIDTH + px) * 3;
                    
                    // 既に緑色が描画されていない場合のみ0.5緑色を描画
                    if (panoramaBuffer[pixelIndex + 1] < 200) {  // 緑が薄い場合のみ
                        float intensity = 1.0f - (abs(dy) / 3.0f);  // 中央が最も明るい
                        panoramaBuffer[pixelIndex + 0] = 0;    // R
                        panoramaBuffer[pixelIndex + 1] = (uint8_t)(127 * intensity);  // G（0.5緑色）
                        panoramaBuffer[pixelIndex + 2] = 0;    // B
                    }
                }
            }
        }
    }
    
    // デバッグ情報をシリアルに出力（毎フレーム出力に変更）
    static int frame_count = 0;
    frame_count++;
    if (frame_count % 30 == 0) {  // 30フレームごと（約0.5秒間隔）に出力
        Serial.printf("🔄 Animation: Phase=%.3f, V1=%.3f (py=%d), V2=%.3f (py=%d)\n", 
                     animationPhase, v1, (int)(v1 * PANORAMA_HEIGHT), 
                     v2, (int)(v2 * PANORAMA_HEIGHT));
    }
    
    // 比較用の縦線を追加：X軸大円の理論位置に青い縦線
    // X軸大円: U=80, U=240 (経度90度、270度)
    // Y軸大円: U=0, U=160 (経度0度、180度) 
    int test_u_pixels[] = {80, 240};  // X軸大円の理論位置
    // 比較用の縦線を追加：X軸大円の理論位置に青い縦線
    // X軸大円: U=80, U=240 (経度90度、270度)
    // Y軸大円: U=0, U=160 (経度0度、180度) 
    int x_axis_pixels[] = {80, 240};   // X軸大円の理論位置
    int y_axis_pixels[] = {0, 160};    // Y軸大円の理論位置（参考用）
    
    // X軸大円の縦線（青色）
    for (int i = 0; i < 2; i++) {
        int test_px = x_axis_pixels[i];
        for (int py = 0; py < PANORAMA_HEIGHT; py++) {
            for (int dx = -1; dx <= 1; dx++) {  // 3ピクセル幅
                int px = test_px + dx;
                if (px >= 0 && px < PANORAMA_WIDTH) {
                    int pixelIndex = (py * PANORAMA_WIDTH + px) * 3;
                    
                    // 既に他の色が設定されていない場合のみ青色を設定
                    if (panoramaBuffer[pixelIndex + 1] == 0 && panoramaBuffer[pixelIndex + 0] == 0) {
                        panoramaBuffer[pixelIndex + 0] = 0;    // R
                        panoramaBuffer[pixelIndex + 1] = 0;    // G
                        panoramaBuffer[pixelIndex + 2] = 200;  // B（青色）
                    }
                }
            }
        }
    }
    
    // Y軸大円の縦線（シアン色 - 参考用）
    for (int i = 0; i < 2; i++) {
        int test_px = y_axis_pixels[i];
        for (int py = 0; py < PANORAMA_HEIGHT; py++) {
            for (int dx = -1; dx <= 1; dx++) {  // 3ピクセル幅
                int px = test_px + dx;
                if (px >= 0 && px < PANORAMA_WIDTH) {
                    int pixelIndex = (py * PANORAMA_WIDTH + px) * 3;
                    
                    // 既に他の色が設定されていない場合のみシアン色を設定
                    if (panoramaBuffer[pixelIndex + 1] == 0 && panoramaBuffer[pixelIndex + 0] == 0 && panoramaBuffer[pixelIndex + 2] == 0) {
                        panoramaBuffer[pixelIndex + 0] = 0;    // R
                        panoramaBuffer[pixelIndex + 1] = 200;  // G（シアン色）
                        panoramaBuffer[pixelIndex + 2] = 200;  // B
                    }
                }
            }
        }
    }
    
    Serial.printf("パノラマ画像に%d個のFaceID座標を描画完了\n", xNearZeroCount);
    Serial.println("描画内容:");
    Serial.println("- 緑色: FaceID座標（X=0付近の緯度リング）");
    Serial.println("- 0.5緑色: 緯度方向にアニメーションするリング");
    Serial.println("- 青色: X軸大円の理論位置（確認用）");
    Serial.println("- シアン: Y軸大円の理論位置（U=0.0, U=0.5）");
    Serial.println("アニメーション効果:");
    Serial.printf("- 0.5緑色リングが緯度方向に振動\n");
    Serial.printf("- 緑リング通過時に重複表示（黄色）\n");
    Serial.printf("- 振動周期: %.1f秒（phase=0→1）\n", 1.0f / ANIMATION_SPEED / 60.0f);
    */
    
    // Serial.println("パノラマ生成スキップ - 全て黒(0,0,0)で初期化済み");
}

// パノラマ画像をPPM形式でLittleFSに保存
bool savePanoramaImageAsPPM(const char* filename) {
    if (!panoramaBuffer) {
        Serial.println("パノラマバッファが存在しません");
        return false;
    }
    
    // LittleFS初期化
    if (!LittleFS.begin(false, "/littlefs", 10, "littlefs")) {
        Serial.println("LittleFS初期化失敗");
        return false;
    }
    
    // ファイルを書き込みモードで開く
    File file = LittleFS.open(filename, "w");
    if (!file) {
        Serial.printf("ファイル作成失敗: %s\n", filename);
        return false;
    }
    
    // PPMヘッダー書き込み
    file.printf("P6\n");
    file.printf("# FaceID coordinates panorama image\n");
    file.printf("%d %d\n", PANORAMA_WIDTH, PANORAMA_HEIGHT);
    file.printf("255\n");
    
    // RGB画像データ書き込み
    size_t totalBytes = PANORAMA_WIDTH * PANORAMA_HEIGHT * 3;
    size_t writtenBytes = file.write(panoramaBuffer, totalBytes);
    
    file.close();
    
    if (writtenBytes == totalBytes) {
        Serial.printf("✅ パノラマ画像保存成功: %s (%d bytes)\n", filename, writtenBytes);
        return true;
    } else {
        Serial.printf("❌ 書き込みエラー: %d/%d bytes\n", writtenBytes, totalBytes);
        return false;
    }
}

// パノラマ画像からRGB値をサンプリング
CRGB samplePanoramaColor(float u, float v) {
    // UV座標をピクセル座標に変換
    int px = (int)(u * (PANORAMA_WIDTH - 1));
    int py = (int)(v * (PANORAMA_HEIGHT - 1));
    
    // 境界チェック
    px = constrain(px, 0, PANORAMA_WIDTH - 1);
    py = constrain(py, 0, PANORAMA_HEIGHT - 1);
    
    // 🎯 テスト配列を使用する場合
    if (useTestPanorama) {
        int pixelIndex = (py * PANORAMA_WIDTH + px) * 3;
        uint8_t r = testPanoramaRGB[pixelIndex + 0];
        uint8_t g = testPanoramaRGB[pixelIndex + 1];
        uint8_t b = testPanoramaRGB[pixelIndex + 2];
        return CRGB(r, g, b);
    }
    
    // 実際のパノラマバッファから色を取得
    if (panoramaBuffer) {
        int pixelIndex = (py * PANORAMA_WIDTH + px) * 3;
        uint8_t r = panoramaBuffer[pixelIndex + 0];
        uint8_t g = panoramaBuffer[pixelIndex + 1];
        uint8_t b = panoramaBuffer[pixelIndex + 2];
        return CRGB(r, g, b);
    }
    
    // バッファが無い場合は仮想パノラマ画像処理
    uint8_t r = 10, g = 5, b = 15;  // 背景色（暗い紺色）
    
    // 🎯 u=0.25, u=0.75の縦線（0.5緑色）をシミュレーション
    float line_width = 0.02f;  // 縦線の幅（U座標での幅）
    
    // u=0.25の縦線チェック
    if (abs(u - 0.25f) < line_width) {
        r = 0;
        g = 0;  // 0.5緑色
        b = 64;
    }
    // u=0.75の縦線チェック
    else if (abs(u - 0.75f) < line_width) {
        r = 0;
        g = 0;  // 0.5緑色
        b = 64;
    }
    // 🎯 追加：ピクセル単位での縦線チェック（u=79,81,239,241）
    else if (px == 79 || px == 81 || px == 239 || px == 241) {
        r = 0;
        g = 0;  // 0.5緑色
        b = 64;
    }
    // 背景色は既に設定済み
    
    return CRGB(r, g, b);
}

void initializePanorama() {
    if (useTestPanorama) {
        // 🎯 テスト用RGB配列を使用
        Serial.println("✅ テスト用320x160 RGB配列を使用");
        Serial.printf("サイズ: %dx%d = %d bytes (静的確保)\n", 
                     PANORAMA_WIDTH, PANORAMA_HEIGHT, PANORAMA_WIDTH * PANORAMA_HEIGHT * 3);
        
        // テスト配列をパノラマバッファとして設定
        panoramaBuffer = testPanoramaRGB;
        
        Serial.println("テスト配列使用: initializeTestPanorama()で設定された太いライン使用");
        
        Serial.printf("配列サイズ確認: %d bytes\n", sizeof(testPanoramaRGB));
    } else {
        // 従来のPSRAM/RAMを使用
        panoramaBuffer = (uint8_t*)heap_caps_malloc(PANORAMA_WIDTH * PANORAMA_HEIGHT * 3, MALLOC_CAP_SPIRAM);
        
        if (panoramaBuffer) {
            Serial.println("PSRAMにパノラマバッファ確保成功");
            Serial.printf("サイズ: %dx%d = %d bytes\n", PANORAMA_WIDTH, PANORAMA_HEIGHT, PANORAMA_WIDTH * PANORAMA_HEIGHT * 3);
        } else {
            Serial.println("PSRAMバッファ確保失敗 - 通常RAMを試行");
            panoramaBuffer = (uint8_t*)malloc(PANORAMA_WIDTH * PANORAMA_HEIGHT * 3);
        }
    }
    
    // パノラマ画像に緑リングのFaceID座標をプロット
    drawFaceIDCoordinatesToPanorama();
    
    // 💾 PPM形式で保存（無圧縮RGB画像）
    // ファイル形式: PPM (Portable Pixmap)
    // - ヘッダー: P6 + 幅 + 高さ + 最大値(255)
    // - データ: RGB888バイト配列（320×160×3 = 153,600 bytes）
    // - 利点: シンプル、無圧縮、デバッグ容易
    // - 欠点: ファイルサイズ大（JPGの約10倍）
    savePanoramaImageAsPPM("/panorama_faceid.ppm");
    
    coordsInitialized = true;
    Serial.println("パノラマ画像システム初期化完了");
    Serial.printf("X軸大円対象LED数: %d個\n", xNearZeroCount);
}

void loop() {
    // アニメーション付きパノラマ表示
    static bool dataLogged = false;
    static unsigned long lastUpdate = 0;
    
    if (!coordsInitialized) {
        initializePanorama();
    }
    
    // 60FPSでアニメーション更新
    unsigned long now = millis();
    if (now - lastUpdate > 16) {  // 約60FPS (1000ms/60 ≈ 16ms)
        lastUpdate = now;
        
        // アニメーション付きでパノラマ画像を再描画
        drawFaceIDCoordinatesToPanorama();
    }
    
    FastLED.clear();
    
    // 座標データ収集
    int faceIDCount = 0;
    int panoramaCount = 0;
    float sumU = 0, sumV = 0;
    
    // 全LEDを背景色で初期化
    for (int i = 0; i < TOTAL_LEDS; i++) {
        leds[i] = CRGB(0, 0, 5);  // 暗い青背景
    }
    
    // CUBE_neon準拠の全LED処理ループ
    // Serial.printf("🎯 CUBE_neon処理開始 - LED座標読み込み:%s, パノラマバッファ:%s, テスト配列:%s\n", 
    //              ledCoordsLoaded ? "成功" : "失敗", 
    //              (panoramaBuffer != nullptr) ? "確保済み" : "未確保",
    //              useTestPanorama ? "使用" : "未使用");
    
    for (int ledIndex = 0; ledIndex < TOTAL_LEDS; ledIndex++) {
        // 🔹 Step 1: led_layout.csvからxyz座標値を取得
        float x = allLEDCoords[ledIndex].x;
        float y = allLEDCoords[ledIndex].y;
        float z = allLEDCoords[ledIndex].z;
        
        // 🔹 Step 2: IMU/オフセットで座標値を回転
        applyRotation(x, y, z, rotationParams);
        
        // 🔹 Step 3: 回転後xyz → 極座標変換（CUBE_neon高速正規化）
        float length_sq = x*x + y*y + z*z;
        if (length_sq > 0.000001f) {
            // CUBE_neon高速逆平方根を使用した正規化
            float inv_length = fastInvSqrt(length_sq);
            x *= inv_length;  // 高速正規化
            y *= inv_length;
            z *= inv_length;
        }
        
        // 🔹 Step 4: 極座標 → UV座標変換（CUBE_neon高速近似計算）
        float u, v;
        sphericalToUV(x, y, z, u, v);
        
        // 🔹 Step 5: UV座標でパノラマ画像サンプリング位置決定
        int px = (int)(u * (PANORAMA_WIDTH - 1));
        int py = (int)(v * (PANORAMA_HEIGHT - 1));
        px = constrain(px, 0, PANORAMA_WIDTH - 1);
        py = constrain(py, 0, PANORAMA_HEIGHT - 1);
        
        // 🔹 Step 6: UV位置のRGB取得
        uint8_t r, g, b;
        if (panoramaBuffer != nullptr) {
            // パノラマバッファから実際の色を取得
            int pixelIndex = (py * PANORAMA_WIDTH + px) * 3;
            r = panoramaBuffer[pixelIndex + 0];
            g = panoramaBuffer[pixelIndex + 1];
            b = panoramaBuffer[pixelIndex + 2];
            
            // 🚨 DEBUG: ランダム緑点の原因調査
            if (r > 0 || g > 0 || b > 0) {  // 色が付いているLEDをログ出力
                Serial.printf("🔵 LED[%d]: xyz(%.3f,%.3f,%.3f) → UV(%.3f,%.3f) → px(%d,%d) → RGB(%d,%d,%d)\n", 
                             ledIndex, x, y, z, u, v, px, py, r, g, b);
            }
            
            // デバッグ: パノラマバッファの内容確認
            if (ledIndex < 5) {
                // Serial.printf("🔵 LED[%d]: %sから RGB(%d,%d,%d) at pixel(%d,%d)\n", 
                //              ledIndex, useTestPanorama ? "テスト配列" : "パノラマバッファ", 
                //              r, g, b, px, py);
            }
        } else {
            // フォールバック: 仮想パノラマから色をサンプリング
            CRGB virtualColor = samplePanoramaColor(u, v);
            r = virtualColor.r;
            g = virtualColor.g;
            b = virtualColor.b;
            
            // デバッグ: 仮想パノラマの内容確認
            if (ledIndex < 5) {
                Serial.printf("🟡 LED[%d]: 仮想パノラマから RGB(%d,%d,%d) at UV(%.3f,%.3f)\n", 
                             ledIndex, r, g, b, u, v);
            }
        }
        
        // 🔹 Step 7: LEDカラーとして設定
        // デバッグ: サンプリング結果の確認（縦線周辺のLEDを特に確認）
        static int debugLEDStart = 0;
        bool isNearVerticalLine = (abs(u - 0.25f) < 0.05f) || (abs(u - 0.75f) < 0.05f);
        
        if ((ledIndex >= debugLEDStart && ledIndex < debugLEDStart + 5) || isNearVerticalLine) {
            // Serial.printf("LED[%d]: xyz(%.3f,%.3f,%.3f) → UV(%.3f,%.3f) → px(%d,%d) → RGB(%d,%d,%d) %s\n", 
            //              ledIndex, x, y, z, u, v, px, py, r, g, b,
            //              isNearVerticalLine ? "★縦線近傍" : "");
        }
        
        // デバッグLED範囲を毎フレーム移動
        if (ledIndex == TOTAL_LEDS - 1) {
            debugLEDStart = (debugLEDStart + 50) % TOTAL_LEDS;
        }
        
        // 🚀 修正: パノラマサンプリング結果をそのまま設定（重複処理削除）
        leds[ledIndex] = CRGB(r, g, b);
        
        panoramaCount++;
        sumU += u;
        sumV += v;
    }
    
    // 🚨 修正: X=0リング重ね描きを無効化（パノラマサンプリングのみに依存）
    // 太いライン版パノラマで既に正しいリングが表示されるため、重複処理は不要
    /*
    // 3. 🎯 X=0付近の緑リングを重ね描き（参照線として表示）
    for (int j = 0; j < xNearZeroCount; j++) {
        int faceID = xNearZeroCoords[j].faceID;
        if (faceID < TOTAL_LEDS) {
            // パノラマサンプリング結果と合成
            CRGB currentColor = leds[faceID];
            // 緑色を強調表示（既存色に緑を追加）
            leds[faceID] = CRGB(currentColor.r, max(currentColor.g, (uint8_t)255), currentColor.b);
            faceIDCount++;
        }
    }
    */
    
    // LED統計を定期的に出力（アニメーション確認用）
    static int frame_counter = 0;
    frame_counter++;
    if (frame_counter % 60 == 0) {  // 60フレームごと（約1秒間隔）に出力
        float avgU = (panoramaCount > 0) ? sumU / panoramaCount : 0;
        float avgV = (panoramaCount > 0) ? sumV / panoramaCount : 0;
        
        // 重複確認
        int halfGreenCount = 0;  // 0.5緑色
        int greenCount = 0;
        int yellowCount = 0;
        for (int i = 0; i < TOTAL_LEDS; i++) {
            if (leds[i].r > 100 && leds[i].g > 200) {
                yellowCount++;  // 黄色（重複）
            } else if (leds[i].g == 64 && leds[i].r == 0) {
                halfGreenCount++;     // 0.5緑色
            } else if (leds[i].g > 200) {
                greenCount++;   // 緑色
            }
        }
        
        Serial.printf("📊 LED統計 - 0.5緑リング:%d個, 緑リング:%d個, 重複:%d個 | Phase=%.3f\n", 
                     halfGreenCount, greenCount, yellowCount, animationPhase);
    }
    
    FastLED.show();
    delay(100);
}