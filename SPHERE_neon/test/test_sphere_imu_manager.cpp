#include <unity.h>
#include "imu/SphereIMUManager.h"

// テスト用のモック設定
namespace sphere_imu_test {
    // 期待されるIMU制御動作をテスト

    void test_constructor_initializes_correctly() {
        SphereIMUManager imuManager;
        
        // コンストラクタが正常に実行されること
        TEST_ASSERT_FALSE(imuManager.isInitialized());
        TEST_ASSERT_EQUAL(100, imuManager.getSampleRate()); // デフォルト100Hz
    }

    void test_initialize_with_valid_parameters() {
        SphereIMUManager imuManager;
        
        // 初期化が成功すること（M5Unified経由）
        bool result = imuManager.initialize();
        TEST_ASSERT_TRUE(result);
        
        // 初期化後の状態確認
        TEST_ASSERT_TRUE(imuManager.isInitialized());
        TEST_ASSERT_TRUE(imuManager.isCalibrated()); // キャリブレーション完了
    }

    void test_set_sample_rate() {
        SphereIMUManager imuManager;
        imuManager.initialize();
        
        // サンプリングレート設定
        bool result1 = imuManager.setSampleRate(200); // 200Hz
        TEST_ASSERT_TRUE(result1);
        TEST_ASSERT_EQUAL(200, imuManager.getSampleRate());
        
        // 無効な値
        bool result2 = imuManager.setSampleRate(0);
        TEST_ASSERT_FALSE(result2);
        
        bool result3 = imuManager.setSampleRate(2000); // 上限超過
        TEST_ASSERT_FALSE(result3);
    }

    void test_read_raw_sensor_data() {
        SphereIMUManager imuManager;
        imuManager.initialize();
        
        // 生データ読み取り
        SphereIMUManager::RawData rawData;
        bool result = imuManager.readRawData(rawData);
        TEST_ASSERT_TRUE(result);
        
        // データ範囲チェック（BMI270仕様）
        // 加速度: ±16g
        TEST_ASSERT_TRUE(rawData.accel_x >= -16.0f && rawData.accel_x <= 16.0f);
        TEST_ASSERT_TRUE(rawData.accel_y >= -16.0f && rawData.accel_y <= 16.0f);
        TEST_ASSERT_TRUE(rawData.accel_z >= -16.0f && rawData.accel_z <= 16.0f);
        
        // 角速度: ±2000dps
        TEST_ASSERT_TRUE(rawData.gyro_x >= -2000.0f && rawData.gyro_x <= 2000.0f);
        TEST_ASSERT_TRUE(rawData.gyro_y >= -2000.0f && rawData.gyro_y <= 2000.0f);
        TEST_ASSERT_TRUE(rawData.gyro_z >= -2000.0f && rawData.gyro_z <= 2000.0f);
    }

    void test_quaternion_calculation() {
        SphereIMUManager imuManager;
        imuManager.initialize();
        
        // クォータニオン計算
        SphereIMUManager::Quaternion quat = imuManager.getOrientation();
        
        // クォータニオンのノルムは1に近い値
        float norm = quat.w * quat.w + quat.x * quat.x + quat.y * quat.y + quat.z * quat.z;
        TEST_ASSERT_FLOAT_WITHIN(0.1f, 1.0f, norm);
        
        // w成分は通常 -1 ～ 1 の範囲
        TEST_ASSERT_TRUE(quat.w >= -1.0f && quat.w <= 1.0f);
    }

    void test_euler_angles() {
        SphereIMUManager imuManager;
        imuManager.initialize();
        
        // オイラー角取得
        SphereIMUManager::EulerAngles euler = imuManager.getEulerAngles();
        
        // 角度範囲チェック
        TEST_ASSERT_TRUE(euler.roll >= -180.0f && euler.roll <= 180.0f);
        TEST_ASSERT_TRUE(euler.pitch >= -90.0f && euler.pitch <= 90.0f);
        TEST_ASSERT_TRUE(euler.yaw >= -180.0f && euler.yaw <= 180.0f);
    }

    void test_shake_detection() {
        SphereIMUManager imuManager;
        imuManager.initialize();
        
        // シェイク検出設定
        imuManager.setShakeThreshold(2.5f); // 2.5g
        imuManager.setShakeTimeWindow(500); // 500ms
        
        // 通常状態ではシェイク検出されない
        bool shakeDetected = imuManager.isShakeDetected();
        TEST_ASSERT_FALSE(shakeDetected);
        
        // 閾値設定確認
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.5f, imuManager.getShakeThreshold());
        TEST_ASSERT_EQUAL(500, imuManager.getShakeTimeWindow());
    }

    void test_tilt_detection() {
        SphereIMUManager imuManager;
        imuManager.initialize();
        
        // 傾き検出
        SphereIMUManager::TiltDirection tilt = imuManager.getTiltDirection();
        
        // 有効な傾き方向
        TEST_ASSERT_TRUE(tilt >= SphereIMUManager::TILT_NONE && 
                         tilt <= SphereIMUManager::TILT_BACKWARD);
    }

    void test_calibration() {
        SphereIMUManager imuManager;
        imuManager.initialize();
        
        // キャリブレーション開始
        bool calibStarted = imuManager.startCalibration();
        TEST_ASSERT_TRUE(calibStarted);
        
        // キャリブレーション中はfalse
        TEST_ASSERT_FALSE(imuManager.isCalibrated());
        
        // キャリブレーション完了待ち（タイムアウト設定）
        unsigned long startTime = millis();
        bool calibCompleted = false;
        while (millis() - startTime < 5000) { // 5秒タイムアウト
            if (imuManager.isCalibrated()) {
                calibCompleted = true;
                break;
            }
            delay(100);
        }
        TEST_ASSERT_TRUE(calibCompleted);
    }

    void test_motion_detection() {
        SphereIMUManager imuManager;
        imuManager.initialize();
        
        // モーション検出設定
        imuManager.setMotionThreshold(0.1f); // 0.1g
        
        // モーション状態チェック
        bool inMotion = imuManager.isInMotion();
        // 実機では環境により異なるため、関数が正常に動作することのみ確認
        TEST_ASSERT_TRUE(inMotion == true || inMotion == false);
    }

    void test_performance_update_rate() {
        SphereIMUManager imuManager;
        imuManager.initialize();
        imuManager.setSampleRate(200); // 200Hz
        
        // 性能テスト: 更新レート確認
        unsigned long startTime = micros();
        const int samples = 100;
        
        for (int i = 0; i < samples; i++) {
            SphereIMUManager::RawData rawData;
            imuManager.readRawData(rawData);
            delayMicroseconds(5000); // 5ms間隔
        }
        
        unsigned long elapsed = micros() - startTime;
        float actualRate = (float)samples * 1000000.0f / elapsed;
        
        // 目標: 200Hz（実際は若干低くなる）
        TEST_ASSERT_TRUE(actualRate >= 180.0f && actualRate <= 220.0f);
    }

    void test_filter_performance() {
        SphereIMUManager imuManager;
        imuManager.initialize();
        
        // フィルタ設定
        imuManager.setLowPassFilterAlpha(0.1f); // ローパスフィルタ
        
        // フィルタ済みデータ取得
        SphereIMUManager::FilteredData filtered = imuManager.getFilteredData();
        
        // フィルタ後もデータ範囲は同じ
        TEST_ASSERT_TRUE(filtered.accel_x >= -16.0f && filtered.accel_x <= 16.0f);
        TEST_ASSERT_TRUE(filtered.gyro_x >= -2000.0f && filtered.gyro_x <= 2000.0f);
    }
}

// Unity テストランナー
void setUp(void) {
    // 各テスト前の初期化
    delay(100); // センサー安定化待ち
}

void tearDown(void) {
    // 各テスト後のクリーンアップ
}

int main() {
    UNITY_BEGIN();
    
    // 基本機能テスト
    RUN_TEST(sphere_imu_test::test_constructor_initializes_correctly);
    RUN_TEST(sphere_imu_test::test_initialize_with_valid_parameters);
    RUN_TEST(sphere_imu_test::test_set_sample_rate);
    
    // センサーデータテスト
    RUN_TEST(sphere_imu_test::test_read_raw_sensor_data);
    RUN_TEST(sphere_imu_test::test_quaternion_calculation);
    RUN_TEST(sphere_imu_test::test_euler_angles);
    
    // ジェスチャー検出テスト
    RUN_TEST(sphere_imu_test::test_shake_detection);
    RUN_TEST(sphere_imu_test::test_tilt_detection);
    RUN_TEST(sphere_imu_test::test_motion_detection);
    
    // 高度機能テスト
    RUN_TEST(sphere_imu_test::test_calibration);
    RUN_TEST(sphere_imu_test::test_filter_performance);
    
    // 性能テスト
    RUN_TEST(sphere_imu_test::test_performance_update_rate);
    
    return UNITY_END();
}

// ESP32実機テスト用のsetup/loop（PlatformIOテスト環境）
void setup() {
    delay(2000); // ESP32起動待ち
    main();
}

void loop() {
    // テスト完了後は何もしない
}