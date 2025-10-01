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
static constexpr uint16_t LEDS_PER_STRIP = 32;
static constexpr uint32_t NUM_LEDS = (uint32_t)STRIPS * LEDS_PER_STRIP;

static CRGB leds[NUM_LEDS];
static TestStripPattern testPattern(STRIPS, LEDS_PER_STRIP);

void setup() {
  // シリアルは 100 フレームごとのログ用に初期化
  Serial.begin(115200);
  delay(50);
  FastLED.addLeds<WS2812B, TEST_STRIP_DATA_PIN, GRB>(leds, NUM_LEDS);
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
    uint16_t pos = currentFrame % LEDS_PER_STRIP;
    Serial.print("Frame ");
    Serial.print(currentFrame);
    Serial.print(" Pos ");
    Serial.println(pos);
  }

  if (TEST_STRIP_LOOP_DELAY_MS > 0) {
    delay(TEST_STRIP_LOOP_DELAY_MS);
  }
}
