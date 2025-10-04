#include <unity.h>
#include "led/SphereStripController.h"

// テスト用のモック設定
namespace sphere_test {
    // 期待されるLED制御動作をテスト

    void test_constructor_initializes_correctly() {
        SphereStripController controller;
        
        // コンストラクタが正常に実行されること
        TEST_ASSERT_FALSE(controller.isInitialized());
        TEST_ASSERT_EQUAL(0, controller.getNumLeds());
        TEST_ASSERT_EQUAL(255, controller.getBrightness());
    }

    void test_initialize_with_valid_parameters() {
        SphereStripController controller;
        
        // 初期化が成功すること
        bool result = controller.initialize(GPIO_NUM_46, 800); // GPIO46, 800 LEDs
        TEST_ASSERT_TRUE(result);
        
        // 初期化後の状態確認
        TEST_ASSERT_TRUE(controller.isInitialized());
        TEST_ASSERT_EQUAL(800, controller.getNumLeds());
        TEST_ASSERT_EQUAL(GPIO_NUM_46, controller.getDataPin());
    }

    void test_initialize_with_invalid_parameters() {
        SphereStripController controller;
        
        // 無効なピン番号
        bool result1 = controller.initialize(GPIO_NUM_NC, 800);
        TEST_ASSERT_FALSE(result1);
        
        // 無効なLED数
        bool result2 = controller.initialize(GPIO_NUM_46, 0);
        TEST_ASSERT_FALSE(result2);
        
        // LED数上限超過
        bool result3 = controller.initialize(GPIO_NUM_46, 10000);
        TEST_ASSERT_FALSE(result3);
    }

    void test_set_single_led_color() {
        SphereStripController controller;
        controller.initialize(46, 800);
        
        // 単一LED色設定
        CRGB red = CRGB::Red;
        bool result = controller.setLedColor(0, red);
        TEST_ASSERT_TRUE(result);
        
        // 設定した色の取得確認
        CRGB retrieved = controller.getLedColor(0);
        TEST_ASSERT_EQUAL(red.r, retrieved.r);
        TEST_ASSERT_EQUAL(red.g, retrieved.g);
        TEST_ASSERT_EQUAL(red.b, retrieved.b);
    }

    void test_set_led_color_out_of_bounds() {
        SphereStripController controller;
        controller.initialize(46, 800);
        
        // 範囲外インデックス
        CRGB blue = CRGB::Blue;
        bool result1 = controller.setLedColor(-1, blue);
        TEST_ASSERT_FALSE(result1);
        
        bool result2 = controller.setLedColor(800, blue);
        TEST_ASSERT_FALSE(result2);
    }

    void test_clear_all_leds() {
        SphereStripController controller;
        controller.initialize(46, 800);
        
        // いくつかのLEDに色を設定
        controller.setLedColor(0, CRGB::Red);
        controller.setLedColor(100, CRGB::Green);
        controller.setLedColor(799, CRGB::Blue);
        
        // 全LEDクリア
        controller.clear();
        
        // 全てのLEDが黒（オフ）になっていることを確認
        for (int i = 0; i < 800; i++) {
            CRGB color = controller.getLedColor(i);
            TEST_ASSERT_EQUAL(0, color.r);
            TEST_ASSERT_EQUAL(0, color.g);
            TEST_ASSERT_EQUAL(0, color.b);
        }
    }

    void test_show_updates_physical_leds() {
        SphereStripController controller;
        controller.initialize(46, 800);
        
        // 色設定
        controller.setLedColor(0, CRGB::Red);
        controller.setLedColor(1, CRGB::Green);
        
        // 物理LEDへの更新実行
        // 注意: 実機テストでは実際のLEDが光ることを目視確認
        bool result = controller.show();
        TEST_ASSERT_TRUE(result);
    }

    void test_set_brightness() {
        SphereStripController controller;
        controller.initialize(46, 800);
        
        // 輝度設定（0-255）
        controller.setBrightness(128);
        TEST_ASSERT_EQUAL(128, controller.getBrightness());
        
        // 境界値テスト
        controller.setBrightness(0);
        TEST_ASSERT_EQUAL(0, controller.getBrightness());
        
        controller.setBrightness(255);
        TEST_ASSERT_EQUAL(255, controller.getBrightness());
    }

    void test_performance_bulk_updates() {
        SphereStripController controller;
        controller.initialize(46, 800);
        
        // 性能テスト: 全LED更新時間測定
        unsigned long start_time = micros();
        
        // 全LEDに色設定
        for (int i = 0; i < 800; i++) {
            CRGB color = CRGB(i % 255, (i * 2) % 255, (i * 3) % 255);
            controller.setLedColor(i, color);
        }
        controller.show();
        
        unsigned long elapsed = micros() - start_time;
        
        // 目標: 10ms（10000us）以内で全LED更新
        TEST_ASSERT_LESS_THAN(10000, elapsed);
    }

    void test_memory_usage() {
        SphereStripController controller;
        
        // メモリ使用量テスト（初期化前後）
        size_t free_heap_before = ESP.getFreeHeap();
        
        controller.initialize(46, 800);
        
        size_t free_heap_after = ESP.getFreeHeap();
        size_t used_memory = free_heap_before - free_heap_after;
        
        // 期待メモリ使用量: 800 LEDs × 3 bytes × 2 (double buffer) = 4800 bytes
        // 余裕を見て 6KB以内であることを確認
        TEST_ASSERT_LESS_THAN(6144, used_memory);
    }
}

// Unity テストランナー
void setUp(void) {
    // 各テスト前の初期化
}

void tearDown(void) {
    // 各テスト後のクリーンアップ
}

int main() {
    UNITY_BEGIN();
    
    // 基本機能テスト
    RUN_TEST(sphere_test::test_constructor_initializes_correctly);
    RUN_TEST(sphere_test::test_initialize_with_valid_parameters);
    RUN_TEST(sphere_test::test_initialize_with_invalid_parameters);
    
    // LED制御テスト
    RUN_TEST(sphere_test::test_set_single_led_color);
    RUN_TEST(sphere_test::test_set_led_color_out_of_bounds);
    RUN_TEST(sphere_test::test_clear_all_leds);
    RUN_TEST(sphere_test::test_show_updates_physical_leds);
    RUN_TEST(sphere_test::test_set_brightness);
    
    // 性能テスト
    RUN_TEST(sphere_test::test_performance_bulk_updates);
    RUN_TEST(sphere_test::test_memory_usage);
    
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