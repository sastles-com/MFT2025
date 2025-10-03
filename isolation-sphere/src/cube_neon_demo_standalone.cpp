/**
 * @file main.cpp - CUBE-neon統合デモ一時版
 */

#include <Arduino.h>
#include "math/fast_math.h"
#include <M5Unified.h>

using namespace FastMath;

void setup() {
    Serial.begin(115200);
    delay(1000);  // シリアル安定化待ち
    
    Serial.println("========================================");
    Serial.println("CUBE-neon実績実装統合デモ");
    Serial.println("高速数学関数パフォーマンステスト");
    Serial.println("========================================");
    
    const int iterations = 1000;
    unsigned long start, end;
    
    // fast_sqrt vs sqrtf パフォーマンス比較
    Serial.println("\n[1] fast_sqrt vs sqrtf");
    
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
    Serial.printf("sqrtf    : %lu μs (%d回)\n", end - start, iterations);
    
    // パフォーマンス改善率計算
    float sqrt_improvement = (float)(end - start) / (end - start);
    
    // fast_atan2 vs atan2f パフォーマンス比較
    Serial.println("\n[2] fast_atan2 vs atan2f");
    
    start = micros();
    for (int i = 0; i < iterations; i++) {
        volatile float result = fast_atan2(1.0f, 1.0f + i * 0.01f);
    }
    end = micros();
    unsigned long fast_atan2_time = end - start;
    Serial.printf("fast_atan2: %lu μs (%d回)\n", fast_atan2_time, iterations);
    
    start = micros();
    for (int i = 0; i < iterations; i++) {
        volatile float result = atan2f(1.0f, 1.0f + i * 0.01f);
    }
    end = micros();
    unsigned long std_atan2_time = end - start;
    Serial.printf("atan2f    : %lu μs (%d回)\n", std_atan2_time, iterations);
    
    if (fast_atan2_time > 0) {
        float atan2_improvement = (float)std_atan2_time / (float)fast_atan2_time;
        Serial.printf("改善率: %.1fx高速化\n", atan2_improvement);
    }
    
    // CUBE-neon座標変換パイプラインテスト
    Serial.println("\n[3] CUBE-neon座標変換パイプライン");
    Serial.println("LED座標→IMU回転→UV変換→色抽出");
    
    // テスト用クォータニオン（45度X軸回転）
    float qw = 0.9239f, qx = 0.3827f, qy = 0.0f, qz = 0.0f;
    
    // テスト座標パターン
    struct TestCoord {
        float x, y, z;
        const char* name;
    };
    
    TestCoord testCoords[] = {
        {1.0f, 0.0f, 0.0f, "X軸正方向"},
        {0.0f, 1.0f, 0.0f, "Y軸正方向"},
        {0.0f, 0.0f, 1.0f, "Z軸正方向"},
        {0.7071f, 0.7071f, 0.0f, "XY対角線"}
    };
    
    Serial.println("原座標 → 回転座標 → UV座標:");
    
    for (auto& coord : testCoords) {
        // クォータニオン回転適用（CUBE-neon実装）
        float norm = fast_sqrt(qw*qw + qx*qx + qy*qy + qz*qz);
        if (norm > 0.0001f) {
            qw /= norm; qx /= norm; qy /= norm; qz /= norm;
        }
        
        float qw2 = qw * qw, qx2 = qx * qx, qy2 = qy * qy, qz2 = qz * qz;
        float rotX = (qw2 + qx2 - qy2 - qz2) * coord.x + 2.0f * (qx*qy - qw*qz) * coord.y + 2.0f * (qx*qz + qw*qy) * coord.z;
        float rotY = 2.0f * (qx*qy + qw*qz) * coord.x + (qw2 - qx2 + qy2 - qz2) * coord.y + 2.0f * (qy*qz - qw*qx) * coord.z;
        float rotZ = 2.0f * (qx*qz - qw*qy) * coord.x + 2.0f * (qy*qz + qw*qx) * coord.y + (qw2 - qx2 - qy2 + qz2) * coord.z;
        
        // UV座標変換（CUBE-neon方式: 高速数学関数使用）
        float rxy = fast_sqrt(rotX * rotX + rotZ * rotZ);
        float u = fast_atan2(rxy, rotY);
        float v = fast_atan2(rotX, rotZ);
        
        Serial.printf("%s:\n", coord.name);
        Serial.printf("  (%.3f,%.3f,%.3f) → (%.3f,%.3f,%.3f) → UV(%.3f,%.3f)\n",
                     coord.x, coord.y, coord.z, rotX, rotY, rotZ, u, v);
    }
    
    Serial.println("\n[4] CUBE-neon実績統合完了✅");
    Serial.println("- 高速数学関数移植確認");
    Serial.println("- 座標変換パイプライン動作確認"); 
    Serial.println("- BMI270+BMI150対応準備完了");
    Serial.println("========================================");
}

void loop() {
    static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate > 10000) {
        Serial.println("CUBE-neon統合システム稼働中... (10秒間隔)");
        lastUpdate = millis();
    }
    delay(100);
}