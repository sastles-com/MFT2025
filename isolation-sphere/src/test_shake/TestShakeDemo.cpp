#include <Arduino.h>
#include "imu/ShakeDetector.h"

// 単純なデモランナー: 既存の ShakeDetector 実装を呼んでシリアル出力する
ShakeDetector detector(2.0f, 2, 1000, 2000, 1000);

// 設定
constexpr int DIAG_PIN = 2; // diagnostic pin
const uint32_t RUN_DURATION_MS = 60 * 1000; // 1分

// ランタイム統計
static uint32_t runStartMs = 0;
static int loopCount = 0;
static int detectionCount = 0;

void pulseDiag(uint32_t ms) {
  digitalWrite(DIAG_PIN, HIGH);
  delay(ms);
  digitalWrite(DIAG_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("TestShakeDemo start");
  // diagnostic: print free heap and PSRAM status
  Serial.print("Free heap: ");
  Serial.println(ESP.getFreeHeap());
#if CONFIG_ESP32_SPIRAM_SUPPORT
  Serial.print("PSRAM: ");
  Serial.println(psramFound() ? "found" : "not found");
#endif
  // diagnostic pin
  pinMode(DIAG_PIN, OUTPUT);
  digitalWrite(DIAG_PIN, LOW);
  // 短いパルスで setup が完了したことを外部で検出できるようにする
  pulseDiag(10);

  // ラン開始時刻を記録
  runStartMs = millis();
}

void loop() {
  static uint32_t t = 1000;

  if ((millis() - runStartMs) >= RUN_DURATION_MS) {
    Serial.println("--- Test duration reached ---");
    Serial.print("Total loops: "); Serial.println(loopCount);
    Serial.print("Detections: "); Serial.println(detectionCount);
    Serial.println("Test finished. Entering idle.");
    while (true) {
      delay(1000);
    }
  }

  loopCount++;

  // 擬似的に 2 回の大きな加速度を 300ms 間隔で与えて検出を確認
  // (閾値 = 2.0 のため，これらは明確に閾値を超える)
  float ax1 = 0.0f;
  float ax2 = 12.0f; // ここを大きくして閾値を超えるようにする
  float ax3 = -12.0f;

  // 常に検出アルゴリズムへは入力を送るが、シリアル出力は 100 フレーム毎に抑制する
  bool detected1 = detector.update(ax1, 0.0f, 9.8f, t);
  t += 100;
  bool detected2 = detector.update(ax2, 0.0f, 9.8f, t);
  t += 200;
  bool detected3 = detector.update(ax3, 0.0f, 9.8f, t);

  // 検出が起きたら即座にカウントし、DIAG_PIN を短くパルスする（シリアルは抑制）
  if (detected1 || detected2 || detected3) {
    detectionCount++;
    pulseDiag(50);
  }

  // テスト用: 毎ループで簡潔なログを出す（本番では 100 フレーム毎に戻す）
  {
    Serial.println("Feeding synthetic samples (detailed)...");
    auto printMag = [&](const char* name, float ax, float ay, float az, uint32_t ts){
      float mag = sqrt(ax*ax + ay*ay + az*az);
      Serial.print(name); Serial.print(" @"); Serial.print(ts); Serial.print(" ms mag="); Serial.println(mag);
    };
    // 出力用に再計算
    printMag("ax1", ax1, 0.0f, 9.8f, t - 300);
    printMag("ax2", ax2, 0.0f, 9.8f, t - 200);
    printMag("ax3", ax3, 0.0f, 9.8f, t - 0);
    Serial.print("Detected this loop: ");
    Serial.print(detected1 ? "D1 " : "");
    Serial.print(detected2 ? "D2 " : "");
    Serial.print(detected3 ? "D3 " : "");
    if (!(detected1 || detected2 || detected3)) Serial.print("NONE");
    Serial.println("");
    Serial.print("Loop: "); Serial.print(loopCount); Serial.print("  Total detections: "); Serial.println(detectionCount);
  }

  // 1秒毎にループ
  delay(1000);
}
