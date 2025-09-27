# Repository Guidelines


## Conversation Guidelines
- 思考は英語で，私との会話は日本語で
- 常に日本語で会話する
- 作業に入る前に `isolation_sphere_spec.md` と `isolation_sphere_tasks.md` の内容を確認し、要件と優先タスクを把握する

## Development Philosophy

### Test-Driven Development (TDD)

- 原則としてテスト駆動開発（TDD）で進める
- 期待される入出力に基づき、まずテストを作成する
- 実装コードは書かず、テストのみを用意する
- テストを実行し、失敗を確認する
- テストが正しいことを確認できた段階でコミットする
- その後、テストをパスさせる実装を進める
- 実装中はテストを変更せず、コードを修正し続ける
- すべてのテストが通過するまで繰り返す
- テストはcpp，pythonで作成する場合はクラスとして実装することでカプセル化し，本実装時にはそのクラスを使って実装する

### classベースの機能実装
- config, buzzer, LCD, IMUなどタスクで分かれているものはクラス化できるか検討
- ユニットテストはクラス単位で作成し，検証する
- プログラム本体はこれらのクラスを組み合わせて実装するスタイル

## 機能リスト

### config.json読み込み機能（ConfigManager）

### WiFi AP機能（外部WiFi接続なし）

- **APモード固定**: isolation-joystick として独立したローカルネットワークを提供
- **IP範囲**: 192.168.100.x (デフォルト)
- **STAモード無効**: 外部WiFiルーターへの接続は行わない（Raspberry Piが担当）
- **理由**: 制御系の独立性確保、外部ネットワーク障害時の直接制御維持

### MQTTブローカー（内蔵）

## ハードウェア

### joystick

- stampfly用のjoystickを流用（["switch science"](https://www.switch-science.com/products/9819?_pos=4&_sid=1d9aad1f7&_ss=r)）し，これを使ってisolation-sphereをコントロールする
  - アナログスティック２基
  - アナログスティック押し込み２基
  - ボタン２基
  - M5atomのLCDボタン
  - IMUセンサを使ったコントロール
- ESP32-S3R搭載の小型マイコンボード

## UI機能リスト

- config.json読み込み機能（ConfigManager）
- MQTTブローカー
- wifi AP機能
- UI
  - Demo：デモ時に必要な機能をパッケージ
  - Contorol：isolation-sphere本体をコントロールするUI
  - Video：再生コントロール，プレイリスト，
  - Maintain：キャリブレーション，オフセット
  - System：再起動，システム関連

## Project Structure & Module Organization

- `platformio.ini` defines the `atoms3r` PlatformIO environment, board-specific clock settings, and Arduino-based dependencies; review it before adding new hardware features.
- `src/main.cpp` contains the firmware entry points (`setup()` and `loop()`); keep hardware abstractions in separate translation units once they stabilize.
- Place shared headers in `include/` and reusable components or vendor forks in `lib/`; prepend module directories with the subsystem (e.g., `display_led/`).
- Add Unity test suites under `test/`; mirror the runtime module layout to keep fixtures discoverable.
- フロントエンド周りは `/Users/katano/Documents/PlatformIO/Projects/MFT2025/atom-joystick/test_sketches/14_udp_joystick_integration/14_udp_joystick_integration.ino` の実装を移植する。

## Build, Test, and Development Commands
- `pio run` builds the active environment; append `-e atoms3r` explicitly when you script CI tasks.
- `pio run --target upload` compiles and flashes the firmware over USB; confirm the board is in bootloader mode before running.
- `pio device monitor -b 115200` opens the serial console with the configured filters (exception decoder and timestamp).
- `pio run --target clean` removes intermediate artifacts; invoke it before benchmarking flash or RAM usage.

## Coding Style & Naming Conventions
- Follow Arduino C++ style with two-space indentation and braces on the same line for functions.
- Name classes and structs in PascalCase, functions in lowerCamelCase, constants and macros in UPPER_SNAKE_CASE, and pins as `PIN_<role>`.
- Prefer `constexpr` over macros for typed constants, guard headers with `#pragma once`, and localize FastLED/M5Unified configuration in dedicated helpers.

## Testing Guidelines
- Use PlatformIO’s Unity framework; create files like `test_sensor.cpp` under `test/` with `UNITY_BEGIN()`/`UNITY_END()` runners.
- Run `pio test -e atoms3r` before committing; add mocks for external buses (I2C/SPI) to keep tests deterministic.
- Target functional coverage of new modules and add regression tests when fixes land.
- `/Users/katano/Documents/PlatformIO/Projects/MFT2025/isolation-sphere` で運用している個別機能テストは流用し、同一手順で実行する。

## Commit & Pull Request Guidelines
- Git metadata is not initialized; establish history with imperative commit subjects (e.g., `feat: add imu driver`) and include scope tags.
- Reference linked issues in commit bodies or PR descriptions, describe hardware tested, and attach serial logs or photos when the change affects physical behavior.
- Keep PRs focused on one subsystem, outline validation steps, and request review from maintainers responsible for the touched hardware stack.

## Configuration & Environment Tips
- The project assumes an ESP32-S3 based M5Stack Atom S3R; verify you are on the same Arduino core version pinned in `platformio.ini` to avoid toolchain drift.
- When adding libraries, declare them in `lib_deps` and prefer versions compatible with PSRAM and FastLED to maintain stable builds.
- LittleFS にアップロードするアセットは `/Users/katano/Documents/PlatformIO/Projects/MFT2025/data` に集約される。`pio run -t buildfs/uploadfs` 前にこのディレクトリを同期し、共通設定ファイル `config.json` を必ず参照する。
- `config.json` の構造やモジュール毎の項目は `../doc/define_config.md` に記載されている。設定を追加・編集する場合は必ず同ドキュメントを更新し、分類ルールに従う。

## MQTT Topic Rules
- 詳細なトピック設計ガイドは `../doc/mqtt_rules.md` に記載されています（`isolation-sphere` と共通）。実装時は必ず参照し、`sphere/ui/#` などの階層構造や JSON ペイロード運用の方針に従ってください。

## UI Rules
- UI 実装ガイドは `../doc/ui_rules.md` を参照する（両プロジェクト共通）。

## Configuration Docs
`config.json` の構造と共通設定に関する規約は `../doc/define_config.md` に集約しています。Sphere / Joystick / RasPi 各モジュールで設定を追加・変更する際は、ドキュメントのルールと項目分類に従ってください。
