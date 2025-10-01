#include "pattern/TestStripPattern.h"

TestStripPattern::TestStripPattern(uint8_t strips, uint16_t ledsPerStrip)
  : strips_(strips), ledsPerStrip_(ledsPerStrip), brightness_(128) {
  colors_.reserve(4);
  colors_.push_back(CRGB::Red);
  colors_.push_back(CRGB::Green);
  colors_.push_back(CRGB::Blue);
  colors_.push_back(CRGB::Yellow);
}

// frame に応じて各ストリップの点灯位置を移動させる。
// 呼び出し前に leds[] 全体は書き換え可能（本実装では毎フレームでクリアして上書き）
void TestStripPattern::renderFrame(CRGB *leds, uint32_t frame) {
  if (!leds) return;

  const uint32_t total = (uint32_t)strips_ * (uint32_t)ledsPerStrip_;
  // 全消灯（簡潔さ優先、最適化は必要なら検討）
  for (uint32_t i = 0; i < total; ++i) {
    leds[i] = CRGB::Black;
  }

  for (uint8_t s = 0; s < strips_; ++s) {
    uint16_t index = (uint16_t)(frame % ledsPerStrip_);
    uint32_t pos = (uint32_t)s * ledsPerStrip_ + index;
    CRGB c = colors_[s % colors_.size()];
    c.nscale8_video(brightness_);
    leds[pos] = c;
  }
}