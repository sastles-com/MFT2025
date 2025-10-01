#pragma once
#include <FastLED.h>
#include <cstdint>
#include <vector>

class TestStripPattern {
public:
  // strips: ストリップ数、ledsPerStrip: 各ストリップあたりのLED数
  TestStripPattern(uint8_t strips = 4, uint16_t ledsPerStrip = 200);

  // フレーム番号を与えて描画（呼び出し側でフレームをインクリメント）
  // leds: 全LED配列（stride = strips * ledsPerStrip）
  void renderFrame(CRGB *leds, uint32_t frame);

  void setBrightness(uint8_t b) { brightness_ = b; }

private:
  uint8_t strips_;
  uint16_t ledsPerStrip_;
  uint8_t brightness_;
  std::vector<CRGB> colors_;
};