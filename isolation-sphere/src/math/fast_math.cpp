/**
 * @file fast_math.cpp  
 * @brief 高速数学関数ライブラリ実装
 */

#include "math/fast_math.h"

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <chrono>
#include <iostream>
#endif

namespace FastMath {

PerfResult benchmark_fast_math(uint32_t iterations) {
    PerfResult result = {};
    
    // テストデータ生成
    float test_vals_sqrt[100];
    float test_vals_x[100];
    float test_vals_y[100];
    
    for (int i = 0; i < 100; ++i) {
        test_vals_sqrt[i] = 0.1f + (float)i * 0.1f;
        test_vals_x[i] = -5.0f + (float)i * 0.1f;
        test_vals_y[i] = -5.0f + (float)(99-i) * 0.1f;
    }
    
#ifdef ARDUINO
    // Arduino環境でのベンチマーク
    uint32_t start_time, end_time;
    volatile float dummy = 0.0f;  // 最適化防止
    
    // fast_sqrt テスト
    start_time = micros();
    for (uint32_t iter = 0; iter < iterations; ++iter) {
        for (int i = 0; i < 100; ++i) {
            dummy += fast_sqrt(test_vals_sqrt[i]);
        }
    }
    end_time = micros();
    result.fast_sqrt_time = (float)(end_time - start_time) / (iterations * 100);
    
    // 標準sqrt テスト
    start_time = micros();
    for (uint32_t iter = 0; iter < iterations; ++iter) {
        for (int i = 0; i < 100; ++i) {
            dummy += sqrtf(test_vals_sqrt[i]);
        }
    }
    end_time = micros();
    result.std_sqrt_time = (float)(end_time - start_time) / (iterations * 100);
    
    // fast_atan2 テスト
    start_time = micros();
    for (uint32_t iter = 0; iter < iterations; ++iter) {
        for (int i = 0; i < 100; ++i) {
            dummy += fast_atan2(test_vals_y[i], test_vals_x[i]);
        }
    }
    end_time = micros();
    result.fast_atan2_time = (float)(end_time - start_time) / (iterations * 100);
    
    // 標準atan2 テスト
    start_time = micros();
    for (uint32_t iter = 0; iter < iterations; ++iter) {
        for (int i = 0; i < 100; ++i) {
            dummy += atan2f(test_vals_y[i], test_vals_x[i]);
        }
    }
    end_time = micros();
    result.std_atan2_time = (float)(end_time - start_time) / (iterations * 100);
    
    Serial.printf("[FastMath] Benchmark completed (dummy=%.3f)\\n", dummy);
    
#else
    // デスクトップ環境でのベンチマーク
    using namespace std::chrono;
    volatile float dummy = 0.0f;
    
    auto start = high_resolution_clock::now();
    for (uint32_t iter = 0; iter < iterations; ++iter) {
        for (int i = 0; i < 100; ++i) {
            dummy += fast_sqrt(test_vals_sqrt[i]);
        }
    }
    auto end = high_resolution_clock::now();
    result.fast_sqrt_time = duration_cast<nanoseconds>(end - start).count() / (1000.0f * iterations * 100);
    
    start = high_resolution_clock::now();
    for (uint32_t iter = 0; iter < iterations; ++iter) {
        for (int i = 0; i < 100; ++i) {
            dummy += sqrtf(test_vals_sqrt[i]);
        }
    }
    end = high_resolution_clock::now();
    result.std_sqrt_time = duration_cast<nanoseconds>(end - start).count() / (1000.0f * iterations * 100);
    
    start = high_resolution_clock::now();
    for (uint32_t iter = 0; iter < iterations; ++iter) {
        for (int i = 0; i < 100; ++i) {
            dummy += fast_atan2(test_vals_y[i], test_vals_x[i]);
        }
    }
    end = high_resolution_clock::now();
    result.fast_atan2_time = duration_cast<nanoseconds>(end - start).count() / (1000.0f * iterations * 100);
    
    start = high_resolution_clock::now();
    for (uint32_t iter = 0; iter < iterations; ++iter) {
        for (int i = 0; i < 100; ++i) {
            dummy += atan2f(test_vals_y[i], test_vals_x[i]);
        }
    }
    end = high_resolution_clock::now();
    result.std_atan2_time = duration_cast<nanoseconds>(end - start).count() / (1000.0f * iterations * 100);
    
    std::cout << "[FastMath] Benchmark completed (dummy=" << dummy << ")" << std::endl;
    
#endif
    
    // 速度向上率計算
    result.speedup_sqrt = result.std_sqrt_time / result.fast_sqrt_time;
    result.speedup_atan2 = result.std_atan2_time / result.fast_atan2_time;
    
    return result;
}

} // namespace FastMath