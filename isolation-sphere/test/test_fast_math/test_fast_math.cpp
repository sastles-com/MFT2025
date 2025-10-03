/**
 * @file test_fast_math.cpp
 * @brief 高速数学関数のユニットテスト
 * 
 * CUBE-neonから移植した近似関数の精度・パフォーマンス検証
 * BMI270+BMI150センサーデータでの実用性テスト
 */

#include <unity.h>
#include "math/fast_math.h"
#include <cmath>
#include <cstdio>  // snprintf用

using namespace FastMath;

// テスト精度閾値
const float SQRT_TOLERANCE = 0.01f;    // sqrt: 1%
const float ATAN2_TOLERANCE = 0.005f;  // atan2: 0.5%  
const float ASIN_TOLERANCE = 0.002f;   // asin: 0.2%

void setUp(void) {
    // テスト前の初期化
}

void tearDown(void) {
    // テスト後のクリーンアップ
}

/**
 * @brief fast_sqrt精度テスト
 */
void test_fast_sqrt_accuracy() {
    float test_values[] = {0.1f, 0.5f, 1.0f, 2.0f, 4.0f, 9.0f, 16.0f, 25.0f, 100.0f};
    int num_tests = sizeof(test_values) / sizeof(test_values[0]);
    
    for (int i = 0; i < num_tests; ++i) {
        float input = test_values[i];
        float expected = sqrtf(input);
        float actual = fast_sqrt(input);
        float error = fabs(actual - expected) / expected;
        
        char msg[64];
        snprintf(msg, sizeof(msg), "sqrt(%.1f): exp=%.6f, act=%.6f, err=%.4f%%", 
                input, expected, actual, error * 100.0f);
        
        TEST_ASSERT_LESS_THAN_MESSAGE(SQRT_TOLERANCE, error, msg);
    }
}

/**
 * @brief fast_atan2精度テスト（球面座標用）
 */
void test_fast_atan2_accuracy() {
    struct TestCase {
        float y, x;
        const char* description;
    } test_cases[] = {
        {1.0f, 1.0f, "45度（第1象限）"},
        {1.0f, -1.0f, "135度（第2象限）"},
        {-1.0f, -1.0f, "-135度（第3象限）"},
        {-1.0f, 1.0f, "-45度（第4象限）"},
        {0.0f, 1.0f, "0度（+X軸）"},
        {1.0f, 0.0f, "90度（+Y軸）"},
        {0.0f, -1.0f, "180度（-X軸）"},
        {-1.0f, 0.0f, "-90度（-Y軸）"},
        {0.866f, 0.5f, "60度"},
        {0.5f, 0.866f, "30度"}
    };
    
    int num_tests = sizeof(test_cases) / sizeof(test_cases[0]);
    
    for (int i = 0; i < num_tests; ++i) {
        float y = test_cases[i].y;
        float x = test_cases[i].x;
        float expected = atan2f(y, x);
        float actual = fast_atan2(y, x);
        float error = fabs(actual - expected) / (fabs(expected) + 1e-6f);
        
        char msg[128];
        snprintf(msg, sizeof(msg), "%s: exp=%.6f, act=%.6f, err=%.4f%%", 
                test_cases[i].description, expected, actual, error * 100.0f);
        
        TEST_ASSERT_LESS_THAN_MESSAGE(ATAN2_TOLERANCE, error, msg);
    }
}

/**
 * @brief fast_asin精度テスト（緯度計算用）
 */
void test_fast_asin_accuracy() {
    float test_values[] = {-1.0f, -0.866f, -0.5f, -0.1f, 0.0f, 0.1f, 0.5f, 0.866f, 1.0f};
    int num_tests = sizeof(test_values) / sizeof(test_values[0]);
    
    for (int i = 0; i < num_tests; ++i) {
        float input = test_values[i];
        float expected = asinf(input);
        float actual = fast_asin(input);
        float error = fabs(actual - expected) / (fabs(expected) + 1e-6f);
        
        char msg[64];
        snprintf(msg, sizeof(msg), "asin(%.3f): exp=%.6f, act=%.6f, err=%.4f%%", 
                input, expected, actual, error * 100.0f);
        
        TEST_ASSERT_LESS_THAN_MESSAGE(ASIN_TOLERANCE, error, msg);
    }
}

/**
 * @brief 球面座標変換の統合テスト
 */
void test_spherical_coordinate_conversion() {
    // 球面上の典型的な点をテスト
    struct TestPoint {
        float x, y, z;
        const char* description;
    } test_points[] = {
        {1.0f, 0.0f, 0.0f, "赤道・0度経線"},
        {0.0f, 1.0f, 0.0f, "北極"},
        {0.0f, 0.0f, 1.0f, "赤道・90度経線"},
        {0.707f, 0.707f, 0.0f, "北緯45度・0度経線"},
        {0.5f, 0.866f, 0.0f, "北緯60度・0度経線"}
    };
    
    int num_tests = sizeof(test_points) / sizeof(test_points[0]);
    
    for (int i = 0; i < num_tests; ++i) {
        float x = test_points[i].x;
        float y = test_points[i].y; 
        float z = test_points[i].z;
        
        // 標準ライブラリ版
        float std_r = sqrtf(x*x + y*y + z*z);
        float std_lat = asinf(y / std_r);
        float std_lon = atan2f(z, x);
        
        // 高速近似版
        float fast_r = fast_sqrt(x*x + y*y + z*z);
        float fast_lat = fast_asin(y / fast_r);
        float fast_lon = fast_atan2(z, x);
        
        // 精度比較
        float r_error = fabs(fast_r - std_r) / std_r;
        float lat_error = fabs(fast_lat - std_lat) / (fabs(std_lat) + 1e-6f);
        float lon_error = fabs(fast_lon - std_lon) / (fabs(std_lon) + 1e-6f);
        
        char msg[128];
        snprintf(msg, sizeof(msg), "%s: r_err=%.4f%%, lat_err=%.4f%%, lon_err=%.4f%%", 
                test_points[i].description, r_error*100, lat_error*100, lon_error*100);
        
        TEST_ASSERT_LESS_THAN_MESSAGE(0.01f, r_error, msg);    // 1%
        TEST_ASSERT_LESS_THAN_MESSAGE(0.005f, lat_error, msg); // 0.5%
        TEST_ASSERT_LESS_THAN_MESSAGE(0.005f, lon_error, msg); // 0.5%
    }
}

/**
 * @brief パフォーマンステスト
 */
void test_performance_benchmark() {
    const uint32_t iterations = 1000;  // テスト用に軽量化
    
    PerfResult result = benchmark_fast_math(iterations);
    
    // 速度向上の検証
    TEST_ASSERT_GREATER_THAN_MESSAGE(2.0f, result.speedup_sqrt, 
                                   "fast_sqrt should be at least 2x faster");
    TEST_ASSERT_GREATER_THAN_MESSAGE(10.0f, result.speedup_atan2, 
                                   "fast_atan2 should be at least 10x faster");
    
    // 実行時間の妥当性チェック（ESP32-S3での想定値）
    TEST_ASSERT_LESS_THAN_MESSAGE(10.0f, result.fast_sqrt_time, 
                                "fast_sqrt should be under 10μs per call");
    TEST_ASSERT_LESS_THAN_MESSAGE(1.0f, result.fast_atan2_time, 
                                "fast_atan2 should be under 1μs per call");
}

/**
 * @brief BMI270+BMI150センサーデータ模擬テスト
 */
void test_bmi270_bmi150_simulation() {
    // BMI270+BMI150の典型的な出力値を模擬
    struct SensorData {
        float qw, qx, qy, qz;  // クォータニオン
        const char* description;
    } sensor_data[] = {
        {1.0f, 0.0f, 0.0f, 0.0f, "静止状態"},
        {0.707f, 0.707f, 0.0f, 0.0f, "X軸90度回転"},
        {0.707f, 0.0f, 0.707f, 0.0f, "Y軸90度回転"},
        {0.707f, 0.0f, 0.0f, 0.707f, "Z軸90度回転"}
    };
    
    int num_tests = sizeof(sensor_data) / sizeof(sensor_data[0]);
    
    for (int i = 0; i < num_tests; ++i) {
        float qw = sensor_data[i].qw;
        float qx = sensor_data[i].qx;
        float qy = sensor_data[i].qy;
        float qz = sensor_data[i].qz;
        
        // クォータニオンからオイラー角への変換（高速版）
        float test_y = 2.0f * (qw * qy - qz * qx);
        if (test_y > 1.0f) test_y = 1.0f;
        if (test_y < -1.0f) test_y = -1.0f;
        
        float pitch_fast = fast_asin(test_y);
        float roll_fast = fast_atan2(2.0f * (qw * qx + qy * qz), 1.0f - 2.0f * (qx * qx + qy * qy));
        float yaw_fast = fast_atan2(2.0f * (qw * qz + qx * qy), 1.0f - 2.0f * (qy * qy + qz * qz));
        
        // 標準版との比較
        float pitch_std = asinf(test_y);
        float roll_std = atan2f(2.0f * (qw * qx + qy * qz), 1.0f - 2.0f * (qx * qx + qy * qy));
        float yaw_std = atan2f(2.0f * (qw * qz + qx * qy), 1.0f - 2.0f * (qy * qy + qz * qz));
        
        // 精度検証
        float pitch_error = fabs(pitch_fast - pitch_std) / (fabs(pitch_std) + 1e-6f);
        float roll_error = fabs(roll_fast - roll_std) / (fabs(roll_std) + 1e-6f);
        float yaw_error = fabs(yaw_fast - yaw_std) / (fabs(yaw_std) + 1e-6f);
        
        char msg[128];
        snprintf(msg, sizeof(msg), "%s: pitch_err=%.4f%%, roll_err=%.4f%%, yaw_err=%.4f%%", 
                sensor_data[i].description, pitch_error*100, roll_error*100, yaw_error*100);
        
        TEST_ASSERT_LESS_THAN_MESSAGE(0.005f, pitch_error, msg); // 0.5%
        TEST_ASSERT_LESS_THAN_MESSAGE(0.005f, roll_error, msg);  // 0.5%
        TEST_ASSERT_LESS_THAN_MESSAGE(0.005f, yaw_error, msg);   // 0.5%
    }
}

/**
 * @brief メインテスト実行
 */
void run_fast_math_tests() {
    UNITY_BEGIN();
    
    RUN_TEST(test_fast_sqrt_accuracy);
    RUN_TEST(test_fast_atan2_accuracy);
    RUN_TEST(test_fast_asin_accuracy);
    RUN_TEST(test_spherical_coordinate_conversion);
    RUN_TEST(test_performance_benchmark);
    RUN_TEST(test_bmi270_bmi150_simulation);
    
    UNITY_END();
}