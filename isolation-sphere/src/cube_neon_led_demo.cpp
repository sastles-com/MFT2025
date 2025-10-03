/**
 * @file cube_neon_led_demo.cpp
 * @brief CUBE-neon実績実装 + パノラマ画像LEDマッピングデモ
 * 
 * AGENTS.md準拠:
 * - パノラマ画像320x160
 * - PSRAM allocator使用
 * - xyz-uv変換性能に焦点
 */

#include <Arduino.h>
#include "math/fast_math.h"
#include <M5Unified.h>
#include <FastLED.h>

using namespace FastMath;

// IMUデータ構造体
struct IMUData {
    float accelX, accelY, accelZ;  // 加速度 [m/s^2]
    float gyroX, gyroY, gyroZ;     // 角速度 [rad/s]
    float temp;                    // 温度 [C]
    uint32_t timestamp;            // タイムスタンプ [ms]
};

// クォータニオン構造体
struct Quaternion {
    float w, x, y, z;
    
    Qua    // RGB バッファ生成（黒背景 + 赤ライン）
    generateRGBBuffer(rgbBuffer);
    
    Serial.printf("UV変換デバッグ: 320x160バッファ、赤ライン高さ=%dpx\n", RED_LINE_HEIGHT);rnion(float w = 1.0f, float x = 0.0f, float y = 0.0f, float z = 0.0f) 
        : w(w), x(x), y(y), z(z) {}
};

// RGB バッファ設定 (AGENTS.md準拠: 320x160)
#define RGB_BUFFER_WIDTH 320   // RGBバッファ幅 (ピクセル)
#define RGB_BUFFER_HEIGHT 160  // RGBバッファ高 (ピクセル)
#define RED_LINE_HEIGHT 80     // 赤ラインの高さ (ピクセル) - 中央

// RGB ピクセルデータ構造体
struct RGBPixel {
    uint8_t r, g, b;
    RGBPixel(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0) 
        : r(red), g(green), b(blue) {}
};

// グローバル変数
IMUData currentIMU;
Quaternion currentRotation;
bool imuInitialized = false;

// 前方宣言
void demonstrateCubeNeonPipeline();
bool initializeIMU();
bool readIMUData(IMUData& data);
Quaternion integrateGyroscope(const IMUData& imu, const Quaternion& prev, float deltaTime);
void updateLEDsWithIMU(const Quaternion& rotation);
void generateRGBBuffer(RGBPixel* buffer);
RGBPixel sampleRGBBuffer(const RGBPixel* buffer, float u, float v);
void updateLEDsWithRGBBuffer(const RGBPixel* rgbBuffer, const Quaternion& rotation);
void demonstrateRGBBufferDemo();

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

// FastLED配列
CRGB leds[TOTAL_LEDS];

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("========================================");
    Serial.println("CUBE-neon + LED表示統合デモ");
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
    
    // IMU初期化（M5Stack AtomS3R: BMI270）
    Serial.println("\n[2] IMU初期化（BMI270）");
    imuInitialized = initializeIMU();
    if (imuInitialized) {
        Serial.println("IMU初期化成功: BMI270センサー準備完了");
        
        // 初期姿勢設定（単位クォータニオン）
        currentRotation = Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    } else {
        Serial.println("警告: IMU初期化失敗 - シミュレーションモードで継続");
    }
    
    // LEDSphereManager初期化（一時的にコメントアウト）
    Serial.println("\n[3] LEDSphereManager初期化（スキップ）");
    Serial.println("注意: LEDSphereManager統合は次段階で実装");
    /*
    LEDSphereManager* manager = SpherePatternInterface::getInstance();
    if (!manager->initialize("data/led_layout.csv")) {
        Serial.println("警告: CSVファイル読み込み失敗 - プロシージャル生成で継続");
    }
    */
    
    // 高速数学関数パフォーマンステスト
    Serial.println("\n[4] CUBE-neon高速数学関数テスト");
    const int iterations = 1000;
    unsigned long start, end;
    
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
    Serial.printf("atan2f    : %lu μs (%d回)\n", end - start, iterations);
    
    float improvement = (start > 0) ? (float)(end - start) / (float)start : 1.0f;
    Serial.printf("改善率: %.1fx高速化\n", improvement);
    
    // xyz-uv変換専用性能テスト（AGENTS.md準拠）
    Serial.println("\n[4.5] xyz-uv変換性能テスト（AGENTS.md要件）");
    const int coord_iterations = 800;  // 実LED数相当
    
    // 標準数学関数使用版
    start = micros();
    for (int i = 0; i < coord_iterations; i++) {
        float x = sinf(i * 0.01f), y = cosf(i * 0.01f), z = sinf(i * 0.02f);
        float rxy = sqrtf(x * x + z * z);
        volatile float u = atan2f(rxy, y);
        volatile float v = atan2f(x, z);
    }
    end = micros();
    unsigned long standard_time = end - start;
    Serial.printf("標準関数xyz-uv: %lu μs (%d座標)\n", standard_time, coord_iterations);
    
    // CUBE-neon高速関数使用版
    start = micros();
    for (int i = 0; i < coord_iterations; i++) {
        float x = sinf(i * 0.01f), y = cosf(i * 0.01f), z = sinf(i * 0.02f);
        float rxy = fast_sqrt(x * x + z * z);
        volatile float u = fast_atan2(rxy, y);
        volatile float v = fast_atan2(x, z);
    }
    end = micros();
    unsigned long fast_time = end - start;
    Serial.printf("CUBE-neon xyz-uv: %lu μs (%d座標)\n", fast_time, coord_iterations);
    
    float coord_improvement = (float)standard_time / (float)fast_time;
    Serial.printf("xyz-uv変換改善率: %.1fx高速化\n", coord_improvement);
    Serial.printf("LED更新レート向上: %.1fHz → %.1fHz\n", 
                  1000000.0f / standard_time, 1000000.0f / fast_time);
    
    // CUBE-neon座標変換デモ表示
    Serial.println("\n[5] CUBE-neon座標変換→LED表示デモ");
    demonstrateCubeNeonPipeline();
    
    // RGBバッファデモ表示
    Serial.println("\n[6] RGBバッファ→LED表示デモ");
    demonstrateRGBBufferDemo();
    
    Serial.println("\n[7] リアルタイムIMU/LED表示開始");
    Serial.println("========================================");
}

void demonstrateCubeNeonPipeline() {
    // テスト用クォータニオン（IMUシミュレート）
    float qw = 1.0f, qx = 0.0f, qy = 0.0f, qz = 0.0f;  // 無回転から開始
    
    Serial.println("CUBE-neon座標変換パイプライン→LED表示:");
    
    // 球面上の基本方向をLEDで表示
    for (int demo = 0; demo < 3; demo++) {
        // クォータニオン更新（回転アニメーション）
        float angle = demo * M_PI / 6.0f;  // 30度ずつ回転
        qw = cos(angle / 2.0f);
        qx = sin(angle / 2.0f);
        qy = 0.0f;
        qz = 0.0f;
        
        Serial.printf("\nデモ%d: %.0f度回転\n", demo + 1, angle * 180.0f / M_PI);
        
        // LED配列クリア
        FastLED.clear();
        
        // プロシージャル球面座標→LED表示
        for (int ledIndex = 0; ledIndex < TOTAL_LEDS; ledIndex++) {
            // プロシージャル球面座標生成（実際はled_layout.csvから読み込み）
            float theta = (float)ledIndex / TOTAL_LEDS * 2.0f * M_PI;  // 経度
            float phi = acos(1.0f - 2.0f * ((float)(ledIndex % 400) / 400.0f));  // 緯度
            
            // 球面座標→直交座標
            float x = sin(phi) * cos(theta);
            float y = cos(phi);
            float z = sin(phi) * sin(theta);
            
            // CUBE-neonクォータニオン回転適用
            float norm = fast_sqrt(qw*qw + qx*qx + qy*qy + qz*qz);
            if (norm > 0.0001f) {
                float qw_n = qw / norm, qx_n = qx / norm, qy_n = qy / norm, qz_n = qz / norm;
                
                float qw2 = qw_n * qw_n, qx2 = qx_n * qx_n, qy2 = qy_n * qy_n, qz2 = qz_n * qz_n;
                float rotX = (qw2 + qx2 - qy2 - qz2) * x + 2.0f * (qx_n*qy_n - qw_n*qz_n) * y + 2.0f * (qx_n*qz_n + qw_n*qy_n) * z;
                float rotY = 2.0f * (qx_n*qy_n + qw_n*qz_n) * x + (qw2 - qx2 + qy2 - qz2) * y + 2.0f * (qy_n*qz_n - qw_n*qx_n) * z;
                float rotZ = 2.0f * (qx_n*qz_n - qw_n*qy_n) * x + 2.0f * (qy_n*qz_n + qw_n*qx_n) * y + (qw2 - qx2 - qy2 + qz2) * z;
                
                // UV座標変換（CUBE-neon方式）
                float rxy = fast_sqrt(rotX * rotX + rotZ * rotZ);
                float u = fast_atan2(rxy, rotY);
                float v = fast_atan2(rotX, rotZ);
                
                // UV→RGB変換
                float norm_u = (u + M_PI/2.0f) / M_PI;
                float norm_v = (v + M_PI) / (2.0f * M_PI);
                
                // クランプ
                norm_u = constrain(norm_u, 0.0f, 1.0f);
                norm_v = constrain(norm_v, 0.0f, 1.0f);
                
                // HSV色生成
                uint8_t hue = (uint8_t)(norm_v * 255);
                uint8_t sat = 255;
                uint8_t val = (uint8_t)(norm_u * 200 + 55);  // 最小輝度55
                
                // FastLEDに設定
                if (ledIndex < TOTAL_LEDS) {
                    leds[ledIndex].setHSV(hue, sat, val);
                }
            }
        }
        
        // LED表示更新
        FastLED.show();
        Serial.printf("LED表示更新: %d個のLED\n", TOTAL_LEDS);
        
        delay(2000);  // 2秒表示
    }
}

void loop() {
    static uint32_t lastUpdate = 0;
    static uint32_t lastIMUUpdate = 0;
    static float rotation = 0.0f;
    
    // IMUデータ更新（33ms間隔 ≈ 30Hz）
    if (imuInitialized && (millis() - lastIMUUpdate >= 33)) {
        IMUData newIMU;
        if (readIMUData(newIMU)) {
            float deltaTime = (millis() - lastIMUUpdate) / 1000.0f;  // 秒
            
            // ジャイロスコープからクォータニオン積分
            currentRotation = integrateGyroscope(newIMU, currentRotation, deltaTime);
            currentIMU = newIMU;
            
            lastIMUUpdate = millis();
        }
    }
    
    if (millis() - lastUpdate > 50) {  // 20FPS更新
        static int updateCount = 0;
        updateCount++;
        
        if (imuInitialized) {
            // IMUベースのLED更新
            updateLEDsWithIMU(currentRotation);
            
            if (updateCount % 40 == 0) {  // 2秒ごとにIMU情報出力
                Serial.printf("IMU: 加速度[%.2f,%.2f,%.2f] ジャイロ[%.2f,%.2f,%.2f] 温度:%.1fC\n",
                    currentIMU.accelX, currentIMU.accelY, currentIMU.accelZ,
                    currentIMU.gyroX, currentIMU.gyroY, currentIMU.gyroZ, currentIMU.temp);
                Serial.printf("クォータニオン: [%.3f,%.3f,%.3f,%.3f]\n",
                    currentRotation.w, currentRotation.x, currentRotation.y, currentRotation.z);
            }
        } else {
            // シミュレーション回転アニメーション
            rotation += 0.05f;
            if (rotation > 2.0f * M_PI) rotation = 0.0f;
            
            float qw = cos(rotation / 2.0f);
            float qx = 0.0f;
            float qy = sin(rotation / 2.0f);
            float qz = 0.0f;
            
            Quaternion simRotation(qw, qx, qy, qz);
            updateLEDsWithIMU(simRotation);
            
            if (updateCount % 40 == 0) {  // 2秒ごとにシミュレーション情報出力
                Serial.printf("シミュレーション回転: %.1f度\n", rotation * 180.0f / M_PI);
            }
        }
        
        FastLED.show();
        lastUpdate = millis();
    }
    
    // M5ボタン処理
    M5.update();
    if (M5.BtnA.wasPressed()) {
        Serial.println("ボタン押下 - パノラマデモ再実行");
        demonstrateRGBBufferDemo();
    }
    
    delay(10);
}

// =============================================================================
// IMU関連関数実装
// =============================================================================

bool initializeIMU() {
    // M5Unified経由でIMU初期化
    if (!M5.Imu.begin()) {
        Serial.println("エラー: IMU初期化失敗");
        return false;
    }
    
    // IMUセンサー情報取得
    Serial.printf("IMUセンサー検出: %s\n", M5.Imu.getType());
    
    // 初期キャリブレーション
    Serial.println("IMUキャリブレーション中...");
    delay(1000);  // 安定化待機
    
    // 初期値読み取りテスト
    IMUData testData;
    if (readIMUData(testData)) {
        Serial.printf("初期IMU値: 加速度[%.2f,%.2f,%.2f] ジャイロ[%.2f,%.2f,%.2f]\n",
            testData.accelX, testData.accelY, testData.accelZ,
            testData.gyroX, testData.gyroY, testData.gyroZ);
        return true;
    }
    
    return false;
}

bool readIMUData(IMUData& data) {
    auto imu_data = M5.Imu.getImuData();
    
    // M5UnifiedのIMUデータ形式から変換
    data.accelX = imu_data.accel.x;
    data.accelY = imu_data.accel.y;
    data.accelZ = imu_data.accel.z;
    
    data.gyroX = imu_data.gyro.x * M_PI / 180.0f;  // 度→ラジアン変換
    data.gyroY = imu_data.gyro.y * M_PI / 180.0f;
    data.gyroZ = imu_data.gyro.z * M_PI / 180.0f;
    
    data.temp = 25.0f;  // M5UnifiedのIMUには温度がないため固定値
    data.timestamp = millis();
    
    return true;  // M5Unifiedは常に有効なデータを返す
}

Quaternion integrateGyroscope(const IMUData& imu, const Quaternion& prev, float deltaTime) {
    // 角速度からクォータニオン微分を計算
    float wx = imu.gyroX * deltaTime * 0.5f;
    float wy = imu.gyroY * deltaTime * 0.5f;
    float wz = imu.gyroZ * deltaTime * 0.5f;
    
    // クォータニオン微分
    Quaternion dq(
        -prev.x * wx - prev.y * wy - prev.z * wz,
         prev.w * wx + prev.y * wz - prev.z * wy,
         prev.w * wy - prev.x * wz + prev.z * wx,
         prev.w * wz + prev.x * wy - prev.y * wx
    );
    
    // 新しいクォータニオン
    Quaternion result(
        prev.w + dq.w,
        prev.x + dq.x,
        prev.y + dq.y,
        prev.z + dq.z
    );
    
    // 正規化
    float norm = fast_sqrt(result.w*result.w + result.x*result.x + result.y*result.y + result.z*result.z);
    if (norm > 0.0001f) {
        result.w /= norm;
        result.x /= norm;
        result.y /= norm;
        result.z /= norm;
    }
    
    return result;
}

void updateLEDsWithIMU(const Quaternion& rotation) {
    // 高速LED更新（4個おきサンプリング）
    for (int i = 0; i < TOTAL_LEDS; i += 4) {
        // プロシージャル球面座標生成
        float t = (float)i / TOTAL_LEDS;
        float theta = t * 2.0f * M_PI;        // 経度 [0, 2π]
        float phi = acos(1.0f - 2.0f * t);    // 緯度 [0, π]
        
        // 球面座標→直交座標
        float x = sin(phi) * cos(theta);
        float y = cos(phi);
        float z = sin(phi) * sin(theta);
        
        // CUBE-neonクォータニオン回転適用
        float qw = rotation.w, qx = rotation.x, qy = rotation.y, qz = rotation.z;
        
        // クォータニオン回転行列展開
        float qw2 = qw * qw, qx2 = qx * qx, qy2 = qy * qy, qz2 = qz * qz;
        float rotX = (qw2 + qx2 - qy2 - qz2) * x + 2.0f * (qx*qy - qw*qz) * y + 2.0f * (qx*qz + qw*qy) * z;
        float rotY = 2.0f * (qx*qy + qw*qz) * x + (qw2 - qx2 + qy2 - qz2) * y + 2.0f * (qy*qz - qw*qx) * z;
        float rotZ = 2.0f * (qx*qz - qw*qy) * x + 2.0f * (qy*qz + qw*qx) * y + (qw2 - qx2 - qy2 + qz2) * z;
        
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
        uint8_t val = (uint8_t)(norm_u * 180 + 75);   // 緯度で輝度（高速化のため少し暗め）
        
        // FastLEDに設定
        leds[i].setHSV(hue, sat, val);
        
        // 近傍LEDも同色で塗る（補間）
        for (int j = 1; j < 4 && (i + j) < TOTAL_LEDS; j++) {
            leds[i + j] = leds[i];
        }
    }
}

// =============================================================================
// RGBバッファ処理関数実装
// =============================================================================

void generateRGBBuffer(RGBPixel* buffer) {
    // 全面黒の背景を作成
    for (int i = 0; i < RGB_BUFFER_WIDTH * RGB_BUFFER_HEIGHT; i++) {
        buffer[i] = RGBPixel(0, 0, 0);  // 黒
    }
    
    // 中央の高さに赤いラインを描画
    int lineY = RED_LINE_HEIGHT;  // 80px（中央）
    for (int x = 0; x < RGB_BUFFER_WIDTH; x++) {
        int index = lineY * RGB_BUFFER_WIDTH + x;
        if (index >= 0 && index < RGB_BUFFER_WIDTH * RGB_BUFFER_HEIGHT) {
            buffer[index] = RGBPixel(0, 0, 255);  // 赤ライン
        }
    }
    
    Serial.printf("RGBバッファ生成完了: %dx%d, 赤ライン高さ=%dpx\n", 
                  RGB_BUFFER_WIDTH, RGB_BUFFER_HEIGHT, RED_LINE_HEIGHT);
}

RGBPixel sampleRGBBuffer(const RGBPixel* buffer, float u, float v) {
    // UV座標をバッファ座標に変換
    // u: 0.0 to 1.0 → 0 to RGB_BUFFER_WIDTH
    // v: 0.0 to 1.0 → 0 to RGB_BUFFER_HEIGHT
    
    float x_f = u * RGB_BUFFER_WIDTH;
    float y_f = v * RGB_BUFFER_HEIGHT;
    
    // 座標をクランプ
    int x = (int)constrain(x_f, 0.0f, RGB_BUFFER_WIDTH - 1.0f);
    int y = (int)constrain(y_f, 0.0f, RGB_BUFFER_HEIGHT - 1.0f);
    
    int index = y * RGB_BUFFER_WIDTH + x;
    
    if (index >= 0 && index < RGB_BUFFER_WIDTH * RGB_BUFFER_HEIGHT) {
        return buffer[index];
    }
    
    return RGBPixel(0, 0, 0);  // 範囲外は黒
}

void updateLEDsWithRGBBuffer(const RGBPixel* rgbBuffer, const Quaternion& rotation) {
    // 高速LED更新（xyz-uv変換に特化）
    for (int i = 0; i < TOTAL_LEDS; i += 4) {
        // 改良された球面座標生成（より均等な分布）
        float t = (float)i / TOTAL_LEDS;
        
        // フィボナッチ球面分布（均等分布に近似）
        float y = 1.0f - 2.0f * t;  // y座標: [-1, 1]
        float radius = fast_sqrt(1.0f - y * y);  // x-z平面での半径
        float theta = 2.0f * M_PI * t * 2.39996323f;  // ゴールデン角
        
        // 球面座標→直交座標 (xyz)
        float x = radius * cos(theta);
        float z = radius * sin(theta);
        // y は既に計算済み
        
        // CUBE-neonクォータニオン回転適用
        float qw = rotation.w, qx = rotation.x, qy = rotation.y, qz = rotation.z;
        
        // クォータニオン回転行列展開
        float qw2 = qw * qw, qx2 = qx * qx, qy2 = qy * qy, qz2 = qz * qz;
        float rotX = (qw2 + qx2 - qy2 - qz2) * x + 2.0f * (qx*qy - qw*qz) * y + 2.0f * (qx*qz + qw*qy) * z;
        float rotY = 2.0f * (qx*qy + qw*qz) * x + (qw2 - qx2 + qy2 - qz2) * y + 2.0f * (qy*qz - qw*qx) * z;
        float rotZ = 2.0f * (qx*qz - qw*qy) * x + 2.0f * (qy*qz + qw*qx) * y + (qw2 - qx2 - qy2 + qz2) * z;
        
        // 正しい球面→UV座標変換（極座標変換）
        float longitude = fast_atan2(rotZ, rotX);  // [-π, π]
        float latitude = fast_atan2(rotY, fast_sqrt(rotX*rotX + rotZ*rotZ));  // [-π/2, π/2]
        
        // UV正規化: 320x160バッファに対応
        float u = (longitude + M_PI) / (2.0f * M_PI);  // [0, 1] → 320pixel
        float v = (latitude + M_PI/2.0f) / M_PI;       // [0, 1] → 160pixel
        
        // UV座標をクランプ（安全性確保）
        u = constrain(u, 0.0f, 1.0f);
        v = constrain(v, 0.0f, 1.0f);
        
        // RGBバッファから色をサンプリング
        RGBPixel pixel = sampleRGBBuffer(rgbBuffer, u, v);
        
        // 赤チャンネルのみを使用してLEDの輝度を決定
        uint8_t redValue = pixel.r;
        
        if (redValue > 10) {  // 赤の閾値（ノイズ除去）
            // 赤い部分は白色で表示（強調表示）
            leds[i] = CRGB(redValue, redValue, redValue);
        } else {
            // 黒い部分は暗い青で表示（背景表示）
            leds[i] = CRGB(0, 0, 20);
        }
        
        // 近傍LEDも同色で塗る（補間）
        for (int j = 1; j < 4 && (i + j) < TOTAL_LEDS; j++) {
            leds[i + j] = leds[i];
        }
    }
}

void demonstrateRGBBufferDemo() {
    Serial.println("RGBバッファデモ開始...");
    
    // PSRAM allocatorでRGBバッファ用メモリ確保（AGENTS.md準拠）
    size_t bufferSize = RGB_BUFFER_WIDTH * RGB_BUFFER_HEIGHT * sizeof(RGBPixel);
    RGBPixel* rgbBuffer = (RGBPixel*)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM);
    
    if (!rgbBuffer) {
        Serial.printf("エラー: PSRAM確保失敗（%zu bytes）\n", bufferSize);
        Serial.println("フォールバック: 通常RAMで試行");
        rgbBuffer = (RGBPixel*)malloc(bufferSize);
        if (!rgbBuffer) {
            Serial.println("エラー: RGBバッファメモリ確保完全失敗");
            return;
        }
    } else {
        Serial.printf("成功: PSRAM確保（%zu bytes）\n", bufferSize);
    }
    
    // RGBバッファ生成（黒背景 + 赤ライン）
    generateRGBBuffer(rgbBuffer);
    
    // 3つの異なる角度でデモ表示
    for (int demo = 0; demo < 3; demo++) {
        Serial.printf("\nRGBバッファデモ %d/3: ", demo + 1);
        
        // 異なる回転角度を設定
        float angle = demo * M_PI / 3.0f;  // 0度, 60度, 120度
        Quaternion demoRotation(cos(angle/2.0f), 0.0f, sin(angle/2.0f), 0.0f);  // Y軸回転
        
        Serial.printf("Y軸%.0f度回転\n", angle * 180.0f / M_PI);
        
        // RGBバッファを使ったLED更新
        updateLEDsWithRGBBuffer(rgbBuffer, demoRotation);
        FastLED.show();
        
        delay(3000);  // 3秒表示
    }
    
    // PSRAM/通常RAMメモリ解放
    heap_caps_free(rgbBuffer);
    
    Serial.println("RGBバッファデモ完了");
}