#pragma once
#include <vector>
#include <cstdint>

class ShakeDetector {
public:
    ShakeDetector(float threshold, int triggerCount, uint32_t windowMs,
                  uint32_t refractoryMs = 2000, uint32_t cooldownMs = 1000);

    void configure(int triggerCount, uint32_t windowMs, uint32_t refractoryMs, uint32_t cooldownMs);
    bool update(float ax, float ay, float az, uint32_t timestampMs);
    void reset();

private:
    struct Entry { float mag; uint32_t timestampMs; };
    std::vector<Entry> history_;
    float threshold_;
    int triggerCount_;
    uint32_t windowMs_;
    uint32_t refractoryMs_;
    uint32_t cooldownMs_;
    uint32_t lastShakeMs_;
    uint32_t lastNotifyMs_;
    bool inRefractory(uint32_t now) const;
    bool inCooldown(uint32_t now) const;
};
