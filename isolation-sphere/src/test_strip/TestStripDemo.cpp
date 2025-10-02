#include <Arduino.h>
#include <FastLED.h>
#include "pattern/TestStripPattern.h"

// #define TEST_STRIP_DATA_PIN 8

// ハードウェアに合わせてDATA_PINを設定してください（例: 35）
#ifndef TEST_STRIP_DATA_PIN
#define TEST_STRIP_DATA_PIN 35
#endif

// 観測用パルスを出すGPIOピン（オシロスコープで測定してください）。必要ならボードに合わせて変更。
#ifndef TEST_STRIP_MEASURE_PIN
#define TEST_STRIP_MEASURE_PIN 2
#endif

// ループの待ち時間（ミリ秒）。0 にすると delay を入れず最大スループットを測定できます。
#ifndef TEST_STRIP_LOOP_DELAY_MS
#define TEST_STRIP_LOOP_DELAY_MS 100
#endif

static constexpr uint8_t STRIPS = 4;
// 各ストリップのLED数: data/config.json と一致させる例
static const uint16_t STRIP_LENGTHS[STRIPS] = {180, 220, 180, 220};
static uint32_t computeTotalLeds() {
  uint32_t t = 0;
  for (uint8_t i = 0; i < STRIPS; ++i) t += STRIP_LENGTHS[i];
  return t;
}

static constexpr uint32_t NUM_LEDS = 0; // ダミー、実行時に確保

// 動的確保: 大きい配列はスタック上に置かない
static CRGB *leds = nullptr;
// TestStripPattern に各ストリップの長さを渡す
static TestStripPattern testPattern(std::vector<uint16_t>(STRIP_LENGTHS, STRIP_LENGTHS + STRIPS));

void setup() {
  // シリアルは 100 フレームごとのログ用に初期化
  Serial.begin(115200);
  delay(50);
  // 動的にフレームバッファを確保
  uint32_t total = computeTotalLeds();
  leds = (CRGB*)malloc(sizeof(CRGB) * total);
  if (!leds) {
    Serial.println("Failed to allocate LED buffer");
    while (1) delay(1000);
  }

  // 各ストリップを個別ピンに割り当てて登録する
  // 実際のハード配線に合わせてピンを変更してください
  const uint8_t pins[STRIPS] = {5, 6, 7, 8};
  uint32_t offset = 0;
  for (uint8_t s = 0; s < STRIPS; ++s) {
    uint16_t len = STRIP_LENGTHS[s];
    switch (pins[s]) {
      case 0: FastLED.addLeds<WS2812B, 0, GRB>(&leds[offset], len); break;
      case 1: FastLED.addLeds<WS2812B, 1, GRB>(&leds[offset], len); break;
      case 2: FastLED.addLeds<WS2812B, 2, GRB>(&leds[offset], len); break;
      case 3: FastLED.addLeds<WS2812B, 3, GRB>(&leds[offset], len); break;
      case 4: FastLED.addLeds<WS2812B, 4, GRB>(&leds[offset], len); break;
      case 5: FastLED.addLeds<WS2812B, 5, GRB>(&leds[offset], len); break;
      case 6: FastLED.addLeds<WS2812B, 6, GRB>(&leds[offset], len); break;
      case 7: FastLED.addLeds<WS2812B, 7, GRB>(&leds[offset], len); break;
      case 8: FastLED.addLeds<WS2812B, 8, GRB>(&leds[offset], len); break;
      case 9: FastLED.addLeds<WS2812B, 9, GRB>(&leds[offset], len); break;
      default:
        // 非対応ピンはログのみ
        Serial.print("Unsupported pin in demo registration: ");
        Serial.println(pins[s]);
        break;
    }
    offset += len;
  }

  FastLED.setBrightness(128);

  pinMode(TEST_STRIP_MEASURE_PIN, OUTPUT);
  digitalWrite(TEST_STRIP_MEASURE_PIN, LOW);
}

void loop() {
  static uint32_t frame = 0;

  // 観測パルス: 各フレームで短いパルスを出す（オシロ等で測定）
  digitalWrite(TEST_STRIP_MEASURE_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TEST_STRIP_MEASURE_PIN, LOW);

  testPattern.renderFrame(leds, frame++);
  FastLED.show();

  // 表示中のフレーム（frame はインクリメント済み）
  uint32_t currentFrame = (frame == 0) ? 0 : frame - 1;
  if (currentFrame % 100 == 0) {
    // ログには第0ストリップの長さに対する位置を表示
    uint16_t pos = currentFrame % STRIP_LENGTHS[0];
    Serial.print("Frame ");
    Serial.print(currentFrame);
    Serial.print(" Pos ");
    Serial.println(pos);
  }

  if (TEST_STRIP_LOOP_DELAY_MS > 0) {
    delay(TEST_STRIP_LOOP_DELAY_MS);
  }
}
