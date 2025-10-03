# Maintenance & Verification Checklist

実機での点検を効率化するため、`isolation-sphere` の動作確認手順をまとめました。

## 前提
- `pio run -t upload -e atoms3r_bmi270`（またはターゲット環境）で最新ファームを転送済み。
- `pio device monitor -b 115200` 等でシリアルモニタを開き、ログを記録。
- `data/config.json` の `led.strips` / `leds_per_strip` / `sphere.instances` 等が現行ハード構成と一致していることを確認。

---

## 1. LED 点灯・消灯テスト
1. `examples/test_strip/TestStripDemo` をビルド／書き込み。
2. シリアルに `Frame` ログが出ることを確認。
3. 全ストリップが順番に赤→緑→青→黄の単点移動で点灯する。
4. 全消灯が入ることを肉眼で確認（1 周期中に黒フレームが存在）。
5. 期待通りでなければ `src/test_strip/TestStripDemo.cpp` 内の `pins[]` や `STRIP_LENGTHS` を調整。

### 追加チェック
- `TEST_STRIP_LOOP_DELAY_MS` を 0 に設定し、`TEST_STRIP_MEASURE_PIN` にオシロを接続 → 更新周期（`FastLED.show()`）を計測。

---

## 2. `led_layout.csv` レイアウト検証
1. `layout/layout_test/layout_visualizer.py` を実行し、座標が球体で正しく分布しているか確認。
   ```bash
   cd layout/layout_test
   python3 layout_visualizer.py ../../data/led_layout.csv
   ```
2. 出力画像（`layout_preview.png`）を確認し、極・赤道が物理位置と一致しているかを目視確認。
3. 必要に応じて CSV を修正し、再度スクリプトを回す。

---

## 3. 表示パターン確認
1. 通常ビルドで `ProceduralPatternGenerator` が提供する主要パターン（オープニング、IMU 連動パターン）を順に呼び出す。
2. MQTT あるいは UI から各パターン ID を切り替え、期待する色／動きになっているか確認。
3. テストやデバッグには `PatternId::kTestStrip`（単色）や `kAxes`, `kRainbow` などシンプルなものを活用する。

---

## 4. 色・明るさ
1. `config.json` の `led.brightness` を変更し、LED の輝度が追従するか確認。
2. `ProceduralPatternGenerator` 側で `SetGlobalBrightness()` を変化させ、色飽和が起こらないかチェック。
3. 必要に応じて `StripController::setBrightness()` や `FastLED.setBrightness()` を調整。

---

## 5. IMU 連携
1. 起動後に IMU データがシリアルへ出力されるか確認（`[Core1][IMU] q=...` ログ）。
2. Shake ジェスチャーで UI モード切り替えができるか、また IMU の姿勢に応じて Procedural パターンが追従するか確認。
3. キャリブレーション完了時に `Calibration saved` ログとブザー通知が鳴るか確認。

---

## 6. Procedural オープニング
1. リセット後、ブザー起動音 → Procedural オープニング → キャリブレーション完了音の順に遷移することを確認。
2. オープニング描画中に大きな揺れ・ちらつきが無いか観察。
3. 必要に応じて `FastBootOrchestrator` や `ProceduralOpeningSequence` の設定値（フェーズ時間・FPS）を調整。

---

## 7. 起動シーケンス通し確認
1. シリアルログが `StorageManager` → `WiFiManager` → `MqttService` → `ProceduralOpening` の順に進むか確認。
2. エラーが出ず `System Ready` まで到達することをチェック。
3. 必要に応じてログを保存し、今後比較できるようナレッジベースへ登録。

---

このチェックリストを元に、定期的なメンテナンス時に確認項目を洗い出し、必要に応じて追加タスクを `isolation_sphere_tasks.md` に反映してください。
