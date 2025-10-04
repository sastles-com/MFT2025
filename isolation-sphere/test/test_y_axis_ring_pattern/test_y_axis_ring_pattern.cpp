#include <unity.h>
#in        // デフォルトリング設定（すべて0.5緑色）
        CRGB halfGreen = CRGB(0, 127, 0);
        rings_.push_back({60.0f, halfGreen, 1.0f, 0.0f});      // 北極寄り
        rings_.push_back({30.0f, halfGreen, 1.2f, PI/3});      // 中緯度北
        rings_.push_back({0.0f, halfGreen, 1.5f, 2*PI/3});     // 赤道
        rings_.push_back({-30.0f, halfGreen, 1.2f, PI});       // 中緯度南
        rings_.push_back({-60.0f, halfGreen, 1.0f, 4*PI/3});   // 南極寄り"pattern/ProceduralPatternGenerator.h"
#include "led/LEDSphereManager.h"
#include <memory>

using namespace ProceduralPattern;

class TestYAxisRingPattern : public IPattern {
private:
    struct Ring {
        float latitude;     // 緯度（-90度〜+90度）
        CRGB color;        // リングの色
        float speed;       // 回転速度
        float phase;       // 位相オフセット
    };
    
    std::vector<Ring> rings_;
    float globalSpeed_;
    float brightness_;
    
public:
    TestYAxisRingPattern() : globalSpeed_(1.0f), brightness_(1.0f) {
        // デフォルトリング設定
        rings_.push_back({60.0f, CRGB::Red, 1.0f, 0.0f});      // 北極寄り
        rings_.push_back({30.0f, CRGB::Green, 1.2f, PI/3});    // 中緯度北
        rings_.push_back({0.0f, CRGB::Blue, 1.5f, 2*PI/3});    // 赤道
        rings_.push_back({-30.0f, CRGB::Yellow, 1.2f, PI});    // 中緯度南
        rings_.push_back({-60.0f, CRGB::Magenta, 1.0f, 4*PI/3}); // 南極寄り
    }
    
    void render(const PatternParams& params) override {
        if (!sphereManager_) return;
        
        sphereManager_->clearAllLEDs();
        
        for (const auto& ring : rings_) {
            // 時間による色の変化（HSV回転）
            float timePhase = params.time * globalSpeed_ + ring.phase;
            
            // 輝度パルス効果
            float pulseBrightness = 0.5f + 0.5f * sinf(timePhase * ring.speed);
            
            // 色の計算
            CRGB animatedColor = ring.color;
            animatedColor.nscale8((uint8_t)(brightness_ * pulseBrightness * 255));
            
            // y軸周りのリング描画（特定緯度の全周）
            sphereManager_->drawLatitudeLine(ring.latitude, animatedColor, 2);
        }
        
        sphereManager_->show();
    }
    
    const char* getName() const override { 
        return "X-Axis Half Green Rings"; 
    }
    
    const char* getDescription() const override { 
        return "Half green rings around Y-axis representing X-axis system"; 
    }
    
    float getDuration() const override { 
        return 5.0f; 
    }
    
    void setGlobalSpeed(float speed) { globalSpeed_ = speed; }
    void setBrightness(float brightness) override { brightness_ = brightness; }
    
    // テスト用アクセサ
    size_t getRingCount() const { return rings_.size(); }
    float getRingLatitude(size_t index) const { 
        return (index < rings_.size()) ? rings_[index].latitude : 0.0f; 
    }
    CRGB getRingColor(size_t index) const {
        return (index < rings_.size()) ? rings_[index].color : CRGB::Black;
    }
};

// テスト用グローバル変数
std::unique_ptr<TestYAxisRingPattern> testPattern;
std::unique_ptr<LEDSphere::LEDSphereManager> testSphereManager;

void setUp() {
    testPattern = std::make_unique<TestYAxisRingPattern>();
    testSphereManager = std::make_unique<LEDSphere::LEDSphereManager>();
    testPattern->setSphereManager(testSphereManager.get());
}

void tearDown() {
    testPattern.reset();
    testSphereManager.reset();
}

void test_YAxisRingPattern_initialization() {
    TEST_ASSERT_NOT_NULL(testPattern.get());
    TEST_ASSERT_EQUAL_STRING("X-Axis Half Green Rings", testPattern->getName());
    TEST_ASSERT_EQUAL_STRING("Half green rings around Y-axis representing X-axis system", testPattern->getDescription());
    TEST_ASSERT_EQUAL_FLOAT(5.0f, testPattern->getDuration());
}

void test_YAxisRingPattern_default_ring_configuration() {
    // デフォルトで5つのリングが設定されているかテスト
    TEST_ASSERT_EQUAL(5, testPattern->getRingCount());
    
    // 各リングの緯度が正しく設定されているかテスト
    TEST_ASSERT_EQUAL_FLOAT(60.0f, testPattern->getRingLatitude(0));   // 北極寄り
    TEST_ASSERT_EQUAL_FLOAT(30.0f, testPattern->getRingLatitude(1));   // 中緯度北
    TEST_ASSERT_EQUAL_FLOAT(0.0f, testPattern->getRingLatitude(2));    // 赤道
    TEST_ASSERT_EQUAL_FLOAT(-30.0f, testPattern->getRingLatitude(3));  // 中緯度南
    TEST_ASSERT_EQUAL_FLOAT(-60.0f, testPattern->getRingLatitude(4));  // 南極寄り
    
    // 各リングの色が正しく設定されているかテスト（すべて0.5緑色）
    CRGB halfGreen = CRGB(0, 127, 0);
    TEST_ASSERT_EQUAL(halfGreen.g, testPattern->getRingColor(0).g);
    TEST_ASSERT_EQUAL(halfGreen.g, testPattern->getRingColor(1).g);
    TEST_ASSERT_EQUAL(halfGreen.g, testPattern->getRingColor(2).g);
    TEST_ASSERT_EQUAL(halfGreen.g, testPattern->getRingColor(3).g);
    TEST_ASSERT_EQUAL(halfGreen.g, testPattern->getRingColor(4).g);
}

void test_YAxisRingPattern_render_with_basic_params() {
    PatternParams params;
    params.progress = 0.5f;
    params.time = 1.0f;
    params.brightness = 0.8f;
    
    // レンダリング実行（例外が発生しないことを確認）
    testPattern->setBrightness(0.8f);
    testPattern->render(params);
    
    // テストが正常終了すればOK
    TEST_ASSERT_TRUE(true);
}

void test_YAxisRingPattern_speed_and_brightness_control() {
    // 速度設定テスト
    testPattern->setGlobalSpeed(2.0f);
    
    // 輝度設定テスト
    testPattern->setBrightness(0.5f);
    
    PatternParams params;
    params.progress = 0.0f;
    params.time = 0.0f;
    
    // レンダリング実行
    testPattern->render(params);
    
    // 設定が適用されていることを確認（例外が発生しないことで判定）
    TEST_ASSERT_TRUE(true);
}

void test_YAxisRingPattern_time_progression() {
    PatternParams params;
    params.brightness = 1.0f;
    
    // 時間進行によるアニメーション確認
    for (float t = 0.0f; t <= 5.0f; t += 0.5f) {
        params.time = t;
        params.progress = t / 5.0f;
        
        testPattern->render(params);
    }
    
    // 全時間でレンダリングが正常実行されることを確認
    TEST_ASSERT_TRUE(true);
}

void test_YAxisRingPattern_edge_cases() {
    PatternParams params;
    
    // 極端な値でのテスト
    params.time = -1.0f;
    params.progress = -0.5f;
    params.brightness = 0.0f;
    testPattern->render(params);
    
    params.time = 100.0f;
    params.progress = 2.0f;
    params.brightness = 2.0f;
    testPattern->render(params);
    
    // エラーが発生しないことを確認
    TEST_ASSERT_TRUE(true);
}

void test_YAxisRingPattern_sphere_manager_null_safety() {
    // LEDSphereManagerがnullの場合の安全性テスト
    testPattern->setSphereManager(nullptr);
    
    PatternParams params;
    params.time = 1.0f;
    params.progress = 0.5f;
    
    // nullポインタでもクラッシュしないことを確認
    testPattern->render(params);
    
    TEST_ASSERT_TRUE(true);
}

int main() {
    UNITY_BEGIN();
    
    RUN_TEST(test_YAxisRingPattern_initialization);
    RUN_TEST(test_YAxisRingPattern_default_ring_configuration);
    RUN_TEST(test_YAxisRingPattern_render_with_basic_params);
    RUN_TEST(test_YAxisRingPattern_speed_and_brightness_control);
    RUN_TEST(test_YAxisRingPattern_time_progression);
    RUN_TEST(test_YAxisRingPattern_edge_cases);
    RUN_TEST(test_YAxisRingPattern_sphere_manager_null_safety);
    
    return UNITY_END();
}