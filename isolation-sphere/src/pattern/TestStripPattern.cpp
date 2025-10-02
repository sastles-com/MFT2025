#include "pattern/TestStripPattern.h"

TestStripPattern::TestStripPattern(const std::vector<uint16_t>& ledsPerStrip)
  : ledsPerStrip_(ledsPerStrip), brightness_(128) {
  colors_.reserve(4);
  colors_.push_back(CRGB::Red);
  colors_.push_back(CRGB::Green);
  colors_.push_back(CRGB::Blue);
  colors_.push_back(CRGB::Yellow);
}

// frame に応じて各ストリップの点灯位置を移動させる。
// 呼び出し前に leds[] 全体は書き換可能（本実装では毎フレームでクリアして上書き）
void TestStripPattern::renderFrame(CRGB *leds, uint32_t frame) {
  if (!leds) return;

  // 全LED合計を計算
  uint32_t total = 0;
  for (auto v : ledsPerStrip_) total += v;

  // 全消灯
  for (uint32_t i = 0; i < total; ++i) {
    leds[i] = CRGB::Black;
  }

  // 各ストリップ毎に移動ドットを表示
  uint32_t offset = 0;
  for (size_t s = 0; s < ledsPerStrip_.size(); ++s) {
    uint16_t len = ledsPerStrip_[s];
    if (len == 0) continue;
    uint16_t index = (uint16_t)(frame % len);
    uint32_t pos = offset + index;
    CRGB c = colors_[s % colors_.size()];
    c.nscale8_video(brightness_);
    leds[pos] = c;
    offset += len;
  }
}