#include "imu/ShakeDetector.h"
#include <cmath>

ShakeDetector::ShakeDetector(float threshold, int triggerCount, uint32_t windowMs,
                             uint32_t refractoryMs, uint32_t cooldownMs)
    : threshold_(threshold), triggerCount_(triggerCount), windowMs_(windowMs),
      refractoryMs_(refractoryMs), cooldownMs_(cooldownMs),
      lastShakeMs_(0), lastNotifyMs_(0) {}

void ShakeDetector::configure(int triggerCount, uint32_t windowMs, uint32_t refractoryMs, uint32_t cooldownMs) {
    triggerCount_ = triggerCount;
    windowMs_ = windowMs;
    refractoryMs_ = refractoryMs;
    cooldownMs_ = cooldownMs;
}

void ShakeDetector::reset() {
    history_.clear();
    lastShakeMs_ = 0;
    lastNotifyMs_ = 0;
}

bool ShakeDetector::inRefractory(uint32_t now) const {
    return (now - lastShakeMs_ < refractoryMs_);
}

bool ShakeDetector::inCooldown(uint32_t now) const {
    return (now - lastNotifyMs_ < cooldownMs_);
}

bool ShakeDetector::update(float ax, float ay, float az, uint32_t timestampMs) {
    float mag = std::sqrt(ax*ax + ay*ay + az*az);
    history_.push_back({mag, timestampMs});
    // 古いデータを削除
    while (!history_.empty() && (timestampMs - history_.front().timestampMs > windowMs_)) {
        history_.erase(history_.begin());
    }
    if (inRefractory(timestampMs)) return false;
    int count = 0;
    for (const auto& h : history_) {
        if (std::fabs(h.mag - 9.8f) > threshold_) count++;
    }
    if (count >= triggerCount_) {
        lastShakeMs_ = timestampMs;
        if (!inCooldown(timestampMs)) {
            lastNotifyMs_ = timestampMs;
            return true;
        }
    }
    return false;
}
