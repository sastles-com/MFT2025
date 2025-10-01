#include <unity.h>
#include "imu/ImuService.h"
#include <vector>
#include <math.h>

// 仮のShakeDetectorインターフェース
class ShakeDetector {
public:
    ShakeDetector(float threshold, int triggerCount, uint32_t windowMs)
        : threshold_(threshold), triggerCount_(triggerCount), windowMs_(windowMs) {}

    // 加速度データを時系列で追加し、shake判定
    bool update(float ax, float ay, float az, uint32_t timestampMs) {
        float mag = sqrtf(ax*ax + ay*ay + az*az);
        history_.push_back({mag, timestampMs});
        // 古いデータを削除
        while (!history_.empty() && (timestampMs - history_.front().timestampMs > windowMs_)) {
            history_.erase(history_.begin());
        }
        // 閾値超え回数カウント
        int count = 0;
        for (const auto& h : history_) {
            if (fabs(h.mag - 9.8f) > threshold_) count++;
        }
        return count >= triggerCount_;
    }
private:
    struct Entry { float mag; uint32_t timestampMs; };
    std::vector<Entry> history_;
    float threshold_;
    int triggerCount_;
    uint32_t windowMs_;
};

void test_shake_simple() {
    ShakeDetector detector(2.0f, 2, 1000); // 2回/1秒, 2G以上
    uint32_t t = 1000;
    // 通常時
    TEST_ASSERT_FALSE(detector.update(0.0f, 0.0f, 9.8f, t));
    TEST_ASSERT_FALSE(detector.update(0.1f, 0.2f, 9.7f, t+100));
    // 1回目のshake
    TEST_ASSERT_FALSE(detector.update(5.0f, 0.0f, 9.8f, t+200));
    // 2回目のshake（1秒以内）
    TEST_ASSERT_TRUE(detector.update(-5.0f, 0.0f, 9.8f, t+500));
}

void test_shake_window() {
    ShakeDetector detector(2.0f, 2, 300); // 2回/0.3秒
    uint32_t t = 2000;
    // 1回目
    TEST_ASSERT_FALSE(detector.update(5.0f, 0.0f, 9.8f, t));
    // 2回目（ウィンドウ外）
    TEST_ASSERT_FALSE(detector.update(-5.0f, 0.0f, 9.8f, t+400));
    // 2回目（ウィンドウ内）
    TEST_ASSERT_TRUE(detector.update(-5.0f, 0.0f, 9.8f, t+200));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_shake_simple);
    RUN_TEST(test_shake_window);
    return UNITY_END();
}
