# SPHERE_neon プロジェクト仕様書

## 概要
CUBE-neonプロジェクトをベースに、球体形状（800LED）での光る電飾球体制御システムを開発する。M5Stack AtomS3RとBMI270 IMUを使用し、姿勢制御によるインタラクティブな演出を実現する。

## ハードウェア仕様

### メイン基板
- **MCU**: M5Stack AtomS3R (ESP32-S3)
- **IMU**: BMI270 (6軸センサー)
- **Flash**: PSRAMサポート付き
- **通信**: Wi-Fi, Bluetooth

### LED仕様
- **LED数**: 800個 (WS2812B互換)
- **配置**: 球体表面への3D配列
- **制御**: GPIO 46ピン経由
- **色空間**: RGB (24bit)
- **フレームレート**: 最大100fps

### 座標系
- **座標データ**: float型 3D座標 (x, y, z)
- **Face/Strip構造**: FaceID, strip, strip_num による階層管理
- **レイアウトファイル**: `include/layout_new.h` (800要素)

## 機能要件

### コア機能

#### 1. LED制御システム
- **基本制御**: 個別LED色指定、一括更新
- **パターン表示**: プロシージャルパターン生成
- **画像マッピング**: JPEG → LED座標変換
- **エフェクト**: フェード、パルス、回転効果

#### 2. IMU姿勢制御
- **姿勢検出**: BMI270による6軸データ取得
- **クォータニオン**: 姿勢表現と座標変換
- **ジェスチャー**: シェイク、傾き検出
- **補正**: センサーキャリブレーション

#### 3. 通信インターフェース
- **MQTT**: リモート制御プロトコル
- **HTTP**: 設定・制御API
- **Bluetooth**: モバイルアプリ連携（将来）

### 表示モード

#### 1. プロシージャルモード
- **幾何学パターン**: 球面調和関数、フラクタル
- **物理シミュレーション**: 波動、粒子システム
- **姿勢連動**: IMUデータによるリアルタイム変化

#### 2. 画像表示モード
- **静的画像**: JPEG/PNG → 球面投影
- **動的コンテンツ**: アニメーション再生
- **リアルタイム**: ライブ映像ストリーミング

#### 3. インタラクションモード
- **ジェスチャー制御**: 振り・傾きによる操作
- **環境応答**: 音・光センサー連動
- **ネットワーク制御**: MQTT/HTTP API

## アーキテクチャ設計

### クラス構成

#### LED制御層
```cpp
class SphereStripController {
    // FastLED代替の低レベルLED制御
    void updateStrip(int stripId, CRGB* colors);
    void show();
};

class LedSphereManager {
    // 球体レイアウト管理、座標 → LED変換
    bool setPixelColor(float x, float y, float z, CRGB color);
    void clear();
    void update();
};

class ProceduralRenderer {
    // プロシージャルパターン生成
    void renderPattern(PatternType type, float time);
};
```

#### IMU制御層
```cpp
class SphereIMUManager {
    // BMI270制御、クォータニオン計算
    bool initialize();
    Quaternion getOrientation();
    bool detectShake();
};
```

#### 通信層
```cpp
class SphereCommunicationService {
    // MQTT/HTTP通信管理
    bool publishTelemetry(const String& data);
    void handleRemoteCommand(const String& command);
};
```

### タスク分散 (ESP32 デュアルコア)

#### Core 0 (Protocol CPU)
- Wi-Fi/MQTT通信処理
- HTTP API サーバー
- ファイルシステム操作

#### Core 1 (Application CPU)
- LED描画・更新 (高優先度)
- IMU データ処理
- パターン生成・エフェクト計算

## パフォーマンス目標

### リアルタイム性能
- **LED更新**: 100fps (10ms間隔)
- **IMU采样**: 200Hz
- **パターン生成**: Core1で60fps以上
- **MQTT遅延**: <50ms

### メモリ使用量
- **LED バッファ**: 800 × 3byte = 2.4KB × 2 (ダブルバッファ)
- **PSRAM活用**: 画像データ、パターンキャッシュ
- **スタック**: Core毎に十分な容量確保

## 開発フェーズ

### Phase 1: 基盤構築
- [ ] FastLED代替LED制御システム
- [ ] BMI270 IMU基本動作
- [ ] 座標系・レイアウト検証

### Phase 2: コア機能
- [ ] プロシージャルパターン生成
- [ ] IMU姿勢制御連動
- [ ] MQTT通信インターフェース

### Phase 3: 高度機能
- [ ] 画像→球面マッピング
- [ ] リアルタイムストリーミング
- [ ] モバイルアプリ連携

## 制約・考慮事項

### 技術的制約
- **FastLED互換性**: C++17テンプレートエラーによる代替実装必要
- **PSRAM制限**: 大容量データの効率的な管理
- **発熱**: 高輝度・高フレームレート時の温度管理

### 運用制約
- **消費電力**: LED800個フル点灯時の電力設計
- **安全性**: 過電流保護、温度監視
- **保守性**: リモート診断・アップデート機能

## 互換性・移植性

### CUBE-neonからの移植
- **LED制御ロジック**: 立方体 → 球体への座標変換
- **パターンライブラリ**: 既存エフェクトの球面対応
- **通信プロトコル**: MQTTトピック構造の共通化

### 将来展開
- **マルチデバイス**: 複数球体の同期制御
- **プラットフォーム拡張**: 他のESP32ボードへの対応
- **アプリ連携**: iOS/Android専用アプリ開発