# M5## 🎯 UI設計思想とポリシー

**「階層ダイアル操作UI」**: モード→機能→値の3段階階層操作システム

### 📱 基本操作フロー

1. **LCDボタン**: モード選択（Live/Control/Video/Maintenance/System）
2. **左アナログスティック**: 選択されたモード内での機能選択
3. **右アナログスティック**: 選択された機能の値・パラメータ調整

### 🎮 操作ポリシー詳細

#### モード選択（LCDボタン）
- **トリガー**: 中央LCDボタン押し込み
- **動作**: 5つのモードを順次循環切替
- **表示**: ヘッダー部にモード名と色で現在モードを明示

#### 機能選択（左アナログスティック）
- **入力方式**: **方向のみ検出**（スティックの大きさ・距離は無視）
- **分割方式**: **機能数に応じた円周等分割**
  - 2択 → 180° × 2（左右）
  - 3択 → 120° × 3 
  - 4択 → 90° × 4（上下左右）
  - 6択 → 60° × 6
  - 8択 → 45° × 8（従来の8方向）
  - N択 → (360° / N) × N
- **視覚表現**: 選択可能機能を円周上に等間隔配置
- **自動整列**: 選択中機能を12時位置に回転表示

#### 値調整（右アナログスティック）  
- **入力方式**: **方向のみ検出**（スティックの大きさ・距離は無視）
- **調整方式**: **機能の値タイプに応じた制御**

##### パターン1: Boolean値（true/false）
```
円周を左右2分割
├ 左半円(270°-90°): false
└ 右半円(90°-270°): true
```

##### パターン2: 離散値選択（N択から1つ）
```  
円周をN等分割
例: 4択の場合
├ 12時方向(315°-45°): 選択肢1
├ 3時方向(45°-135°): 選択肢2  
├ 6時方向(135°-225°): 選択肢3
└ 9時方向(225°-315°): 選択肢4
```

##### パターン3: 連続値調整（アナログ値）
```
回転角度を連続値にマッピング
├ 時計回り回転: 値増加
├ 反時計回り回転: 値減少
├ 回転速度: 変化率制御
└ 360°で最小値→最大値の1周期
```

### 🎨 視覚フィードバック設計

#### 左スティック表示（機能選択）
- **外側リング**: 選択可能な全機能を等間隔配置
- **現在選択**: ハイライト色 + 12時位置自動整列
- **機能数表示**: リング上のドット数で選択肢数を明示

#### 右スティック表示（値調整）
- **内側リング**: 現在選択中機能の値調整UI
- **Boolean**: 左右2色分割（赤=false, 緑=true）
- **離散値**: N分割エリア + 現在値ハイライト
- **連続値**: 円形プログレスバー + 数値表示

#### 統合表示
```
┌─────────────────────────┐ 
│ [Control Mode]  [WiFi:8]│ ← モード名 + ステータス
├─────────────────────────┤
│    機能1  ○←選択中       │ ← 左スティック: 機能選択
│ 機能8 ○      ○ 機能2    │   (N等分割)
│       ○   ●   ○         │ ← 右スティック: 値調整
│ 機能7 ○      ○ 機能3    │   (タイプ別制御)
│    機能6  ○  機能4      │
│          機能5          │
│ 明度調整: ████▒▒ 75%   │ ← 現在の機能と値
└─────────────────────────┘
```

### ⚙️ 実装上の技術仕様

#### 角度計算
```cpp
// 左スティック: 機能選択
float angle = atan2(stick_y, stick_x);  // -π to π
int function_count = current_mode_functions.size();
int selected_function = (int)((angle + PI) / (2*PI) * function_count);

// 右スティック: 値調整  
float value_angle = atan2(stick_y, stick_x);
switch(current_function.value_type) {
    case BOOLEAN:
        bool value = (value_angle > -PI/2 && value_angle < PI/2); // 右半円
        break;
    case DISCRETE:
        int choice_count = current_function.choices.size();
        int selected = (int)((value_angle + PI) / (2*PI) * choice_count);
        break;
    case ANALOG:
        float normalized = (value_angle + PI) / (2*PI); // 0.0-1.0
        int value = min_val + (max_val - min_val) * normalized;
        break;
}
```

#### デッドゾーン処理
```cpp
#define DIRECTION_DEADZONE  0.3f  // 方向検出用デッドゾーン

bool isValidDirection(float x, float y) {
    return (sqrt(x*x + y*y) > DIRECTION_DEADZONE);
}
```

### 🔄 MQTT統合ポリシー

#### トピック設計
```cpp
// 機能選択時
sphere/ui/mode_changed: {"mode": "control", "timestamp": 1234567890}

// 機能内選択時  
sphere/ui/function_selected: {"mode": "control", "function": "brightness", "function_index": 0}

// 値変更時
sphere/ui/value_changed: {"mode": "control", "function": "brightness", "value": 185, "type": "analog"}
```om JoyStick デュアルダイアルUI仕様書

#joystick #ui #dualdial #esp32 #m5atoms3r

## � UI設計思想

**「デュアルダイアルUI」**: 左右アナログスティックを活用した直感的な操作UI
- **外ダイアル（左スティック）**: 機能選択 - 8方向でメニュー項目を選択
- **内ダイアル（右スティック）**: 値調整 - 連続値で細やかな数値調整
- **自動整列**: 選択項目を常に12時位置に配置して視認性向上
- **ホールド確定**: スティック押し込み1秒で設定確定
- **MQTT統合**: リアルタイム状態同期とsphere/ui/*トピック配信

## 🎮 操作モード定義

### 1. Live Mode（ライブ操作モード）

**用途**: リアルタイムisolation-sphere制御

```yaml
UI_MODE_LIVE: 0
テーマカラー: 橙色系（COLOR_LIVE_PRIMARY）
機能数: 8個（45°等分割）
```

**機能リスト**（左スティック8方向選択）:

1. **基本明度** - 連続値 (0-255) - 右スティック回転で調整
2. **色温度** - 連続値 (2700K-6500K) - 右スティック回転で調整
3. **回転速度** - 連続値 (0-100%) - 右スティック回転で調整
4. **エフェクト** - 4択選択 (None/Blur/Sharp/Glow) - 右スティック90°分割
5. **X軸制御** - 連続値 (-100～100) - 右スティック回転で調整
6. **Y軸制御** - 連続値 (-100～100) - 右スティック回転で調整  
7. **Z軸制御** - 連続値 (-100～100) - 右スティック回転で調整
8. **リセット** - Boolean (実行/キャンセル) - 右スティック左右分割

### 2. Control Mode（基本制御モード）

**用途**: 照明・表示の基本設定

```yaml
UI_MODE_CONTROL: 1  
テーマカラー: 青色系（COLOR_CONTROL_PRIMARY）
機能数: 8個（45°等分割）
```

**機能リスト**（左スティック8方向選択）:

1. **明度調整** - 連続値 (0-255) - 右スティック回転で調整
2. **色温度** - 連続値 (2700K-6500K) - 右スティック回転で調整
3. **RGB-Red** - 連続値 (0-255) - 右スティック回転で調整
4. **RGB-Green** - 連続値 (0-255) - 右スティック回転で調整
5. **RGB-Blue** - 連続値 (0-255) - 右スティック回転で調整
6. **更新間隔** - 6択選択 (10/20/50/100/250/500ms) - 右スティック60°分割
7. **自動調光** - Boolean (ON/OFF) - 右スティック左右分割
8. **省電力** - Boolean (ON/OFF) - 右スティック左右分割

### 3. Video Mode（動画制御モード）

**用途**: メディア再生・プレイリスト管理

```yaml
UI_MODE_VIDEO: 2
テーマカラー: 緑色系（COLOR_VIDEO_PRIMARY）
機能数: 8個（45°等分割）
```

**機能リスト**（左スティック8方向選択）:

1. **音量調整** - 連続値 (0-100%) - 右スティック回転で調整
2. **再生速度** - 8択選択 (0.25x/0.5x/0.75x/1.0x/1.25x/1.5x/1.75x/2.0x) - 右スティック45°分割
3. **動画ID** - 連続値 (1-999) - 右スティック回転で調整
4. **リピート** - 3択選択 (Off/One/All) - 右スティック120°分割
5. **シャッフル** - Boolean (ON/OFF) - 右スティック左右分割
6. **プレイリスト** - 連続値 (1-10) - 右スティック回転で調整
7. **エフェクト** - 4択選択 (None/Blur/Sharp/Vintage) - 右スティック90°分割
8. **再生制御** - 3択選択 (Stop/Play/Pause) - 右スティック120°分割

### 4. Maintenance Mode（保守調整モード）

**用途**: システム調整・キャリブレーション

```yaml
UI_MODE_MAINTENANCE: 3
テーマカラー: 黄色系（COLOR_MAINTAIN_PRIMARY）
機能数: 8個（45°等分割）
```

**機能リスト**（左スティック8方向選択）:

1. **LED校正** - 連続値 (0.8x-1.2x) - 右スティック回転で調整
2. **IMU補正** - 連続値 (-10°～10°) - 右スティック回転で調整
3. **タイムアウト** - 連続値 (1s-10s) - 右スティック回転で調整
4. **更新頻度** - 6択選択 (10/15/20/30/45/60Hz) - 右スティック60°分割
5. **ログレベル** - 4択選択 (Error/Warn/Info/Debug) - 右スティック90°分割
6. **診断実行** - Boolean (実行/キャンセル) - 右スティック左右分割
7. **バックアップ** - Boolean (作成/キャンセル) - 右スティック左右分割
8. **工場リセット** - Boolean (実行/キャンセル) - 右スティック左右分割

### 5. System Mode（システム監視モード）

**用途**: システム状態監視・統計表示

```yaml
UI_MODE_SYSTEM: 4
テーマカラー: マゼンタ系（COLOR_SYSTEM_PRIMARY）
機能数: 8個（45°等分割）
```

**監視項目**（左スティック8方向選択）:

1. **CPU使用率** - 表示のみ (0-100%) - 右スティックで詳細期間選択
2. **メモリ使用量** - 表示のみ (0-100%) - 右スティックで詳細期間選択
3. **温度監視** - 表示のみ (20°C-80°C) - 右スティックでアラート閾値調整
4. **バッテリー** - 表示のみ (0-100%) - 右スティックで省電力設定
5. **WiFi信号** - 表示のみ (-100dBm～-30dBm) - 右スティックでチャンネル選択
6. **MQTT接続** - 表示のみ (0-8) - 右スティックで接続制限調整
7. **エラー履歴** - ログ表示 - 右スティックでページング
8. **システム制御** - 3択選択 (再起動/シャットダウン/キャンセル) - 右スティック120°分割

## 🎨 視覚デザイン仕様

### 画面レイアウト（128x128px）

```text
┌─────────────────────────┐ 128x128px
│ [MODE NAME]     [STATUS]│ ← ヘッダー(28px)
├─────────────────────────┤ 
│        ○ ←12時位置      │
│    ○       ○            │ ← 外ダイアル
│ ○    ◉    ○           │   (選択項目)
│    ○   ●   ○            │ ← 内ダイアル  
│        ○               │   (値調整)
│                        │
│ [選択中項目: 明度 85%]    │ ← 下部ステータス
└─────────────────────────┘
```

### カラーパレット（RGB565）

```cpp
// 基本色
#define COLOR_BACKGROUND        0x0000  // 黒
#define COLOR_TEXT_PRIMARY      0xFFFF  // 白
#define COLOR_TEXT_SECONDARY    0xC618  // 薄い灰色

// モード別テーマカラー
#define COLOR_LIVE_PRIMARY      0xFD20  // 橙（新規）
#define COLOR_CONTROL_PRIMARY   0x001F  // 青
#define COLOR_VIDEO_PRIMARY     0x07E0  // 緑  
#define COLOR_MAINTAIN_PRIMARY  0xFFE0  // 黄
#define COLOR_SYSTEM_PRIMARY    0xF81F  // マゼンタ

// 操作状態色
#define COLOR_DIAL_NORMAL       0x4208  // 通常項目
#define COLOR_DIAL_ACTIVE       0x07FF  // 選択中項目  
#define COLOR_DIAL_SELECTED     0xFFE0  // 確定項目
#define COLOR_HOLD_PROGRESS     0xF800  // ホールド進行
```

## ⚙️ 技術仕様

### 入力処理

```cpp
// アナログスティック仕様
#define ANALOG_STICK_CENTER     2048.0f
#define ANALOG_STICK_MAX        2048.0f  
#define DEADZONE_THRESHOLD      0.15f     // 15%デッドゾーン
#define SENSITIVITY_MULTIPLIER  2.0f      // 感度倍率

// 確定システム
#define HOLD_CONFIRM_TIME_MS    1000     // 1秒ホールドで確定
```

### 描画パフォーマンス

```cpp  
// 描画設定
#define DIAL_UI_SCREEN_WIDTH    128
#define DIAL_UI_SCREEN_HEIGHT   128
#define OUTER_DIAL_RADIUS       45       // 外ダイアル半径
#define INNER_DIAL_RADIUS       25       // 内ダイアル半径

// フレームレート目標: 20-30fps
// 描画統計を UIDrawStats で追跡
```

## 🔄 状態管理・MQTT連携

### UI状態構造

```cpp
struct UIOperationState {
    uint8_t selected_item_index;     // 選択中項目(0-7)
    float left_stick_x, left_stick_y;   // 左スティック(-1.0～1.0)
    float right_stick_x, right_stick_y; // 右スティック(-1.0～1.0) 
    bool left_stick_pressed;         // 左スティック押し込み
    bool right_stick_pressed;        // 右スティック押し込み
    bool left_button_pressed;        // Lボタン
    bool right_button_pressed;       // Rボタン
    
    // ホールド確定システム
    unsigned long hold_start_time;   // ホールド開始時刻
    bool hold_in_progress;           // ホールド進行中
    bool hold_confirmed;             // ホールド確定完了
    
    // 回転制御（自動整列用）
    float outer_dial_rotation;       // 外ダイアル回転角度
    float inner_dial_rotation;       // 内ダイアル回転角度
    float target_rotation;           // 12時位置整列目標角度
};
```

### MQTT統合パターン

```cpp
// UI→MQTTコールバック設定
dual_dial_ui.setValueChangeCallback([](const char* topic, int value) {
    if (strcmp(topic, "control/brightness") == 0) {
        mqtt_manager.publishBrightness(value);
    } else if (strcmp(topic, "control/color_temp") == 0) {
        mqtt_manager.publishColorTemp(value);  
    } else if (strcmp(topic, "video/volume") == 0) {
        mqtt_manager.publishVolume(value);
    }
    // sphere/ui/* トピックでリアルタイム配信
});

// 確定時コールバック  
dual_dial_ui.setConfirmCallback([](uint8_t item_index, int value) {
    buzzer.button_click();  // 確定音再生
    // 設定の永続化処理
});
```

## 🚀 実装優先度（階層ダイアル操作システム）

### Phase 1: 基本階層操作システム

- [ ] **LCDボタンモード切替**: 5モード循環システム
- [ ] **方向のみスティック入力**: 距離無視の角度検出システム
- [ ] **動的分割システム**: 機能数に応じた円周N等分割
- [ ] **UIOperationState拡張**: 階層状態管理（モード→機能→値）

### Phase 2: 値タイプ別制御システム

- [ ] **Boolean制御**: 左右2分割での true/false 選択
- [ ] **離散値制御**: N択での円周等分割選択システム
- [ ] **連続値制御**: 回転角度→数値マッピングシステム
- [ ] **値タイプ自動判別**: 機能定義に基づく制御方式切替

### Phase 3: 視覚フィードバックシステム

- [ ] **機能選択リング**: 外側リングでのN等分割表示
- [ ] **値調整リング**: 内側リングでのタイプ別表示（Boolean/離散/連続）
- [ ] **12時位置整列**: 選択機能の自動回転表示
- [ ] **進行状況表示**: 連続値での円形プログレスバー

### Phase 4: MQTT統合・高度化

- [ ] **階層状態トピック**: mode→function→value の3段階配信
- [ ] **リアルタイム同期**: 各階層での状態変更即座配信
- [ ] **設定永続化**: モード別機能設定の保存・復元
- [ ] **パフォーマンス最適化**: 60fps描画での滑らか操作感

### 🔧 実装上の重要ポイント

#### 角度計算の標準化
```cpp
// 統一角度計算関数
float calculateNormalizedAngle(float stick_x, float stick_y) {
    float angle = atan2(stick_y, stick_x);           // -π to π
    return (angle + PI) / (2*PI);                    // 0.0 to 1.0
}

// N分割選択
int selectFromN(float normalized_angle, int choice_count) {
    return (int)(normalized_angle * choice_count) % choice_count;
}
```

#### 値タイプ定義
```cpp
enum ValueType {
    BOOLEAN,    // true/false
    DISCRETE,   // N択選択
    ANALOG      // 連続値
};

struct FunctionConfig {
    const char* name;
    ValueType type;
    union {
        struct { bool default_val; } boolean;
        struct { const char** choices; int count; int default_idx; } discrete;
        struct { int min_val; int max_val; int default_val; } analog;
    };
};
```

## 📊 モード・機能構成の検討

### 🎯 現在の仕様サマリー

#### モード数: **5モード**
1. **Live Mode** (橙色) - リアルタイム制御
2. **Control Mode** (青色) - 基本設定
3. **Video Mode** (緑色) - メディア管理  
4. **Maintenance Mode** (黄色) - 保守調整
5. **System Mode** (マゼンタ) - 監視・統計

#### 各モードの機能数: **8機能固定**

**全モード共通**: 左スティック8方向（45°等分割）での機能選択

### 🤔 検討事項・課題

#### 1. 機能数の最適化検討
```yaml
現在: 全モード8機能固定（45°等分割）

検討案:
- Live Mode: 6機能 (60°分割) - よく使う機能に集約
- Control Mode: 8機能 (45°分割) - 現状維持
- Video Mode: 6機能 (60°分割) - 重複機能整理
- Maintenance Mode: 8機能 (45°分割) - 現状維持
- System Mode: 4機能 (90°分割) - 監視項目をグループ化
```

#### 2. 機能の重複・整理
**重複している機能**:
- 明度調整: Live Mode + Control Mode
- 色温度: Live Mode + Control Mode
- エフェクト: Live Mode + Video Mode

**整理案**:
- **Live Mode**: 即座制御（明度/色温度/回転/エフェクト）
- **Control Mode**: 詳細設定（RGB個別/更新間隔/自動調光）

#### 3. 機能の優先度・使用頻度
**高頻度機能** (Live/Controlに配置):
- 明度調整
- 色温度調整
- 音量調整
- 再生制御

**中頻度機能** (各専門モード):
- RGB個別調整
- プレイリスト選択
- 診断実行

**低頻度機能** (Maintenance/System):
- 校正値調整
- ログレベル変更
- 工場リセット

#### 4. 値タイプ分布の検討
```yaml
現在の分布:
- 連続値(Analog): 21個 (52.5%)
- 離散値(Discrete): 12個 (30.0%)  
- Boolean: 7個 (17.5%)

最適化案:
- 連続値: よく調整する数値（明度/音量/速度）
- 離散値: 選択肢が明確（エフェクト/ログレベル）
- Boolean: ON/OFF切替（自動調光/シャッフル）
```

### 💡 改善提案

#### 提案1: モード機能数の動的調整
```yaml
Live Mode: 6機能 (60°分割)
├ 基本明度 (連続値)
├ 色温度 (連続値)  
├ 音量 (連続値)
├ 再生制御 (3択: Stop/Play/Pause)
├ エフェクト (4択)
└ 全体リセット (Boolean)

Control Mode: 8機能 (45°分割) - 現状維持

Video Mode: 6機能 (60°分割)
├ 音量調整 (連続値)
├ 再生速度 (8択)
├ 動画ID (連続値) 
├ プレイリスト (連続値)
├ リピート (3択)
└ シャッフル (Boolean)

Maintenance Mode: 6機能 (60°分割)  
├ LED校正 (連続値)
├ IMU補正 (連続値)
├ 更新頻度 (6択)
├ ログレベル (4択)
├ 診断実行 (Boolean)
└ 工場リセット (Boolean)

System Mode: 4機能 (90°分割)
├ システム状態 (表示グループ: CPU/メモリ/温度)
├ 通信状態 (表示グループ: WiFi/MQTT/バッテリー)  
├ エラー履歴 (ログ表示)
└ システム制御 (3択: 再起動/シャットダウン/キャンセル)
```

#### 提案2: 階層化による機能拡張
```yaml
基本モード (LCDボタン1回): Live/Control/Video (3モード)
拡張モード (LCDボタン長押し): Maintenance/System (2モード)

利点:
- 日常操作は3モードに集約
- 高度機能は長押しでアクセス
- 誤操作防止
```

### 🔧 実装上の検討点

#### 動的分割システムの実装
```cpp
// モード別機能数定義
const int MODE_FUNCTION_COUNTS[] = {6, 8, 6, 6, 4}; // Live,Control,Video,Maintenance,System

// 動的角度計算
float getAngleStep(int mode) {
    return 360.0f / MODE_FUNCTION_COUNTS[mode];
}
```

#### 機能定義構造の拡張
```cpp
struct ModeConfig {
    const char* name;
    uint16_t theme_color;
    int function_count;
    FunctionConfig functions[MAX_FUNCTIONS];
};
```

---

**参考実装**: `/Users/katano/Documents/home/isolation_sphere/sastle/atom-joystick/test_sketches/14_udp_joystick_integration/`  
**最終更新**: 2025年9月28日


