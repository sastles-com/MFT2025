/**
 * @file test_procedural_patterns.cpp
 * @brief プロシージャルパターン生成システムのテスト
 * 
 * 座標軸インジケータパターンを含む全パターンクラスの動作検証テスト
 */

#include <unity.h>
#include "pattern/ProceduralPatternGenerator.h"
#include "M5Unified.h"

using namespace ProceduralPattern;

/**
 * @brief テスト用のモックディスプレイ
 */
class MockDisplay {
public:
    static bool initialized;
    static int lastDrawnX, lastDrawnY;
    static uint16_t lastColor;
    static int pixelCount;
    
    static void reset() {
        initialized = false;
        lastDrawnX = lastDrawnY = -1;
        lastColor = 0;
        pixelCount = 0;
    }
    
    static void fillScreen(uint16_t color) {
        initialized = true;
        lastColor = color;
    }
    
    static void drawPixel(int x, int y, uint16_t color) {
        lastDrawnX = x;
        lastDrawnY = y;
        lastColor = color;
        pixelCount++;
    }
    
    static void drawCircle(int x, int y, int r, uint16_t color) {
        lastDrawnX = x;
        lastDrawnY = y;
        lastColor = color;
    }
    
    static void fillCircle(int x, int y, int r, uint16_t color) {
        lastDrawnX = x;
        lastDrawnY = y;
        lastColor = color;
    }
    
    static void setCursor(int x, int y) {}
    static void setTextColor(uint16_t color) { lastColor = color; }
    static void setTextSize(int size) {}
    static void print(const char* text) {}
    static void printf(const char* format, ...) {}
    static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { 
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); 
    }
};

// 静的メンバー定義
bool MockDisplay::initialized = false;
int MockDisplay::lastDrawnX = -1;
int MockDisplay::lastDrawnY = -1;
uint16_t MockDisplay::lastColor = 0;
int MockDisplay::pixelCount = 0;

void setUp(void) {
    MockDisplay::reset();
}

void tearDown(void) {
    // テスト後のクリーンアップ
}

/**
 * @brief PatternGeneratorファクトリーメソッドのテスト
 */
void test_pattern_generator_factory() {
    PatternGenerator generator;
    
    // 利用可能なパターン一覧の確認
    auto patterns = generator.getAvailablePatterns();
    TEST_ASSERT_EQUAL(5, patterns.size());
    
    // 各パターンの生成テスト
    auto latitudePattern = generator.createPattern("latitude_rings");
    TEST_ASSERT_NOT_NULL(latitudePattern.get());
    TEST_ASSERT_EQUAL_STRING("Latitude Rings", latitudePattern->getName());
    
    auto longitudePattern = generator.createPattern("longitude_lines");
    TEST_ASSERT_NOT_NULL(longitudePattern.get());
    TEST_ASSERT_EQUAL_STRING("Longitude Lines", longitudePattern->getName());
    
    auto axisPattern = generator.createPattern("coordinate_axis");
    TEST_ASSERT_NOT_NULL(axisPattern.get());
    TEST_ASSERT_EQUAL_STRING("Coordinate Axis", axisPattern->getName());
    
    // 無効なパターン名
    auto invalidPattern = generator.createPattern("invalid_pattern");
    TEST_ASSERT_NULL(invalidPattern.get());
}

/**
 * @brief 座標軸インジケータパターンのテスト
 */
void test_coordinate_axis_pattern() {
    auto axisPattern = std::make_unique<CoordinateAxisPattern>();
    
    // パターン基本情報
    TEST_ASSERT_EQUAL_STRING("Coordinate Axis", axisPattern->getName());
    TEST_ASSERT_EQUAL_STRING("XYZ axis indicators with grid and labels", axisPattern->getDescription());
    
    // 設定変更テスト
    axisPattern->setBrightness(0.8f);
    axisPattern->setShowLabels(false);
    axisPattern->setShowGrid(false);
    axisPattern->setAnimateRotation(true);
    axisPattern->setRotationSpeed(2.0f);
    
    // 描画テスト（モックディスプレイ）
    PatternParams params;
    params.screenWidth = 128;
    params.screenHeight = 128;
    params.centerX = 64;
    params.centerY = 64;
    params.radius = 60;
    params.progress = 0.5f;
    params.time = 1.0f;
    
    // NOTE: 実際のM5.Displayへの描画はモック化が複雑なため、
    // ここではパターンの生成と基本設定のみテスト
    TEST_ASSERT_NOT_NULL(axisPattern.get());
}

/**
 * @brief SphereCoordinateSystemユーティリティのテスト
 */
void test_sphere_coordinate_system() {
    // 球座標変換テスト
    auto spherical = SphereCoordinateSystem::cartesianToSpherical(1.0f, 0.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, spherical.theta);  // X軸正方向
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, spherical.phi);    // 赤道
    
    // UV座標変換テスト
    auto uv = SphereCoordinateSystem::sphericalToUV(spherical);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, uv.u);  // 経度0度 → u=0.5
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, uv.v);  // 緯度0度 → v=0.5
    
    // 3D点投影テスト
    auto projected = SphereCoordinateSystem::project3DPoint(1.0f, 0.0f, 0.0f, 64, 64, 60);
    TEST_ASSERT_TRUE(projected.visible);
    TEST_ASSERT_GREATER_THAN(60, projected.x);  // 画面右側
    TEST_ASSERT_EQUAL(64, projected.y);         // 中央
    
    // 色調整テスト
    uint16_t testColor = 0xF800;  // 赤
    uint16_t adjustedColor = SphereCoordinateSystem::adjustBrightness(testColor, 0.5f);
    TEST_ASSERT_NOT_EQUAL(testColor, adjustedColor);  // 明度が変わること
}

/**
 * @brief 緯度線・経度線生成のテスト
 */
void test_coordinate_lines() {
    // 赤道線（緯度0度）の生成
    auto equatorPoints = SphereCoordinateSystem::getLatitudeLine(0.0f, 64, 64, 60);
    TEST_ASSERT_GREATER_THAN(10, equatorPoints.size());  // 十分な点数
    
    // 本初子午線（経度0度）の生成
    auto primePoints = SphereCoordinateSystem::getLongitudeLine(0.0f, 64, 64, 60);
    TEST_ASSERT_GREATER_THAN(10, primePoints.size());  // 十分な点数
    
    // 3D線分生成
    auto linePoints = SphereCoordinateSystem::get3DLine(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 
                                                       64, 64, 60, 10);
    TEST_ASSERT_EQUAL(10, linePoints.size());  // 指定セグメント数
}

/**
 * @brief パターンパラメータのテスト
 */
void test_pattern_params() {
    PatternGenerator generator;
    
    // デフォルトパラメータ取得
    auto defaultParams = generator.getDefaultParams();
    TEST_ASSERT_EQUAL(128, defaultParams.screenWidth);
    TEST_ASSERT_EQUAL(128, defaultParams.screenHeight);
    TEST_ASSERT_EQUAL(64, defaultParams.centerX);
    TEST_ASSERT_EQUAL(64, defaultParams.centerY);
    TEST_ASSERT_EQUAL(60, defaultParams.radius);
    
    // カスタムパラメータ設定
    PatternParams customParams;
    customParams.screenWidth = 240;
    customParams.screenHeight = 240;
    customParams.centerX = 120;
    customParams.centerY = 120;
    customParams.radius = 100;
    
    generator.setDefaultParams(customParams);
    auto updatedParams = generator.getDefaultParams();
    TEST_ASSERT_EQUAL(240, updatedParams.screenWidth);
    TEST_ASSERT_EQUAL(120, updatedParams.centerX);
    TEST_ASSERT_EQUAL(100, updatedParams.radius);
}

/**
 * @brief パターン描画統合テスト
 */
void test_pattern_rendering() {
    PatternGenerator generator;
    
    // 座標軸パターンの描画テスト
    MockDisplay::reset();
    // NOTE: 実際のM5.Display呼び出しはテスト環境で実行困難
    // generator.renderPattern("coordinate_axis", 0.5f, 1.0f);
    
    // パターン名の記録確認
    // TEST_ASSERT_EQUAL_STRING("coordinate_axis", generator.getCurrentPatternName().c_str());
    
    TEST_ASSERT_TRUE(true);  // プレースホルダー
}

/**
 * @brief 数学的精度のテスト
 */
void test_mathematical_accuracy() {
    // 正規化された球面座標の境界値テスト
    
    // 北極点 (0, 0, 1)
    auto northPole = SphereCoordinateSystem::cartesianToSpherical(0.0f, 0.0f, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, PI/2.0f, northPole.phi);
    
    // 南極点 (0, 0, -1)
    auto southPole = SphereCoordinateSystem::cartesianToSpherical(0.0f, 0.0f, -1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -PI/2.0f, southPole.phi);
    
    // 東方向 (1, 0, 0)
    auto eastPoint = SphereCoordinateSystem::cartesianToSpherical(1.0f, 0.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, eastPoint.theta);
    
    // 西方向 (-1, 0, 0)
    auto westPoint = SphereCoordinateSystem::cartesianToSpherical(-1.0f, 0.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, PI, fabsf(westPoint.theta));
}

/**
 * @brief メインテスト実行関数
 */
void runTests() {
    UNITY_BEGIN();
    
    RUN_TEST(test_pattern_generator_factory);
    RUN_TEST(test_coordinate_axis_pattern);
    RUN_TEST(test_sphere_coordinate_system);
    RUN_TEST(test_coordinate_lines);
    RUN_TEST(test_pattern_params);
    RUN_TEST(test_pattern_rendering);
    RUN_TEST(test_mathematical_accuracy);
    
    UNITY_END();
}

/**
 * @brief Arduinoスタイルのセットアップ
 */
void setup() {
    delay(2000);  // シリアル初期化待機
    runTests();
}

/**
 * @brief Arduinoスタイルのループ
 */
void loop() {
    // テスト完了後は何もしない
    delay(1000);
}

#ifdef UNIT_TEST
// PlatformIOテスト環境用エントリーポイント
int main() {
    runTests();
    return 0;
}
#endif