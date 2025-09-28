# 🎯 LED基盤システム設計・機能分解

## 📋 システム概要

LED基盤システムは、800個のWS2812 LED球体を統一的に制御する基幹システムです。ProceduralPatternGeneratorと画像テクスチャの両方から使用される共通基盤として設計します。

---

## 🔧 機能分解

### 1. LED物理配置管理 📍
**機能**: 800個LEDの物理位置・接続構成の管理
- **LED配置データ読み込み** (`led_layout.csv` 解析)
- **ストリップ構成管理** (4本×200LED構成)
- **LED ID↔物理位置マッピング** (FaceID, Strip, Strip_num)
- **3D座標正規化** (x,y,z ∈ [-1,1])

### 2. 球面座標変換システム 🌍
**機能**: 3D→球面座標→UV座標の数学的変換
- **カルテシアン→球面座標変換** (x,y,z → θ,φ)
- **クォータニオン姿勢変換** (IMU姿勢反映)
- **緯度・経度オフセット合成** (UI制御値反映)
- **UV座標正規化** (u,v ∈ [0,1])
- **高速数学演算** (近似arctan, sqrt最適化)

### 3. FastLED出力制御 ⚡
**機能**: WS2812ストリップへの高速データ出力
- **4ストリップ並列出力** (GPIO 5/6/7/8)
- **I2S DMA駆動** (30fps目標)
- **フレームバッファ管理** (CRGB[800]配列)
- **輝度・ガンマ補正** (表示品質最適化)
- **同期タイミング制御** (IMU更新と同期)

### 4. UV座標キャッシュシステム 🚀
**機能**: 座標変換結果の高速キャッシュ
- **姿勢変化検出** (quaternion差分監視)
- **キャッシュ無効化判定** (閾値ベース更新)
- **部分更新最適化** (変更LED のみ再計算)
- **メモリ効率管理** (PSRAM活用)

### 5. パフォーマンス監視 📊
**機能**: レンダリング性能の測定・最適化
- **フレームレート測定** (リアルタイム監視)
- **レンダリング時間計測** (ボトルネック検出)
- **メモリ使用量監視** (リソース管理)
- **統計情報出力** (デバッグ支援)

---

## 🏗️ アーキテクチャ構成

### レイヤー構成（下から上へ）

```
┌─────────────────────────────────────────────────────────┐
│          📱 アプリケーション層                               │
│  ┌─────────────────┐  ┌─────────────────────────────┐    │
│  │ ProceduralPattern│  │   ImageTexture              │    │
│  │   Generator     │  │    Renderer                 │    │
│  └─────────────────┘  └─────────────────────────────┘    │
└─────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────┐
│          🎯 LED基盤システム層                              │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────┐    │
│  │LED配置管理  │ │球面座標変換  │ │FastLED出力制御  │    │
│  │- CSV解析   │ │- 3D→UV変換 │ │- I2S DMA      │    │
│  │- ID管理    │ │- 姿勢変換   │ │- 4ストリップ   │    │
│  └─────────────┘ └─────────────┘ └─────────────────┘    │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────┐    │
│  │UVキャッシュ  │ │数学演算最適化│ │パフォーマンス監視│    │
│  │- 姿勢検出   │ │- 近似計算   │ │- FPS測定      │    │
│  │- 差分更新   │ │- SIMD最適化 │ │- 統計情報     │    │
│  └─────────────┘ └─────────────┘ └─────────────────┘    │
└─────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────┐
│          ⚙️ ハードウェア抽象層                              │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────┐    │
│  │  FastLED    │ │   M5Unified  │ │   LittleFS      │    │
│  │  Library    │ │     IMU      │ │   Storage       │    │
│  └─────────────┘ └─────────────┘ └─────────────────┘    │
└─────────────────────────────────────────────────────────┘
```

### 主要クラス構成

#### 1. **LEDSphereManager** (中核制御クラス)
```cpp
class LEDSphereManager {
    // 統合管理・外部インターフェース
    - LEDLayoutManager layoutManager_
    - SphereCoordinateTransform transformer_
    - FastLEDController ledController_ 
    - UVCoordinateCache uvCache_
    - PerformanceMonitor perfMonitor_
    
    public:
        bool initialize()
        void updateLEDs(const RenderParams& params)
        void setLED(uint16_t faceID, CRGB color)
        void clearAllLEDs()
        void show()
}
```

#### 2. **LEDLayoutManager** (配置データ管理)
```cpp
class LEDLayoutManager {
    // LED配置・構成管理
    private:
        std::vector<LEDPosition> positions_
        std::map<uint16_t, size_t> faceIdToIndex_
    
    public:
        bool loadFromCSV(const char* path)
        const LEDPosition& getPosition(uint16_t faceId)
        uint16_t findClosestLED(float u, float v)
        std::vector<uint16_t> findLEDsInRange(float u, float v, float radius)
}
```

#### 3. **SphereCoordinateTransform** (座標変換)
```cpp
class SphereCoordinateTransform {
    // 球面座標変換・姿勢制御
    private:
        float quaternion_[4]  // IMU姿勢
        float latOffset_, lonOffset_  // UI オフセット
        
    public:
        void setPosture(float qw, float qx, float qy, float qz)
        void setOffset(float lat, float lon)
        UVCoordinate transform3DToUV(float x, float y, float z)
        bool isTransformDirty()  // 変更検出
}
```

#### 4. **FastLEDController** (出力制御)
```cpp
class FastLEDController {
    // FastLED I2S DMA制御
    private:
        CRGB leds_[800]
        uint8_t brightness_
        bool isDirty_
        
    public:
        bool initializeI2S()
        void setLED(uint16_t index, CRGB color)
        void setBrightness(uint8_t brightness)  
        void show()  // DMA 出力実行
        void clear()
}
```

#### 5. **UVCoordinateCache** (高速キャッシュ)
```cpp
class UVCoordinateCache {
    // UV座標結果キャッシュ
    private:
        struct CachedUV { float u, v; bool dirty; }
        std::vector<CachedUV> cache_
        
    public:
        void updateAll(SphereCoordinateTransform& transform)
        void updateDirty(SphereCoordinateTransform& transform)  
        UVCoordinate getUV(uint16_t faceId)
        void invalidateAll()
}
```

#### 6. **PerformanceMonitor** (性能監視)
```cpp
class PerformanceMonitor {
    // パフォーマンス測定
    private:
        uint32_t frameCount_, lastFpsTime_
        float currentFps_, avgRenderTime_
        
    public:
        void frameStart()
        void frameEnd()
        float getCurrentFPS()
        void logStats()
}
```

---

## 🎨 ProceduralPatternGeneratorとの統合

### 統合インターフェース設計

```cpp
namespace LEDSphere {
    // ProceduralPatternGenerator から使用される統一インターフェース
    class SpherePatternInterface {
    public:
        // 高速パターン描画（スパース）
        void drawCoordinateAxis(const AxisConfig& config)
        void drawLatitudeRing(float latitude, CRGB color)
        void drawLongitudeLine(float longitude, CRGB color)
        void drawSparsePattern(const PatternPoints& points)
        
        // 姿勢制御
        void setIMUPosture(float qw, float qx, float qy, float qz)
        void setUIOffset(float latOffset, float lonOffset)
        
        // パフォーマンス制御
        void setSparseMode(bool enabled)  // 30fps vs 10fps 切替
        void setTargetFPS(uint8_t fps)
        
        // 状態取得
        uint16_t getActiveLEDCount()
        float getCurrentFPS()
    }
}
```

### 使用例：座標軸インジケータ実装

```cpp
// ProceduralPatternGenerator側
void CoordinateAxisPattern::render(const PatternParams& params) {
    // LED基盤システム取得
    auto* sphereInterface = LEDSphere::SpherePatternInterface::getInstance();
    
    // 姿勢更新
    sphereInterface->setIMUPosture(params.qw, params.qx, params.qy, params.qz);
    sphereInterface->setUIOffset(params.latOffset, params.lonOffset);
    
    // 高速描画モード設定（30fps）
    sphereInterface->setSparseMode(true);
    
    // 座標軸描画
    AxisConfig axisConfig;
    axisConfig.showLabels = showLabels_;
    axisConfig.showGrid = showGrid_;
    axisConfig.brightness = brightness_;
    
    sphereInterface->drawCoordinateAxis(axisConfig);
}
```

---

## ⚡ パフォーマンス最適化戦略

### 1. スパースレンダリング（30fps）
- **アクティブLEDのみ計算**: 全800LED → 3-200LED
- **UV計算スキップ**: キャッシュヒット率 >95%
- **部分フレーム更新**: 差分LEDのみ出力

### 2. フルレンダリング（10fps）
- **全LED品質重視**: バイリニア補間
- **キャッシュ最大活用**: UV座標完全キャッシュ
- **バッチ処理最適化**: 複数LED同時処理

### 3. 数学演算最適化
- **近似計算導入**: arctan2, sqrt の高速近似
- **SIMD活用**: ESP32-S3のベクトル演算
- **ルックアップテーブル**: 三角関数キャッシュ

### 4. メモリ最適化  
- **PSRAM活用**: 大容量キャッシュデータ
- **ガベージコレクション回避**: 固定サイズ配列
- **データ圧縮**: LED位置データ効率化

---

## 📊 期待パフォーマンス

| 動作モード | LED計算数 | UV変換 | 期待FPS | メモリ使用量 |
|------------|-----------|--------|---------|------------|
| 座標軸インジケータ | 3-15個 | キャッシュ | 30fps | ~10KB |
| プロシージャル標準 | 50-200個 | 部分更新 | 20-30fps | ~30KB |
| 画像テクスチャ | 800個 | 全更新 | 8-12fps | ~80KB |
| ハイブリッド | 800+α | 全更新+α | 8-10fps | ~90KB |

---

## 🔧 実装優先度

### Phase 1: 基盤システム構築
1. **LEDLayoutManager**: CSV読み込み・位置管理
2. **FastLEDController**: I2S DMA出力基盤
3. **SphereCoordinateTransform**: 基本座標変換

### Phase 2: 高速化システム
1. **UVCoordinateCache**: キャッシュシステム
2. **SpherePatternInterface**: ProceduralPattern統合
3. **座標軸インジケータ**: 実用的デバッグ機能

### Phase 3: 最適化・統合
1. **PerformanceMonitor**: 性能測定・チューニング
2. **数学演算最適化**: 近似計算導入
3. **ImageTextureRenderer**: 画像系統合

---

## 💡 設計の利点

✅ **統一インターフェース**: ProceduralとImage両方からの一貫したアクセス  
✅ **パフォーマンス柔軟性**: 30fps高速 ↔ 10fps高品質の切り替え  
✅ **拡張性**: 新パターン追加の容易さ  
✅ **保守性**: 共通処理の集約によるバグ減少  
✅ **テスト容易性**: レイヤー分離による単体テスト対応  

この設計により、ProceduralPatternGeneratorと画像系が共通のLED基盤システムを効率的に活用できる統合アーキテクチャが実現されます🎯