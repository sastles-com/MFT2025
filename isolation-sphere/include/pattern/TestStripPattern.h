#pragma once
#include <FastLED.h>
#include <cstdint>
#include <vector>

// TestStripPattern: 各ストリップごとに異なる長さを持てるように変更
class TestStripPattern {
public:
  // ledsPerStrip: 各ストリップのLED数を保持する配列（サイズ = ストリップ数）
  TestStripPattern(const std::vector<uint16_t>& ledsPerStrip = std::vector<uint16_t>{200,200,200,200});

  // フレーム番号を与えて描画（呼び出し側でフレームをインクリメント）
  // leds: 全LED配列（stride = sum(ledsPerStrip)）
  void renderFrame(CRGB *leds, uint32_t frame);

  void setBrightness(uint8_t b) { brightness_ = b; }

private:
  std::vector<uint16_t> ledsPerStrip_;
  uint8_t brightness_;
  std::vector<CRGB> colors_;
};