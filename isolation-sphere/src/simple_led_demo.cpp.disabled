/**
 * @file simple_led_demo.cpp
 * @brief シンプルなCUBE-neon LED表示デモ
 * 
 * LEDSphereManagerを使わずに、CUBE-neonの座標変換のみ実装
 * M5Stack AtomS3RでFastLEDを直接制御
 */

#include <Arduino.h>
#include "math/fast_math.h"
#include <M5Unified.h>
#include <FastLED.h>

using namespace FastMath;

// 前方宣言
void demonstrateCubeNeonPipeline();

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
#define NUM_STRIPS 4       // ストリップ数
#define TOTAL_LEDS (LEDS_STRIP_1 + LEDS_STRIP_2 + LEDS_STRIP_3 + LEDS_STRIP_4) // 合計800個
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// FastLED配列
CRGB leds[TOTAL_LEDS];

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("========================================");
    Serial.println("CUBE-neon シンプルLED表示デモ");
    Serial.println("========================================");
    
    // M5Stack初期化
    M5.begin();
    
    // FastLED初期化（config.json準拠：GPIO 5,6,7,8、可変長LED数）
    Serial.println("\n[1] FastLED初期化（config.json準拠構成）");
    
    // ストリップごとのオフセット計算
    int offset = 0;
    FastLED.addLeds<LED_TYPE, LED_DATA_PIN_1, COLOR_ORDER>(leds, offset, LEDS_STRIP_1);
    offset += LEDS_STRIP_1;
    FastLED.addLeds<LED_TYPE, LED_DATA_PIN_2, COLOR_ORDER>(leds, offset, LEDS_STRIP_2);
    offset += LEDS_STRIP_2;
    FastLED.addLeds<LED_TYPE, LED_DATA_PIN_3, COLOR_ORDER>(leds, offset, LEDS_STRIP_3);
    offset += LEDS_STRIP_3;
    FastLED.addLeds<LED_TYPE, LED_DATA_PIN_4, COLOR_ORDER>(leds, offset, LEDS_STRIP_4);
    
    FastLED.setBrightness(32);  // config.json準拠（12.5%）
    FastLED.clear();
    FastLED.show();
    
    Serial.printf("LED初期化完了:\n");
    Serial.printf("  ストリップ1 (GPIO %d): %d LED\n", LED_DATA_PIN_1, LEDS_STRIP_1);
    Serial.printf("  ストリップ2 (GPIO %d): %d LED\n", LED_DATA_PIN_2, LEDS_STRIP_2);
    Serial.printf("  ストリップ3 (GPIO %d): %d LED\n", LED_DATA_PIN_3, LEDS_STRIP_3);
    Serial.printf("  ストリップ4 (GPIO %d): %d LED\n", LED_DATA_PIN_4, LEDS_STRIP_4);
    Serial.printf("  合計: %d LED\n", TOTAL_LEDS);
    
    // 高速数学関数パフォーマンステスト
    Serial.println("\n[2] CUBE-neon高速数学関数テスト");
    const int iterations = 1000;
    unsigned long start, end;
    
    start = micros();
    for (int i = 0; i < iterations; i++) {
        volatile float result = fast_atan2(1.0f, 1.0f + i * 0.01f);
    }
    end = micros();
    unsigned long fast_time = end - start;
    Serial.printf("fast_atan2: %lu μs (%d回)\n", fast_time, iterations);
    
    start = micros();
    for (int i = 0; i < iterations; i++) {
        volatile float result = atan2f(1.0f, 1.0f + i * 0.01f);
    }
    end = micros();
    unsigned long std_time = end - start;
    Serial.printf("atan2f    : %lu μs (%d回)\n", std_time, iterations);
    
    if (fast_time > 0) {
        float improvement = (float)std_time / (float)fast_time;
        Serial.printf("改善率: %.1fx高速化\n", improvement);
    }
    
    // CUBE-neon座標変換デモ表示
    Serial.println("\n[3] CUBE-neon座標変換→LED表示デモ");
    demonstrateCubeNeonPipeline();
    
    Serial.println("\n[4] リアルタイムLED表示開始");
    Serial.println("========================================");
}

void demonstrateCubeNeonPipeline() {
    Serial.println("CUBE-neon座標変換パイプライン→LED表示:");
    
    // 基本色デモ（3パターン）
    for (int demo = 0; demo < 3; demo++) {
        // テスト用クォータニオン（回転シミュレート）
        float angle = demo * M_PI / 3.0f;  // 60度ずつ回転
        float qw = cos(angle / 2.0f);
        float qx = sin(angle / 2.0f);
        float qy = 0.0f;
        float qz = 0.0f;
        
        Serial.printf("\nデモ%d: %.0f度回転\n", demo + 1, angle * 180.0f / M_PI);
        
        // LED配列クリア
        FastLED.clear();
        
        // 球面座標→LED表示（CUBE-neon方式）
        for (int i = 0; i < TOTAL_LEDS; i++) {
            // プロシージャル球面座標生成
            float t = (float)i / TOTAL_LEDS;
            float theta = t * 2.0f * M_PI;        // 経度 [0, 2π]
            float phi = acos(1.0f - 2.0f * t);    // 緯度 [0, π]
            
            // 球面座標→直交座標
            float x = sin(phi) * cos(theta);
            float y = cos(phi);
            float z = sin(phi) * sin(theta);
            
            // CUBE-neonクォータニオン回転適用
            float norm = fast_sqrt(qw*qw + qx*qx + qy*qy + qz*qz);
            if (norm > 0.0001f) {
                float qw_n = qw / norm, qx_n = qx / norm, qy_n = qy / norm, qz_n = qz / norm;
                
                // クォータニオン回転行列展開
                float qw2 = qw_n * qw_n, qx2 = qx_n * qx_n, qy2 = qy_n * qy_n, qz2 = qz_n * qz_n;
                float rotX = (qw2 + qx2 - qy2 - qz2) * x + 2.0f * (qx_n*qy_n - qw_n*qz_n) * y + 2.0f * (qx_n*qz_n + qw_n*qy_n) * z;
                float rotY = 2.0f * (qx_n*qy_n + qw_n*qz_n) * x + (qw2 - qx2 + qy2 - qz2) * y + 2.0f * (qy_n*qz_n - qw_n*qx_n) * z;
                float rotZ = 2.0f * (qx_n*qz_n - qw_n*qy_n) * x + 2.0f * (qy_n*qz_n + qw_n*qx_n) * y + (qw2 - qx2 - qy2 + qz2) * z;
                
                // UV座標変換（CUBE-neon方式）
                float rxy = fast_sqrt(rotX * rotX + rotZ * rotZ);
                float u = fast_atan2(rxy, rotY);     // 緯度系
                float v = fast_atan2(rotX, rotZ);    // 経度系
                
                // UV→RGB変換
                float norm_u = (u + M_PI/2.0f) / M_PI;
                float norm_v = (v + M_PI) / (2.0f * M_PI);
                
                // 正規化（0-1範囲）
                norm_u = constrain(norm_u, 0.0f, 1.0f);
                norm_v = constrain(norm_v, 0.0f, 1.0f);
                
                // HSV色生成（CUBE-neon方式）
                uint8_t hue = (uint8_t)(norm_v * 255);        // 経度で色相
                uint8_t sat = 255;                            // 彩度最大
                uint8_t val = (uint8_t)(norm_u * 200 + 55);   // 緯度で輝度
                
                // FastLEDに設定
                leds[i].setHSV(hue, sat, val);
            }
        }
        
        // LED表示更新
        FastLED.show();
        Serial.printf("LED表示更新: %d個のLED表示\n", TOTAL_LEDS);
        
        delay(3000);  // 3秒表示
    }
    
    // 全LED消灯
    FastLED.clear();
    FastLED.show();
    Serial.println("デモ完了 - LED消灯");
}

void loop() {
    static uint32_t lastUpdate = 0;
    static float rotation = 0.0f;
    
    if (millis() - lastUpdate > 50) {  // 20FPS更新
        // 連続回転アニメーション
        rotation += 0.05f;
        if (rotation > 2.0f * M_PI) rotation = 0.0f;
        
        // クォータニオン生成（Y軸回転）
        float qw = cos(rotation / 2.0f);
        float qy = sin(rotation / 2.0f);
        
        // 簡易LED更新（パフォーマンス重視）
        static int frameCount = 0;
        frameCount++;
        
        if (frameCount % 100 == 0) {  // 5秒ごとにシリアル出力
            Serial.printf("回転中: %.1f度, FPS: 20\n", rotation * 180.0f / M_PI);
        }
        
        // 高速LED更新（4個おきサンプリング）
        for (int i = 0; i < TOTAL_LEDS; i += 4) {
            float t = (float)i / TOTAL_LEDS + rotation / (2.0f * M_PI);
            if (t > 1.0f) t -= 1.0f;
            
            uint8_t hue = (uint8_t)(t * 255);
            uint8_t val = 128;  // 中輝度
            
            leds[i].setHSV(hue, 255, val);
            
            // 近傍LEDも同色で塗る（補間）
            for (int j = 1; j < 4 && (i + j) < TOTAL_LEDS; j++) {
                leds[i + j] = leds[i];
            }
        }
        
        FastLED.show();
        lastUpdate = millis();
    }
    
    // M5ボタン処理
    M5.update();
    if (M5.BtnA.wasPressed()) {
        Serial.println("ボタン押下 - デモ再実行");
        demonstrateCubeNeonPipeline();
    }
    
    delay(10);
}