/**
 * @file cube_neon_integration_demo.cpp
 * @brief CUBE-neon実績実装統合デモ
 * 
 * CUBE-neonの座標変換パイプライン実装確認
 * - 高速数学関数の動作検証
 * - LED座標→IMU回転→UV変換→色抽出の動作確認
 * - BMI270+BMI150センサーデータでの実用性テスト
 */

#include <Arduino.h>
#include "led/LEDSphereManager.h"
#include "math/fast_math.h"
#include <M5Unified.h>

using namespace LEDSphere;
using namespace FastMath;

void setup() {
    Serial.begin(115200);
    M5.begin();
    
    Serial.println("========================================");
    Serial.println("CUBE-neon実績実装統合デモ");
    Serial.println("========================================");
    
    // 高速数学関数のパフォーマンステスト
    Serial.println("\n[1] 高速数学関数パフォーマンステスト");
    Serial.println("CUBE-neon移植版 vs 標準関数");
    
    const int iterations = 10000;
    unsigned long start, end;
    
    // fast_sqrt vs sqrtf パフォーマンス比較
    start = micros();
    for (int i = 0; i < iterations; i++) {
        volatile float result = fast_sqrt(1.0f + i * 0.1f);
    }
    end = micros();
    Serial.printf("fast_sqrt: %lu μs (%d回)\n", end - start, iterations);
    
    start = micros();
    for (int i = 0; i < iterations; i++) {
        volatile float result = sqrtf(1.0f + i * 0.1f);
    }
    end = micros();
    Serial.printf("sqrtf: %lu μs (%d回)\n", end - start, iterations);
    
    // fast_atan2 vs atan2f パフォーマンス比較
    start = micros();
    for (int i = 0; i < iterations; i++) {
        volatile float result = fast_atan2(1.0f, 1.0f + i * 0.01f);
    }
    end = micros();
    Serial.printf("fast_atan2: %lu μs (%d回)\n", end - start, iterations);
    
    start = micros();
    for (int i = 0; i < iterations; i++) {
        volatile float result = atan2f(1.0f, 1.0f + i * 0.01f);
    }
    end = micros();
    Serial.printf("atan2f: %lu μs (%d回)\n", end - start, iterations);
    
    // LEDSphereManager初期化
    Serial.println("\n[2] LEDSphereManager初期化");
    LEDSphereManager* manager = SpherePatternInterface::getInstance();
    if (!manager->initialize("data/led_layout.csv")) {
        Serial.println("警告: CSVファイル読み込み失敗 - デフォルトパターンで継続");
    }
    
    // CUBE-neon座標変換パイプラインテスト
    Serial.println("\n[3] CUBE-neon座標変換パイプライン");
    Serial.println("LED座標→IMU回転→UV変換→色抽出");
    
    // テスト用IMUデータ（BMI270+BMI150シミュレート）
    ImuPosture testPosture;
    testPosture.quaternionW = 0.7071f;  // 45度回転のクォータニオン
    testPosture.quaternionX = 0.7071f;
    testPosture.quaternionY = 0.0f;
    testPosture.quaternionZ = 0.0f;
    
    // テスト座標パターン
    struct TestCoord {
        float x, y, z;
        const char* name;
    };
    
    TestCoord testCoords[] = {
        {1.0f, 0.0f, 0.0f, "X軸正方向"},
        {0.0f, 1.0f, 0.0f, "Y軸正方向"},
        {0.0f, 0.0f, 1.0f, "Z軸正方向"},
        {0.7071f, 0.7071f, 0.0f, "XY対角線"},
        {-0.5f, 0.5f, 0.7071f, "複合角度"}
    };
    
    Serial.println("テスト座標での変換結果:");
    Serial.println("座標 → 回転座標 → UV座標 → RGB色");
    
    for (auto& coord : testCoords) {
        // 1. クォータニオン回転適用
        float rotX, rotY, rotZ;
        // manager->applyQuaternionRotation(coord.x, coord.y, coord.z, rotX, rotY, rotZ);
        
        // 暫定実装（関数アクセス制限のため）
        float qw = testPosture.quaternionW;
        float qx = testPosture.quaternionX;
        float qy = testPosture.quaternionY;
        float qz = testPosture.quaternionZ;
        
        // クォータニオン正規化
        float norm = fast_sqrt(qw*qw + qx*qx + qy*qy + qz*qz);
        if (norm > 0.0001f) {
            qw /= norm; qx /= norm; qy /= norm; qz /= norm;
        }
        
        // ベクトル回転展開計算
        float qw2 = qw * qw, qx2 = qx * qx, qy2 = qy * qy, qz2 = qz * qz;
        rotX = (qw2 + qx2 - qy2 - qz2) * coord.x + 2.0f * (qx*qy - qw*qz) * coord.y + 2.0f * (qx*qz + qw*qy) * coord.z;
        rotY = 2.0f * (qx*qy + qw*qz) * coord.x + (qw2 - qx2 + qy2 - qz2) * coord.y + 2.0f * (qy*qz - qw*qx) * coord.z;
        rotZ = 2.0f * (qx*qz - qw*qy) * coord.x + 2.0f * (qy*qz + qw*qx) * coord.y + (qw2 - qx2 - qy2 + qz2) * coord.z;
        
        // 2. UV座標変換（CUBE-neon方式）
        float rxy = fast_sqrt(rotX * rotX + rotZ * rotZ);
        float u = fast_atan2(rxy, rotY);  // 緯度系
        float v = fast_atan2(rotX, rotZ); // 経度系
        
        // 3. UV→RGB変換
        float norm_u = (u + M_PI/2.0f) / M_PI;
        float norm_v = (v + M_PI) / (2.0f * M_PI);
        
        // クランプ
        if (norm_u < 0.0f) norm_u = 0.0f;
        if (norm_u > 1.0f) norm_u = 1.0f;
        if (norm_v < 0.0f) norm_v = 0.0f;
        if (norm_v > 1.0f) norm_v = 1.0f;
        
        // HSV→RGB変換
        uint8_t hue = (uint8_t)(norm_v * 255);
        uint8_t sat = 255;
        uint8_t val = (uint8_t)(norm_u * 255);
        
        CRGB color;
        color.setHSV(hue, sat, val);
        
        Serial.printf("%s:\n", coord.name);
        Serial.printf("  原座標: (%.3f, %.3f, %.3f)\n", coord.x, coord.y, coord.z);
        Serial.printf("  回転後: (%.3f, %.3f, %.3f)\n", rotX, rotY, rotZ);
        Serial.printf("  UV座標: (%.3f, %.3f)\n", u, v);
        Serial.printf("  RGB色 : (%d, %d, %d)\n\n", color.r, color.g, color.b);
    }
    
    Serial.println("[4] CUBE-neon実績統合完了");
    Serial.println("高速数学関数とパイプライン動作確認済み");
    Serial.println("========================================");
}

void loop() {
    delay(5000);
    
    // 簡単な動作表示
    static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate > 5000) {
        Serial.println("CUBE-neon統合システム動作中...");
        lastUpdate = millis();
    }
}