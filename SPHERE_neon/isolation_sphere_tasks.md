# SPHERE_neon 開発タスクリスト

## 現在の状況

### ✅ 完了済み
- M5Unified基本動作確認
- BMI270 IMU基本読み取り  
- LED座標データ変換（CSV → float配列）
- PlatformIO環境構築（atoms3r_bmi270）

### ⚠️ 一時的課題
- FastLED: C++17互換性エラーで無効化
- DabbleESP32: ESP32フレームワークAPI変更で無効化

## Phase 1: 基盤構築 [優先度: 最高]

### 1.1 LED制御システム代替実装 [CRITICAL]
- [ ] **`SphereStripController`クラス作成**
  - ESP32のRMT/SPI経由でWS2812B制御
  - FastLED互換API提供
  - テスト: 単色点灯、レインボーパターン

- [ ] **`LedSphereManager`クラス作成**  
  - float座標 → LED index変換
  - 800LED管理・バッファリング
  - テスト: 座標指定点灯、範囲指定

- [ ] **レイアウト検証**
  - `layout_new.h`の座標精度確認
  - Face/Strip構造の妥当性検証
  - テスト: 各Face個別点灯

### 1.2 IMU姿勢制御強化 [HIGH]
- [ ] **`SphereIMUManager`クラス作成**
  - BMI270設定最適化（サンプリング周波数、フィルタ）
  - クォータニオン計算実装
  - テスト: 姿勢データ精度、応答性

- [ ] **ジェスチャー検出**
  - シェイク検出アルゴリズム
  - 傾き・回転検出
  - テスト: 各ジェスチャー認識率

### 1.3 基本通信インターフェース [MEDIUM]
- [ ] **`SphereCommunicationService`クラス作成**
  - MQTT client基本実装
  - Wi-Fi接続管理
  - テスト: メッセージ送受信

## Phase 2: コア機能実装 [優先度: 高]

### 2.1 プロシージャルパターン生成 [HIGH]
- [ ] **`ProceduralRenderer`クラス作成**
  - 基本幾何学パターン（波、スパイラル）
  - 時間ベースアニメーション
  - テスト: パターン精度、フレームレート

- [ ] **IMU連動エフェクト**
  - 姿勢に応じたパターン回転
  - ジェスチャーによるエフェクト変更
  - テスト: リアルタイム応答性

### 2.2 設定管理システム [MEDIUM]
- [ ] **`ConfigManager`クラス作成**
  - JSON設定ファイル読み込み
  - Wi-Fi/MQTT設定管理
  - テスト: 設定保存・復元

### 2.3 デュアルコアタスク分散 [MEDIUM]
- [ ] **`SphereCore0Task`/`SphereCore1Task`**
  - Core0: 通信処理
  - Core1: LED描画・IMU処理
  - テスト: 負荷分散、同期性能

## Phase 3: 高度機能 [優先度: 中]

### 3.1 画像マッピング機能 [MEDIUM]
- [ ] **`ImageRenderer`クラス作成**
  - JPEG → 球面投影変換
  - PSRAM活用による大容量画像処理
  - テスト: 画像品質、メモリ使用量

### 3.2 リアルタイムストリーミング [LOW]
- [ ] **リアルタイム映像処理**
  - UDP/TCP映像ストリーミング受信
  - フレームバッファ管理
  - テスト: ストリーミング遅延

### 3.3 モバイルアプリ連携 [LOW]
- [ ] **Bluetooth LE実装**
  - スマートフォンアプリとの通信
  - カスタムプロトコル設計
  - テスト: 接続安定性

## テスト戦略 [TDD適用]

### 単体テスト
- [ ] **`test_sphere_strip_controller.cpp`**
  - LED制御基本動作
  - 色指定・更新タイミング

- [ ] **`test_led_sphere_manager.cpp`** 
  - 座標変換精度
  - レイアウト境界条件

- [ ] **`test_sphere_imu_manager.cpp`**
  - IMUデータ精度
  - ジェスチャー検出率

### 統合テスト  
- [ ] **IMU ↔ LED連動テスト**
- [ ] **通信 ↔ 表示連動テスト**
- [ ] **パフォーマンステスト**（フレームレート、メモリ使用量）

## 優先タスク（今週実装対象）

### 🚀 最優先
1. **SphereStripController** - FastLED代替LED制御
2. **LedSphereManager** - 座標→LED変換
3. **基本テスト** - 単色・パターン点灯確認

### 📋 次優先  
4. **SphereIMUManager** - BMI270制御強化
5. **ProceduralRenderer** - 基本パターン生成
6. **単体テスト** - TDDベースの検証

## 実装方針

### クラス設計原則
- **基底クラス分離**: 共通インターフェース + SPHERE特化実装
- **依存注入**: テスト容易性とモジュール独立性
- **RAII**: リソース管理の自動化

### ファイル配置
```
include/
├── led/
│   ├── SphereStripController.h
│   └── LedSphereManager.h  
├── imu/
│   └── SphereIMUManager.h
├── communication/
│   └── SphereCommunicationService.h
└── renderer/
    └── ProceduralRenderer.h

src/
├── led/
├── imu/  
├── communication/
└── renderer/

test/
├── test_sphere_strip_controller.cpp
├── test_led_sphere_manager.cpp
└── test_sphere_imu_manager.cpp
```

### ビルド・テスト手順
1. **テスト先行作成** - 期待動作の明確化
2. **最小実装** - テストパス最小限コード
3. **リファクタリング** - コード品質向上
4. **パフォーマンス最適化** - 実機での性能調整