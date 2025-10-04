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

### classベースの機能実装
- config, buzzer, LCD, IMUなどタスクで分かれているものはクラス化できるか検討
- ユニットテストはクラス単位で作成し，検証する
- プログラム本体はこれらのクラスを組み合わせて実装するスタイル



## Project Structure & Module Organization
- `platformio.ini` defines the `atoms3r` PlatformIO environment, board-specific clock settings, and Arduino-based dependencies; review it before adding new hardware features.
- `src/main.cpp` contains the firmware entry points (`setup()` and `loop()`); keep hardware abstractions in separate translation units once they stabilize.
- Place shared headers in `include/` and reusable components or vendor forks in `lib/`; prepend module directories with the subsystem (e.g., `display_led/`).
- Add Unity test suites under `test/`; mirror the runtime module layout to keep fixtures discoverable.
- 起動シーケンス全体の流れは `doc/boot_sequence.md` に集約（更新時は同ドキュメントも必ず修正）。

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

## Commit & Pull Request Guidelines
- Git metadata is not initialized; establish history with imperative commit subjects (e.g., `feat: add imu driver`) and include scope tags.
- Reference linked issues in commit bodies or PR descriptions, describe hardware tested, and attach serial logs or photos when the change affects physical behavior.
- Keep PRs focused on one subsystem, outline validation steps, and request review from maintainers responsible for the touched hardware stack.

## Configuration & Environment Tips
- The project assumes an ESP32-S3 based M5Stack Atom S3R; verify you are on the same Arduino core version pinned in `platformio.ini` to avoid toolchain drift.
- When adding libraries, declare them in `lib_deps` and prefer versions compatible with PSRAM and FastLED to maintain stable builds.

## MQTT Topic Rules
詳細なトピック設計ガイドは `../doc/mqtt_rules.md` に記載されています。実装時は必ず参照し、`sphere/ui/#` などの階層構造や JSON ペイロード運用の方針に従ってください。

## Configuration Docs
`config.json` の構造と共通設定に関する規約は `../doc/define_config.md` に集約しています。Sphere / Joystick / RasPi 各モジュールで設定を追加・変更する際は、ドキュメントのルールと項目分類に従ってください。



## 移植
- /Users/katano/Documents/PlatformIO/Projects/MFT2025/CUBE-neon
- このプロジェクトをsphere_neonとして移植したい．
  - LED数：800LED
  - IMU: BMI270

- 諸機能は
  - /Users/katano/Documents/PlatformIO/Projects/MFT2025/isolation-sphere
  - ここにあるプロジェクトから流用できるものはしておきたい．

- 参考資料
- /Users/katano/Documents/PlatformIO/Projects/MFT2025/SPHERE_neon/doc
  - ここに別プログラムから参考になりそうなファイルを置くので活用すること
  






<!-- 


## CoreTask Architecture Separation
### ⚠️ 重要: joystick / sphere 間でのCoreTask完全分離

**isolation-sphere プロジェクトでは以下のファイルのみを使用:**
- `include/core/SphereCoreTask.h` → `SphereCore0Task` / `SphereCore1Task`
- `src/core/SphereCoreTask.cpp` → Sphere専用実装

**joystick プロジェクトでは以下のファイルのみを使用:**
- `include/core/JoystickCoreTasks.h` → `JoystickCore0Task` / `JoystickCore1Task` 
- `src/core/JoystickCoreTasks.cpp` → Joystick専用実装

**🚫 絶対に避けるべき混在:**
- isolation-sphere内にJoystickCoreTask系ファイルを配置
- joystick内にSphereCoreTask系ファイルを配置
- 共通のCoreTaskクラス名を使用（`Core0Task`/`Core1Task`は廃止）

**共通部分:**
- `include/core/CoreTask.h` （基底クラス）
- `include/core/SharedState.h`

この分離により、各プロジェクトが独立してCoreTask実装を進化させ、相互の変更による影響を防ぎます。

## Base Class & Implementation Separation Strategy

### 🏗️ 重要: 基底クラス・実体分離の設計方針

#### 原則: 共通インターフェース + プロジェクト固有実装

- プロジェクト間で機能が類似するが実装が異なるクラスは、基底クラス（抽象インターフェース）と具体実装に分離する
- 基底クラスは共通ヘッダーに配置し、具体実装は各プロジェクトに分離配置する
- この方針により、インターフェース統一性とプロジェクト独立性を両立する

### 分離対象候補クラス（優先度順）

#### Phase 1: UIMode分離 [優先度: 最高] 📱

```cpp
// 共通基底: include/ui/UIMode.h
class UIMode {
  virtual void handleLeftStick(int16_t x, int16_t y) = 0;
  virtual void handleRightStick(int16_t x, int16_t y) = 0;
  virtual void handleButtons() = 0;
  virtual String getModeName() = 0;
  virtual uint16_t getModeIcon() = 0;
};

// Sphere実装: include/ui/SphereUIMode.h
class SphereControlMode : public UIMode { /* 球体制御特化 */ };
class SphereVideoMode : public UIMode { /* 映像管理特化 */ };

// Joystick実装: include/ui/JoystickUIMode.h  
class JoystickIsolationSphereMode : public UIMode { /* MQTT送信特化 */ };
class JoystickVideoManagementMode : public UIMode { /* 動画選択特化 */ };
```

## Image → LED mapping guide

See `doc/image_to_led.md` for the JPEG/image → per-LED RGB mapping workflow, PSRAM/DMA considerations, and recommended class/module layout (LedLayout, ImageRenderer, ProceduralRenderer, Compositor, StripController). This is the canonical reference for implementing image playback and compositing on the sphere.

#### Phase 2: CommunicationService分離 [優先度: 高] 📡

```cpp
// 共通基底: include/communication/CommunicationService.h
class CommunicationService {
  virtual bool initialize(const ConfigManager::Config& config) = 0;
  virtual bool publishData(const std::string& topic, const std::string& data) = 0;
  virtual void loop() = 0;
};

// Sphere実装: include/communication/SphereCommunicationService.h
class SphereMqttService : public CommunicationService { /* MQTT Client特化 */ };

// Joystick実装: include/communication/JoystickCommunicationService.h
class JoystickMqttBrokerService : public CommunicationService { /* 内蔵ブローカー+UDP */ };
```

#### Phase 3: InputManager分離 [優先度: 高] 🎮

```cpp
// 共通基底: include/input/InputManager.h
class InputManager {
  virtual bool initialize() = 0;
  virtual bool readInput(InputState& state) = 0;
  virtual bool hasNewInput() const = 0;
};

// Sphere実装: include/input/SphereInputManager.h
class SphereImuInputManager : public InputManager { /* IMU+ジェスチャー検出 */ };

// Joystick実装: include/input/JoystickInputManager.h  
class JoystickAnalogInputManager : public InputManager { /* デュアルスティック特化 */ };
```

#### Phase 4: DisplayDriver拡張 [優先度: 中] 🖥️

```cpp
// 既存基底を拡張: include/display/DisplayDriver.h
// SphereDisplayDriver: LED制御統合
// JoystickDisplayDriver: LCD UI特化
```

### 実装ガイドライン

#### ファイル配置ルール

- **基底クラス**: `include/[category]/[BaseClassName].h` (両プロジェクト共通)
- **Sphere実装**: `include/[category]/Sphere[ClassName].h` + `src/[category]/Sphere[ClassName].cpp`
- **Joystick実装**: `include/[category]/Joystick[ClassName].h` + `src/[category]/Joystick[ClassName].cpp`

#### 設計原則

- 基底クラスは純粋仮想関数（`= 0`）のみ定義
- プロジェクト固有の実装詳細は具体クラスに隠蔽
- 依存注入パターンを活用してテスタビリティを維持
- 名前空間衝突を避けるため、明確なプレフィックスを使用

#### 移行戦略

1. **段階的移行**: 既存クラスを一度に変更せず、新機能から適用開始
2. **後方互換性**: 既存コードが動作し続けるよう、エイリアスや移行期間を設定
3. **テスト優先**: 新しいインターフェースのテストを先行作成
4. **ドキュメント更新**: 変更内容をAGENTS.md、README.mdに反映

#### 将来の共通ライブラリ化方針

**Phase 5: 共通ライブラリ化 [優先度: 将来] 📚**

基底クラス・実体分離パターンが確立された後、以下の構造への移行を検討：

```text
MFT2025/
├── lib/common/                   # 共通ライブラリ
│   ├── include/
│   │   ├── core/
│   │   │   ├── CoreTask.h        # 基底クラス
│   │   │   └── SharedState.h     # 共通状態
│   │   ├── ui/UIMode.h           # UI基底クラス
│   │   ├── communication/CommunicationService.h
│   │   └── input/InputManager.h
│   └── src/ (対応する実装)
├── isolation-sphere/
│   ├── platformio.ini: lib_deps += file://../lib/common
│   └── 具体実装クラスのみ
└── joystick/
    ├── platformio.ini: lib_deps += file://../lib/common  
    └── 具体実装クラスのみ
```

**移行条件:**

- 各プロジェクトで基底クラス・実体分離が安定動作
- 共通インターフェースのAPI仕様が固まる
- 両プロジェクトで十分なテストカバレッジを達成

**メリット:**

- 真の基底クラス統一管理
- バージョン管理の一元化
- 依存関係の明確化

この分離戦略により、プロジェクト間の独立性を保ちながら、共通インターフェースによる一貫性を実現します。 -->
