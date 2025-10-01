# 起動シーケンス概要

## 1. システム起動と M5Unified 準備
- `Serial.begin(115200)` でログを準備し、500ms 待機。
- `M5.config()` を調整して AtomS3R に合わせた設定（IMU/RTC/5V 出力など）を有効化後、`M5.begin(cfg)` を実行。

## 2. IMU 初期化
- BMI270/BMM150 内蔵 IMU を最大 5 回までリトライしながら `M5.Imu.begin()` で起動。
- I2C バスのスキャン結果をシリアルに出力し、センサの接続状態を可視化。
- `ImuService.begin()` を呼び、姿勢データ取得と UI ジェスチャー連携（`ShakeDetector`/`ShakeToUiBridge`）を構築。

## 3. ストレージ準備
- LittleFS をフォーマット＆マウントして破損をリカバリ。
- 現状 PSRamFS はライブラリアップデート待ちのため初期化をスキップ（ログに明示）。

## 4. BootOrchestrator 連携
- `BootOrchestrator::Callbacks`
  - `onStorageReady`: LittleFS/PSRamFS のマウント結果をログ出力。
  - `stageAssets`: PSRamFS に `/images` を作成、重いアセットコピーは後続タスクへ委譲。
- `BootOrchestrator::Services`
  - `displayInitialize`: `DisplayController` で LCD を初期化。
  - `playStartupTone`: `BuzzerService` を用いた起動音再生（設定で無効化可能）。

## 5. BootOrchestrator 実行
- `BootOrchestrator.run()` がストレージ確認 → 設定ロード → コールバック実行を直列で処理。
- 失敗時にはシリアルログで通知し、復旧フローを追いやすくする。

## 6. UI ジェスチャー設定
- `config.json` の IMU セクションから UI シェイク閾値を読み取り、`ShakeDetector` に反映。
- `ShakeToUiBridge` を介してシェイク → UI モード遷移をトリガー。

## 7. LED サブシステム
- `USE_FASTLED` 定義時のみ `FastLED.addLeds` で LED を初期化し、RGB 点灯テストを実行。
- `LEDSphereManager` と `ProceduralPatternPerformanceTest` を初期化して球体表示とベンチマークの準備を整える。

## 8. オープニングアニメーション
- PSRamFS に `/images/opening/001.jpg` が存在すれば `playOpeningAnimation()` で再生。
- 無い場合はデモ用 JPEG を生成し、動作確認に利用。

## 9. CoreTask 統合（今後の課題）
- 現段階では `CoreTasks` 呼び出しはコメントアウト。将来的に Core0/Core1 並列化へ移行する際のチェックポイントとして記録。

---

この文書は `src/main.cpp` の実装状況（2024-12 時点）を基にしており、BootOrchestrator や周辺サービスの改修時は本ファイルの更新を忘れないでください。
