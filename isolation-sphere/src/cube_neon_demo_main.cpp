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
    
    // 簡単なLED動作テスト
    Serial.println("\n[2] LED動作テスト");
    for (int i = 0; i < TOTAL_LEDS; i += 10) {
        leds[i] = CRGB::Red;
    }
    FastLED.show();
    delay(1000);
    
    FastLED.clear();
    FastLED.show();
    
    Serial.println("初期化完了 - メインループ開始");
}

// パノラマバッファのグローバル変数
uint8_t* panoramaBuffer = nullptr;
const int PANORAMA_WIDTH = 320;
const int PANORAMA_HEIGHT = 160;

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

// 実際のLED座標データ（led_layout.csvから抜粋）
struct LEDCoord {
    int faceID;
    float x, y, z;
};

// X=0付近のLED座標データ
LEDCoord xNearZeroCoords[] = {
    {7, 0.030549123, 0.094021168, -0.995101387},
    {79, 0.030549126, -0.094021175, -0.995101387},
    {81, -0.02274537, -0.291095547, -0.956423562},
    {82, 0.01352336, -0.413422178, -0.910439027},
    {87, 0.026891631, -0.801619553, -0.59722938},
    {88, -0.009428102, -0.718389689, -0.695577002},
    {89, -0.047752107, -0.616040131, -0.786266045},
    {164, -0.022745379, 0.291095547, -0.956423562},
    {165, 0.013523353, 0.413422149, -0.91043904},
    {172, -0.047752128, 0.616040106, -0.786266063},
    {175, -0.009428109, 0.718389651, -0.695577041},
    {177, 0.026891632, 0.801619526, -0.597229416},
    {186, -0.009363985, -0.913207209, -0.40738791},
    {187, 0.048756564, -0.95543504, -0.291147181},
    {189, 0.036591272, -0.997574814, -0.059207847},
    {190, -0.023806654, -0.983374153, -0.180023663},
    {199, -0.036591265, -0.997574814, 0.059207847},
    {381, -0.009364001, 0.913207209, -0.40738791},
    {396, -0.023806662, 0.983374157, -0.180023636},
    {397, 0.048756545, 0.955435041, -0.291147181},
    {407, -0.030549123, -0.094021168, 0.995101387},
    {479, -0.030549126, 0.094021175, 0.995101387},
    {481, 0.02274537, 0.291095547, 0.956423562},
    {482, -0.01352336, 0.413422178, 0.910439027},
    {487, -0.026891631, 0.801619553, 0.59722938},
    {488, 0.009428102, 0.718389689, 0.695577002},
    {489, 0.047752107, 0.616040131, 0.786266045},
    {564, 0.022745379, -0.291095547, 0.956423562},
    {565, -0.013523353, -0.413422149, 0.91043904},
    {572, 0.047752128, -0.616040106, 0.786266063},
    {575, 0.009428109, -0.718389651, 0.695577041},
    {577, -0.026891632, -0.801619526, 0.597229416},
    {586, 0.009363985, -0.913207209, 0.40738791},
    {587, -0.048756564, -0.95543504, 0.291147181},
    {589, -0.036591272, -0.997574814, 0.059207847},
    {590, 0.023806654, -0.983374153, 0.180023663},
    {599, 0.036591265, -0.997574814, -0.059207847},
    {781, 0.009364001, 0.913207209, 0.40738791},
    {796, 0.023806662, 0.983374157, 0.180023636},
    {797, -0.048756545, 0.955435041, 0.291147181}
};

bool coordsInitialized = false;

// 球面座標→UV座標変換（修正版：パノラマの対面側も表現）
void sphericalToUV(float x, float y, float z, float& u, float& v) {
    // 球面座標（経度・緯度）計算
    float longitude = atan2(z, x);  // -π to π
    float latitude = atan2(y, sqrt(x*x + z*z));  // -π/2 to π/2
    
    // UV正規化 [0, 1]
    u = (longitude + M_PI) / (2.0f * M_PI);  // 0 to 1
    v = (latitude + M_PI/2.0f) / M_PI;       // 0 to 1
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

// 緑リングのFaceID座標をパノラマ画像に描画
void drawFaceIDCoordinatesToPanorama() {
    if (!panoramaBuffer) return;
    
    // パノラマ画像を黒で初期化
    memset(panoramaBuffer, 0, PANORAMA_WIDTH * PANORAMA_HEIGHT * 3);
    
    Serial.println("=== FaceID座標をパノラマ画像に描画 ===");
    
    // 各FaceID座標をUV変換してパノラマ画像に描画
    for (int i = 0; i < xNearZeroCount; i++) {
        float x = xNearZeroCoords[i].x;
        float y = xNearZeroCoords[i].y;
        float z = xNearZeroCoords[i].z;
        
        // 座標正規化（半径1に正規化）
        float length = sqrt(x*x + y*y + z*z);
        if (length > 0) {
            x /= length;
            y /= length;
            z /= length;
        }
        
        // 球面座標→UV変換
        float u, v;
        sphericalToUV(x, y, z, u, v);
        
        // 第1の位置（元の位置）に描画
        drawGreenPixelAt(u, v, PANORAMA_WIDTH, PANORAMA_HEIGHT);
        
        // 第2の位置（対面側 +180度）に描画
        float u_opposite = u + 0.5f;
        if (u_opposite > 1.0f) u_opposite -= 1.0f;  // ラップアラウンド
        drawGreenPixelAt(u_opposite, v, PANORAMA_WIDTH, PANORAMA_HEIGHT);
        
        // さらに詳細分析：X=0の理論位置も描画
        if (abs(x) < 0.1f) {  // X=0付近
            // 理論的なX=0位置 (経度0度と180度)
            float u_theory_0 = 0.5f;   // 0度 → U=0.5 → ピクセル160
            float u_theory_180 = 0.0f; // 180度 → U=0.0 → ピクセル0（またはU=1.0 → ピクセル320）
            
            drawBluePixelAt(u_theory_0, v, PANORAMA_WIDTH, PANORAMA_HEIGHT);    // 青色で理論位置
            drawBluePixelAt(u_theory_180, v, PANORAMA_WIDTH, PANORAMA_HEIGHT);  // 青色で理論位置
        }
        
        // 最初の10個の詳細ログ
        if (i < 10) {
            Serial.printf("FaceID[%d]: XYZ(%.3f,%.3f,%.3f) → UV(%.3f,%.3f) → Pixel(%d,%d)\n",
                         xNearZeroCoords[i].faceID, x, y, z, u, v, (int)(u * (PANORAMA_WIDTH-1)), (int)(v * (PANORAMA_HEIGHT-1)));
            Serial.printf("         対面側: UV(%.3f,%.3f) → Pixel(%d,%d)\n", 
                         u_opposite, v, (int)(u_opposite * (PANORAMA_WIDTH-1)), (int)(v * (PANORAMA_HEIGHT-1)));
        }
    }
    
    // 赤い縦線（X軸大円の理論位置）- アニメーション版
    // 極軸（Z軸）周りに回転する大円のアニメーション
    // X軸大円基準位置: U=0.25, U=0.75 (経度90度、270度) 
    // アニメーション: U座標を連続的に変化させる
    
    // アニメーション位相を更新（0-1の範囲で循環）
    animationPhase += ANIMATION_SPEED;
    if (animationPhase >= 1.0f) {
        animationPhase = 0.0f;
    }
    
    // 回転による U座標の計算
    // animationPhase=0: 元のX軸大円位置（U=0.25, U=0.75）
    // animationPhase=0.5: Y軸大円位置（U=0.0, U=0.5）
    float u1 = fmod(0.25f + animationPhase, 1.0f);  // 第1の縦線
    float u2 = fmod(0.75f + animationPhase, 1.0f);  // 第2の縦線（180度対面）
    
    int red_animated_pixels[] = {(int)(u1 * PANORAMA_WIDTH), (int)(u2 * PANORAMA_WIDTH)};
    
    for (int i = 0; i < 2; i++) {
        int px = red_animated_pixels[i];
        for (int py = 0; py < PANORAMA_HEIGHT; py++) {
            for (int dx = -2; dx <= 2; dx++) {  // 5ピクセル幅（視認性向上）
                int test_px = px + dx;
                if (test_px >= 0 && test_px < PANORAMA_WIDTH) {
                    int pixelIndex = (py * PANORAMA_WIDTH + test_px) * 3;
                    
                    // 既に緑色が描画されていない場合のみ赤色を描画
                    if (panoramaBuffer[pixelIndex + 1] < 200) {  // 緑が薄い場合のみ
                        float intensity = 1.0f - (abs(dx) / 2.0f);  // 中央が最も明るい
                        panoramaBuffer[pixelIndex + 0] = (uint8_t)(255 * intensity);  // R（赤色）
                        panoramaBuffer[pixelIndex + 1] = 0;    // G
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
        Serial.printf("🔄 Animation: Phase=%.3f, U1=%.3f (px=%d), U2=%.3f (px=%d)\n", 
                     animationPhase, u1, (int)(u1 * PANORAMA_WIDTH), 
                     u2, (int)(u2 * PANORAMA_WIDTH));
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
    Serial.println("- 緑色: FaceID座標（X軸大円の固定基準位置）");
    Serial.println("- 赤色: 極軸周りに回転するアニメーション大円");
    Serial.println("- 青色: X軸大円の理論位置（確認用）");
    Serial.println("- シアン: Y軸大円の理論位置（U=0.0, U=0.5）");
    Serial.println("アニメーション効果:");
    Serial.printf("- 赤線が時計の針のように極軸（Z軸）周りに回転\n");
    Serial.printf("- 緑線通過時に完全一致（X軸大円位置）\n");
    Serial.printf("- 1回転: %.1f秒（phase=0→1）\n", 1.0f / ANIMATION_SPEED / 60.0f);
}

// パノラマ画像をPPM形式でLittleFSに保存
bool savePanoramaImageAsPPM(const char* filename) {
    if (!panoramaBuffer) {
        Serial.println("パノラマバッファが存在しません");
        return false;
    }
    
    // LittleFS初期化
    if (!LittleFS.begin()) {
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
    
    // 実際のパノラマバッファから色を取得
    if (panoramaBuffer) {
        int pixelIndex = (py * PANORAMA_WIDTH + px) * 3;
        uint8_t r = panoramaBuffer[pixelIndex + 0];
        uint8_t g = panoramaBuffer[pixelIndex + 1];
        uint8_t b = panoramaBuffer[pixelIndex + 2];
        return CRGB(r, g, b);
    }
    
    // バッファが無い場合は従来の仮想パノラマ画像処理
    uint8_t r = 0, g = 0, b = 0;
    
    // 水平ライン（X軸大円に対応）をシミュレーション
    float centerV = 0.5f;  // 画像中央（緯度0度）
    float distFromCenter = abs(v - centerV);
    
    if (distFromCenter < 0.2f) {  // 中央付近20%の幅（太くした）
        // 赤色グラデーション
        float intensity = (0.2f - distFromCenter) / 0.2f;  // 0-1
        r = (uint8_t)(255 * intensity);
        g = (uint8_t)(64 * intensity);   // 少し橙色を混ぜる
        b = 0;
    } else {
        // 背景は暗い青
        r = 0;
        g = 0;
        b = 20;
    }
    
    return CRGB(r, g, b);
}

void initializePanorama() {
    // PSRAMにパノラマバッファを確保
    panoramaBuffer = (uint8_t*)heap_caps_malloc(PANORAMA_WIDTH * PANORAMA_HEIGHT * 3, MALLOC_CAP_SPIRAM);
    
    if (panoramaBuffer) {
        Serial.println("PSRAMにパノラマバッファ確保成功");
        Serial.printf("サイズ: %dx%d = %d bytes\n", PANORAMA_WIDTH, PANORAMA_HEIGHT, PANORAMA_WIDTH * PANORAMA_HEIGHT * 3);
    } else {
        Serial.println("PSRAMバッファ確保失敗 - 通常RAMを試行");
        panoramaBuffer = (uint8_t*)malloc(PANORAMA_WIDTH * PANORAMA_HEIGHT * 3);
    }
    
    // パノラマ画像に緑リングのFaceID座標をプロット
    drawFaceIDCoordinatesToPanorama();
    
    // PPM形式で保存
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
        
        // パノラマバッファをクリア
        if (panoramaBuffer) {
            memset(panoramaBuffer, 0, PANORAMA_WIDTH * PANORAMA_HEIGHT * 3);
        }
        
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
    
    // 1. まず緑線（X=0大円の基準）を元の方法で描画
    for (int j = 0; j < xNearZeroCount; j++) {
        int faceID = xNearZeroCoords[j].faceID;
        
        if (faceID < TOTAL_LEDS) {
            leds[faceID] = CRGB(0, 255, 0);  // 緑色
            faceIDCount++;
        }
    }
    
    // 2. 赤線アニメーション（全800個のLEDに対して適用）
    // アニメーション位置の計算
    float u1 = fmod(0.25f + animationPhase, 1.0f);
    float u2 = fmod(0.75f + animationPhase, 1.0f);
    
    // 全LEDに対してアニメーション大円の判定を行う（改良された座標変換）
    for (int ledIndex = 0; ledIndex < TOTAL_LEDS; ledIndex++) {
        // LEDインデックスから球面座標を計算（改良版）
        // フィボナッチ螺旋による均等分布またはシンプルな緯度経度グリッド
        
        // 方法1: シンプルな緯度経度グリッド
        int strips = 4;  // 4つのストリップ
        int ledsPerStrip = TOTAL_LEDS / strips;  // 200個/ストリップ
        
        int stripIndex = ledIndex / ledsPerStrip;  // 0-3
        int indexInStrip = ledIndex % ledsPerStrip;  // 0-199
        
        // 各ストリップを経度方向に配置
        float theta = (float)indexInStrip / ledsPerStrip * 2.0f * M_PI;  // 経度 0-2π
        float phi = M_PI * (stripIndex + 0.5f) / strips;  // 緯度を4分割（上から下へ）
        
        // 球面座標→直交座標
        float x = sin(phi) * cos(theta);
        float y = sin(phi) * sin(theta);
        float z = cos(phi);
        
        // 球面座標→UV変換
        float u, v;
        sphericalToUV(x, y, z, u, v);
        
        // 縦線に近いLEDを赤色で描画
        float u_diff1 = abs(u - u1);
        float u_diff2 = abs(u - u2);
        if (u_diff1 > 0.5f) u_diff1 = 1.0f - u_diff1;  // ラップアラウンド考慮
        if (u_diff2 > 0.5f) u_diff2 = 1.0f - u_diff2;  // ラップアラウンド考慮
        
        float line_width = 0.02f;  // U座標での線幅
        if (u_diff1 < line_width || u_diff2 < line_width) {
            // 既に緑色の場合は黄色に（重複表示）
            if (leds[ledIndex].g > 200) {
                leds[ledIndex] = CRGB(255, 255, 0);  // 黄色（重複）
            } else {
                leds[ledIndex] = CRGB(255, 0, 0);  // 赤色
            }
            panoramaCount++;
            sumU += u;
            sumV += v;
        }
    }
    
    // LED統計を定期的に出力（アニメーション確認用）
    static int frame_counter = 0;
    frame_counter++;
    if (frame_counter % 60 == 0) {  // 60フレームごと（約1秒間隔）に出力
        float avgU = (panoramaCount > 0) ? sumU / panoramaCount : 0;
        float avgV = (panoramaCount > 0) ? sumV / panoramaCount : 0;
        
        // 重複確認
        int redCount = 0;
        int greenCount = 0;
        int yellowCount = 0;
        for (int i = 0; i < TOTAL_LEDS; i++) {
            if (leds[i].r > 200 && leds[i].g > 200) {
                yellowCount++;  // 黄色（重複）
            } else if (leds[i].r > 200) {
                redCount++;     // 赤色
            } else if (leds[i].g > 200) {
                greenCount++;   // 緑色
            }
        }
        
        Serial.printf("📊 LED統計 - 赤:%d個, 緑:%d個, 黄:%d個 | Phase=%.3f\n", 
                     redCount, greenCount, yellowCount, animationPhase);
    }
    
    FastLED.show();
    delay(100);
}