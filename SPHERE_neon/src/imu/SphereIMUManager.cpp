#include "imu/SphereIMUManager.h"
#include <esp_log.h>
#include <cmath>

static const char* TAG = "SphereIMUManager";

// Madgwickフィルタ用定数
static const float MADGWICK_BETA = 0.1f; // フィルタゲイン

SphereIMUManager::SphereIMUManager()
    : initialized(false)
    , calibrated(false)
    , sample_rate(100)
    , lowpass_alpha(0.1f)
    , shake_threshold(2.0f)
    , shake_time_window(500)
    , motion_threshold(0.1f)
    , shake_buffer_index(0)
    , last_shake_time(0)
    , step_count(0)
    , current_activity(ACTIVITY_STILL)
    , high_g_threshold(8.0f)
    , low_g_threshold(0.2f)
    , orientation_hysteresis(32)
    , wrist_gesture_enabled(false)
    , tap_sensitivity(0.5f)
    , double_tap_enabled(false) {
    
    // フィルタ済みデータ初期化
    memset(&filtered_data, 0, sizeof(filtered_data));
    
    // オフセット初期化
    memset(accel_offset, 0, sizeof(accel_offset));
    memset(gyro_offset, 0, sizeof(gyro_offset));
    
    // シェイクバッファ初期化
    memset(shake_buffer, 0, sizeof(shake_buffer));
    
    // BMI270特殊機能初期化
    memset(features_enabled, false, sizeof(features_enabled));
}

SphereIMUManager::~SphereIMUManager() {
    // 特別なクリーンアップは不要（M5Unifiedが管理）
}

bool SphereIMUManager::initialize() {
    if (initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }
    
    // M5UnifiedのIMU初期化確認
    if (!M5.Imu.isEnabled()) {
        ESP_LOGE(TAG, "M5Unified IMU not enabled");
        return false;
    }
    
    // BMI270固有設定
    if (!configureBMI270()) {
        ESP_LOGE(TAG, "BMI270 configuration failed");
        return false;
    }
    
    // 初期キャリブレーション実行
    if (!startCalibration()) {
        ESP_LOGE(TAG, "Initial calibration failed");
        return false;
    }
    
    // 初期クォータニオン設定
    current_quaternion = Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    
    initialized = true;
    ESP_LOGI(TAG, "Initialized successfully - Sample rate: %d Hz", sample_rate);
    return true;
}

bool SphereIMUManager::configureBMI270() {
    // M5Unifiedを通じてBMI270を設定
    // 具体的な設定はM5Unifiedライブラリが管理
    
    // サンプリングレート設定（M5Unifiedの制約内で）
    // 実際の設定は次のsetSampleRateで行う
    return setSampleRate(sample_rate);
}

bool SphereIMUManager::setSampleRate(uint16_t rate) {
    // サポートされるレート確認
    if (rate != 25 && rate != 50 && rate != 100 && rate != 200 && rate != 400) {
        ESP_LOGE(TAG, "Unsupported sample rate: %d", rate);
        return false;
    }
    
    sample_rate = rate;
    ESP_LOGI(TAG, "Sample rate set to %d Hz", sample_rate);
    return true;
}

uint16_t SphereIMUManager::getSampleRate() const {
    return sample_rate;
}

bool SphereIMUManager::readRawData(RawData& data) {
    if (!initialized) {
        return false;
    }
    
    // M5UnifiedからIMUデータ取得
    auto imu_data = M5.Imu.getImuData();
    
    // タイムスタンプ
    data.timestamp = micros();
    
    // 加速度データ（g単位）
    data.accel_x = imu_data.accel.x - accel_offset[0];
    data.accel_y = imu_data.accel.y - accel_offset[1];
    data.accel_z = imu_data.accel.z - accel_offset[2];
    
    // 角速度データ（dps単位）
    data.gyro_x = imu_data.gyro.x - gyro_offset[0];
    data.gyro_y = imu_data.gyro.y - gyro_offset[1];
    data.gyro_z = imu_data.gyro.z - gyro_offset[2];
    
    // 温度データ（M5UnifiedのIMUデータ構造体には温度フィールドがない）
    data.temp = 25.0f; // デフォルト値
    
    // フィルタリング適用
    applyLowPassFilter(data);
    
    return true;
}

void SphereIMUManager::applyLowPassFilter(const RawData& raw) {
    // ローパスフィルタ: filtered = α * raw + (1-α) * previous
    filtered_data.accel_x = lowpass_alpha * raw.accel_x + (1.0f - lowpass_alpha) * filtered_data.accel_x;
    filtered_data.accel_y = lowpass_alpha * raw.accel_y + (1.0f - lowpass_alpha) * filtered_data.accel_y;
    filtered_data.accel_z = lowpass_alpha * raw.accel_z + (1.0f - lowpass_alpha) * filtered_data.accel_z;
    
    filtered_data.gyro_x = lowpass_alpha * raw.gyro_x + (1.0f - lowpass_alpha) * filtered_data.gyro_x;
    filtered_data.gyro_y = lowpass_alpha * raw.gyro_y + (1.0f - lowpass_alpha) * filtered_data.gyro_y;
    filtered_data.gyro_z = lowpass_alpha * raw.gyro_z + (1.0f - lowpass_alpha) * filtered_data.gyro_z;
}

const SphereIMUManager::FilteredData& SphereIMUManager::getFilteredData() const {
    return filtered_data;
}

void SphereIMUManager::updateQuaternion(const FilteredData& data, float dt) {
    // Madgwickフィルタでクォータニオン更新
    float gx = data.gyro_x * M_PI / 180.0f; // rad/s変換
    float gy = data.gyro_y * M_PI / 180.0f;
    float gz = data.gyro_z * M_PI / 180.0f;
    
    float ax = data.accel_x;
    float ay = data.accel_y;
    float az = data.accel_z;
    
    // 加速度正規化
    float norm = sqrt(ax*ax + ay*ay + az*az);
    if (norm > 0.0001f) {
        ax /= norm;
        ay /= norm;
        az /= norm;
    }
    
    // 現在のクォータニオン
    float q0 = current_quaternion.w;
    float q1 = current_quaternion.x;
    float q2 = current_quaternion.y;
    float q3 = current_quaternion.z;
    
    // 重力ベクトル（クォータニオンから推定）
    float _2q0 = 2.0f * q0;
    float _2q1 = 2.0f * q1;
    float _2q2 = 2.0f * q2;
    float _2q3 = 2.0f * q3;
    float _4q0 = 4.0f * q0;
    float _4q1 = 4.0f * q1;
    float _4q2 = 4.0f * q2;
    float _8q1 = 8.0f * q1;
    float _8q2 = 8.0f * q2;
    float q0q0 = q0 * q0;
    float q1q1 = q1 * q1;
    float q2q2 = q2 * q2;
    float q3q3 = q3 * q3;
    
    // 推定重力方向
    float s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
    float s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * q1 - _2q0 * ay - _4q1 + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
    float s2 = 4.0f * q0q0 * q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
    float s3 = 4.0f * q1q1 * q3 - _2q1 * ax + 4.0f * q2q2 * q3 - _2q2 * ay;
    
    // 正規化
    norm = sqrt(s0*s0 + s1*s1 + s2*s2 + s3*s3);
    if (norm > 0.0001f) {
        s0 /= norm;
        s1 /= norm;
        s2 /= norm;
        s3 /= norm;
    }
    
    // ジャイロ統合
    float qDot1 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
    float qDot2 = 0.5f * (q0 * gx + q2 * gz - q3 * gy);
    float qDot3 = 0.5f * (q0 * gy - q1 * gz + q3 * gx);
    float qDot4 = 0.5f * (q0 * gz + q1 * gy - q2 * gx);
    
    // フィードバック適用
    qDot1 -= MADGWICK_BETA * s0;
    qDot2 -= MADGWICK_BETA * s1;
    qDot3 -= MADGWICK_BETA * s2;
    qDot4 -= MADGWICK_BETA * s3;
    
    // 積分
    q0 += qDot1 * dt;
    q1 += qDot2 * dt;
    q2 += qDot3 * dt;
    q3 += qDot4 * dt;
    
    // 正規化
    norm = sqrt(q0*q0 + q1*q1 + q2*q2 + q3*q3);
    if (norm > 0.0001f) {
        current_quaternion.w = q0 / norm;
        current_quaternion.x = q1 / norm;
        current_quaternion.y = q2 / norm;
        current_quaternion.z = q3 / norm;
    }
    
    // オイラー角更新
    quaternionToEuler(current_quaternion, current_euler);
}

void SphereIMUManager::quaternionToEuler(const Quaternion& q, EulerAngles& euler) {
    // クォータニオン → オイラー角変換
    float sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
    float cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
    euler.roll = atan2(sinr_cosp, cosr_cosp) * 180.0f / M_PI;
    
    float sinp = 2 * (q.w * q.y - q.z * q.x);
    if (abs(sinp) >= 1) {
        euler.pitch = copysign(M_PI / 2, sinp) * 180.0f / M_PI;
    } else {
        euler.pitch = asin(sinp) * 180.0f / M_PI;
    }
    
    float siny_cosp = 2 * (q.w * q.z + q.x * q.y);
    float cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
    euler.yaw = atan2(siny_cosp, cosy_cosp) * 180.0f / M_PI;
}

const SphereIMUManager::Quaternion& SphereIMUManager::getOrientation() const {
    return current_quaternion;
}

const SphereIMUManager::EulerAngles& SphereIMUManager::getEulerAngles() const {
    return current_euler;
}

bool SphereIMUManager::startCalibration() {
    ESP_LOGI(TAG, "Starting calibration...");
    
    float accel_sum[3] = {0, 0, 0};
    float gyro_sum[3] = {0, 0, 0};
    
    // 複数サンプル取得してオフセット計算
    for (size_t i = 0; i < CALIB_SAMPLES; i++) {
        auto imu_data = M5.Imu.getImuData();
        
        accel_sum[0] += imu_data.accel.x;
        accel_sum[1] += imu_data.accel.y;
        accel_sum[2] += imu_data.accel.z;
        
        gyro_sum[0] += imu_data.gyro.x;
        gyro_sum[1] += imu_data.gyro.y;
        gyro_sum[2] += imu_data.gyro.z;
        
        delay(10); // 10ms間隔
    }
    
    // 平均値をオフセットとして設定
    accel_offset[0] = accel_sum[0] / CALIB_SAMPLES;
    accel_offset[1] = accel_sum[1] / CALIB_SAMPLES;
    accel_offset[2] = accel_sum[2] / CALIB_SAMPLES - 1.0f; // 重力補正
    
    gyro_offset[0] = gyro_sum[0] / CALIB_SAMPLES;
    gyro_offset[1] = gyro_sum[1] / CALIB_SAMPLES;
    gyro_offset[2] = gyro_sum[2] / CALIB_SAMPLES;
    
    calibrated = true;
    ESP_LOGI(TAG, "Calibration completed");
    ESP_LOGI(TAG, "Accel offset: %.3f, %.3f, %.3f", accel_offset[0], accel_offset[1], accel_offset[2]);
    ESP_LOGI(TAG, "Gyro offset: %.3f, %.3f, %.3f", gyro_offset[0], gyro_offset[1], gyro_offset[2]);
    
    return true;
}

bool SphereIMUManager::isCalibrated() const {
    return calibrated;
}

bool SphereIMUManager::isInitialized() const {
    return initialized;
}

void SphereIMUManager::setShakeThreshold(float threshold) {
    shake_threshold = threshold;
}

void SphereIMUManager::setShakeTimeWindow(uint32_t timeWindow) {
    shake_time_window = timeWindow;
}

bool SphereIMUManager::detectShake(float magnitude) {
    // シェイクバッファに追加
    shake_buffer[shake_buffer_index] = magnitude;
    shake_buffer_index = (shake_buffer_index + 1) % SHAKE_BUFFER_SIZE;
    
    // 閾値超過チェック
    if (magnitude > shake_threshold) {
        unsigned long current_time = millis();
        if (current_time - last_shake_time > shake_time_window) {
            last_shake_time = current_time;
            return true;
        }
    }
    
    return false;
}

bool SphereIMUManager::isShakeDetected() {
    if (!initialized) {
        return false;
    }
    
    RawData data;
    if (!readRawData(data)) {
        return false;
    }
    
    // 加速度の大きさ計算
    float magnitude = sqrt(data.accel_x * data.accel_x + 
                          data.accel_y * data.accel_y + 
                          data.accel_z * data.accel_z);
    
    return detectShake(magnitude);
}

float SphereIMUManager::getShakeThreshold() const {
    return shake_threshold;
}

uint32_t SphereIMUManager::getShakeTimeWindow() const {
    return shake_time_window;
}

SphereIMUManager::TiltDirection SphereIMUManager::calculateTiltDirection(const EulerAngles& euler) {
    const float TILT_THRESHOLD = 30.0f; // 30度
    
    if (abs(euler.pitch) > abs(euler.roll)) {
        if (euler.pitch > TILT_THRESHOLD) {
            return TILT_FORWARD;
        } else if (euler.pitch < -TILT_THRESHOLD) {
            return TILT_BACKWARD;
        }
    } else {
        if (euler.roll > TILT_THRESHOLD) {
            return TILT_RIGHT;
        } else if (euler.roll < -TILT_THRESHOLD) {
            return TILT_LEFT;
        }
    }
    
    return TILT_NONE;
}

SphereIMUManager::TiltDirection SphereIMUManager::getTiltDirection() {
    return calculateTiltDirection(current_euler);
}

void SphereIMUManager::setMotionThreshold(float threshold) {
    motion_threshold = threshold;
}

bool SphereIMUManager::isInMotion() {
    if (!initialized) {
        return false;
    }
    
    // フィルタ済み加速度の変化量で判定
    float motion_magnitude = sqrt(filtered_data.accel_x * filtered_data.accel_x + 
                                 filtered_data.accel_y * filtered_data.accel_y + 
                                 filtered_data.accel_z * filtered_data.accel_z);
    
    // 重力(1g)からの差分で動作判定
    float motion_delta = abs(motion_magnitude - 1.0f);
    
    return motion_delta > motion_threshold;
}

void SphereIMUManager::setLowPassFilterAlpha(float alpha) {
    lowpass_alpha = constrain(alpha, 0.0f, 1.0f);
}

void SphereIMUManager::update() {
    if (!initialized) {
        return;
    }
    
    static unsigned long last_update = 0;
    unsigned long current_time = micros();
    float dt = (current_time - last_update) / 1000000.0f; // 秒変換
    
    if (dt > 0.001f) { // 最小更新間隔1ms
        RawData raw_data;
        if (readRawData(raw_data)) {
            updateQuaternion(filtered_data, dt);
        }
        last_update = current_time;
    }
}

void SphereIMUManager::printDebugInfo() const {
    if (!initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "=== IMU Debug Info ===");
    ESP_LOGI(TAG, "Initialized: %s, Calibrated: %s", 
             initialized ? "YES" : "NO", calibrated ? "YES" : "NO");
    ESP_LOGI(TAG, "Sample Rate: %d Hz", sample_rate);
    
    ESP_LOGI(TAG, "Filtered Accel: %.3f, %.3f, %.3f", 
             filtered_data.accel_x, filtered_data.accel_y, filtered_data.accel_z);
    ESP_LOGI(TAG, "Filtered Gyro: %.3f, %.3f, %.3f", 
             filtered_data.gyro_x, filtered_data.gyro_y, filtered_data.gyro_z);
    
    ESP_LOGI(TAG, "Quaternion: %.3f, %.3f, %.3f, %.3f", 
             current_quaternion.w, current_quaternion.x, current_quaternion.y, current_quaternion.z);
    ESP_LOGI(TAG, "Euler: Roll=%.1f°, Pitch=%.1f°, Yaw=%.1f°", 
             current_euler.roll, current_euler.pitch, current_euler.yaw);
    
    ESP_LOGI(TAG, "Shake Threshold: %.2fg, Motion Threshold: %.2fg", 
             shake_threshold, motion_threshold);
}

// =============BMI270特殊機能実装============= //

bool SphereIMUManager::enableFeature(BMI270Feature feature, bool enable) {
    if (!initialized) {
        ESP_LOGE(TAG, "IMU not initialized");
        return false;
    }
    
    if (feature >= 10) { // 配列範囲チェック
        ESP_LOGE(TAG, "Invalid feature: %d", feature);
        return false;
    }
    
    features_enabled[feature] = enable;
    
    // 実際のBMI270への設定は、M5Unifiedの制約により
    // 一部機能のみサポート。将来的に直接レジスタアクセスで拡張可能。
    switch (feature) {
        case FEATURE_STEP_COUNTER:
            ESP_LOGI(TAG, "Step counter %s", enable ? "enabled" : "disabled");
            break;
            
        case FEATURE_ANY_MOTION:
            ESP_LOGI(TAG, "Any motion detection %s", enable ? "enabled" : "disabled");
            break;
            
        case FEATURE_NO_MOTION:
            ESP_LOGI(TAG, "No motion detection %s", enable ? "enabled" : "disabled");
            break;
            
        default:
            ESP_LOGW(TAG, "Feature %d not fully supported via M5Unified", feature);
            break;
    }
    
    return true;
}

uint32_t SphereIMUManager::getStepCount() {
    if (!features_enabled[FEATURE_STEP_COUNTER]) {
        return 0;
    }
    
    // 簡易歩数計算アルゴリズム（実際のBMI270では内蔵機能を使用）
    // この実装では加速度の変化から歩行を推定
    static uint32_t last_step_time = 0;
    static float last_magnitude = 0;
    
    float magnitude = sqrt(filtered_data.accel_x * filtered_data.accel_x + 
                          filtered_data.accel_y * filtered_data.accel_y + 
                          filtered_data.accel_z * filtered_data.accel_z);
    
    uint32_t current_time = millis();
    
    // 歩行パターン検出（簡易版）
    if (current_time - last_step_time > 300) { // 最小歩行間隔
        float magnitude_diff = abs(magnitude - last_magnitude);
        if (magnitude_diff > 0.5f && magnitude > 0.8f && magnitude < 1.5f) {
            step_count++;
            last_step_time = current_time;
            ESP_LOGD(TAG, "Step detected: %lu", step_count);
        }
    }
    
    last_magnitude = magnitude;
    return step_count;
}

bool SphereIMUManager::resetStepCount() {
    step_count = 0;
    ESP_LOGI(TAG, "Step count reset");
    return true;
}

SphereIMUManager::ActivityState SphereIMUManager::getActivityState() {
    if (!initialized) {
        return ACTIVITY_UNKNOWN;
    }
    
    // 加速度データから活動状態を推定
    float magnitude = sqrt(filtered_data.accel_x * filtered_data.accel_x + 
                          filtered_data.accel_y * filtered_data.accel_y + 
                          filtered_data.accel_z * filtered_data.accel_z);
    
    float motion_level = abs(magnitude - 1.0f); // 重力から偏差
    
    if (motion_level < 0.1f) {
        current_activity = ACTIVITY_STILL;
    } else if (motion_level < 0.3f) {
        current_activity = ACTIVITY_WALKING;
    } else {
        current_activity = ACTIVITY_RUNNING;
    }
    
    return current_activity;
}

void SphereIMUManager::setHighGThreshold(float threshold) {
    high_g_threshold = threshold;
    ESP_LOGI(TAG, "High-G threshold set to %.2fg", threshold);
}

void SphereIMUManager::setLowGThreshold(float threshold) {
    low_g_threshold = threshold;
    ESP_LOGI(TAG, "Low-G threshold set to %.2fg", threshold);
}

void SphereIMUManager::setOrientationHysteresis(uint16_t hysteresis) {
    orientation_hysteresis = hysteresis;
    ESP_LOGI(TAG, "Orientation hysteresis set to %d", hysteresis);
}

void SphereIMUManager::enableWristGesture(bool enable) {
    wrist_gesture_enabled = enable;
    ESP_LOGI(TAG, "Wrist gesture %s", enable ? "enabled" : "disabled");
}

void SphereIMUManager::setTapSensitivity(float sensitivity) {
    tap_sensitivity = constrain(sensitivity, 0.0f, 1.0f);
    ESP_LOGI(TAG, "Tap sensitivity set to %.2f", tap_sensitivity);
}

void SphereIMUManager::enableDoubleTap(bool enable) {
    double_tap_enabled = enable;
    ESP_LOGI(TAG, "Double tap %s", enable ? "enabled" : "disabled");
}

bool SphereIMUManager::isSingleTapDetected() {
    if (!wrist_gesture_enabled) {
        return false;
    }
    
    // 簡易タップ検出（加速度スパイクを検出）
    static unsigned long last_tap_time = 0;
    unsigned long current_time = millis();
    
    float magnitude = sqrt(filtered_data.accel_x * filtered_data.accel_x + 
                          filtered_data.accel_y * filtered_data.accel_y + 
                          filtered_data.accel_z * filtered_data.accel_z);
    
    float tap_threshold = 2.0f + (1.0f - tap_sensitivity) * 2.0f; // 2.0g〜4.0g
    
    if (magnitude > tap_threshold && (current_time - last_tap_time) > 200) {
        last_tap_time = current_time;
        ESP_LOGD(TAG, "Single tap detected (%.2fg)", magnitude);
        return true;
    }
    
    return false;
}

bool SphereIMUManager::isDoubleTapDetected() {
    if (!double_tap_enabled) {
        return false;
    }
    
    // ダブルタップ検出（2回のタップ間隔を確認）
    static unsigned long first_tap_time = 0;
    static bool waiting_for_second_tap = false;
    
    if (isSingleTapDetected()) {
        unsigned long current_time = millis();
        
        if (!waiting_for_second_tap) {
            first_tap_time = current_time;
            waiting_for_second_tap = true;
        } else {
            if ((current_time - first_tap_time) < 500) { // 500ms以内
                waiting_for_second_tap = false;
                ESP_LOGD(TAG, "Double tap detected");
                return true;
            } else {
                first_tap_time = current_time; // リセット
            }
        }
    }
    
    // タイムアウト処理
    if (waiting_for_second_tap && (millis() - first_tap_time) > 500) {
        waiting_for_second_tap = false;
    }
    
    return false;
}

bool SphereIMUManager::isSignificantMotionDetected() {
    if (!features_enabled[FEATURE_SIG_MOTION]) {
        return false;
    }
    
    // 有意な動作の検出（長期間の動きの変化）
    static float motion_accumulator = 0.0f;
    static unsigned long last_check_time = 0;
    
    unsigned long current_time = millis();
    float dt = (current_time - last_check_time) / 1000.0f;
    
    if (dt > 0.1f) { // 100ms間隔でチェック
        float current_motion = sqrt(filtered_data.accel_x * filtered_data.accel_x + 
                                  filtered_data.accel_y * filtered_data.accel_y + 
                                  filtered_data.accel_z * filtered_data.accel_z);
        
        float motion_delta = abs(current_motion - 1.0f); // 重力からの偏差
        motion_accumulator = motion_accumulator * 0.9f + motion_delta * 0.1f; // 指数移動平均
        
        last_check_time = current_time;
    }
    
    return motion_accumulator > 0.3f; // 閾値以上で有意な動作と判定
}

bool SphereIMUManager::isFlatDetected() {
    if (!features_enabled[FEATURE_FLAT]) {
        return false;
    }
    
    // 平面検出（Z軸が重力方向に近いかチェック）
    float z_magnitude = abs(filtered_data.accel_z);
    float xy_magnitude = sqrt(filtered_data.accel_x * filtered_data.accel_x + 
                             filtered_data.accel_y * filtered_data.accel_y);
    
    // デバイスが平らに置かれている場合、Z軸が約1g、X,Y軸が約0g
    return (z_magnitude > 0.9f && xy_magnitude < 0.2f);
}

bool SphereIMUManager::isFreeFallDetected() {
    if (!features_enabled[FEATURE_LOW_G]) {
        return false;
    }
    
    // 自由落下検出（全軸の加速度が低G閾値以下）
    float total_magnitude = sqrt(filtered_data.accel_x * filtered_data.accel_x + 
                                filtered_data.accel_y * filtered_data.accel_y + 
                                filtered_data.accel_z * filtered_data.accel_z);
    
    return total_magnitude < low_g_threshold;
}

float SphereIMUManager::getCalibratedTemperature() {
    if (!initialized) {
        return 0.0f;
    }
    
    float temp = 0.0f;
    if (!M5.Imu.getTemp(&temp)) {
        return 0.0f;
    }

    // BMI270 の温度は内部オフセットを持つため簡易補正を加える
    return temp + 23.0f;
}

bool SphereIMUManager::setPowerMode(bool low_power) {
    if (!initialized) {
        return false;
    }
    
    // M5Unifiedを通じた電力管理は制限的
    // 実際のBMI270では、レジスタ直接アクセスで詳細制御が可能
    ESP_LOGI(TAG, "Power mode set to %s", low_power ? "low power" : "normal");
    
    if (low_power) {
        // 低電力モードではサンプリングレートを下げる
        setSampleRate(25);
    } else {
        // 通常モードでは標準サンプリングレート
        setSampleRate(100);
    }
    
    return true;
}

size_t SphereIMUManager::readFifoData(RawData* buffer, size_t buffer_size) {
    if (!initialized || !buffer || buffer_size == 0) {
        return 0;
    }
    
    // M5UnifiedではFIFO機能が制限的
    // 単一データ読み取りで代替
    if (buffer_size >= 1) {
        if (readRawData(buffer[0])) {
            return 1;
        }
    }
    
    return 0;
}
