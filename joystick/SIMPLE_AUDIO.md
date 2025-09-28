# シンプル音量設定ガイド

M5Stack Atom Joystickの簡素化された音量設定について説明します。

## 🎵 簡素化された設定構造

### config.json設定 (シンプル版)
```json
{
  "joystick": {
    "audio": {
      "enabled": true,        // 音響機能のON/OFF
      "master_volume": 20,    // 全体音量 (0-100%)
      "sounds": {
        "startup": true,      // 起動音のON/OFF
        "click": true,        // クリック音のON/OFF
        "error": true,        // エラー音のON/OFF
        "test": true          // テスト音のON/OFF
      },
      "volumes": {
        "startup": 55,        // 起動音の音量 (0-100%)
        "click": 40,          // クリック音の音量 (0-100%)
        "error": 70,          // エラー音の音量 (0-100%)
        "test": 60            // テスト音の音量 (0-100%)
      }
    }
  }
}
```

## 🎛️ 音量計算方式

### 実効音量の算出
```
実効音量 = (マスター音量 × 音種別音量) ÷ 100
```

### 設定例
- **マスター音量**: 20%
- **クリック音量**: 40%

→ **実効クリック音量**: (20 × 40) ÷ 100 = **8%**

## 🔧 主な改善点

### ✅ Before (複雑)
```json
{
  "master_enabled": true,
  "master_volume": 75,
  "buzzer": {
    "enabled": true,
    "volume": 50,
    "test_sounds": {
      "enabled": true,
      "volume": 60
    },
    "feedback_sounds": {
      "click_enabled": true,
      "click_volume": 40,
      "error_enabled": true,
      "error_volume": 70
    },
    "startup_sounds": {
      "enabled": true,
      "volume": 55
    }
  }
}
```

### ✅ After (シンプル)
```json
{
  "enabled": true,
  "master_volume": 20,
  "sounds": {
    "startup": true,
    "click": true,
    "error": true,
    "test": true
  },
  "volumes": {
    "startup": 55,
    "click": 40,
    "error": 70,
    "test": 60
  }
}
```

## 📋 設定項目一覧

| 設定項目 | 型 | 範囲 | 説明 |
|---------|---|-----|-----|
| `enabled` | bool | true/false | 音響機能全体のON/OFF |
| `master_volume` | int | 0-100 | マスター音量（%） |
| `sounds.startup` | bool | true/false | 起動音の有効/無効 |
| `sounds.click` | bool | true/false | クリック音の有効/無効 |
| `sounds.error` | bool | true/false | エラー音の有効/無効 |
| `sounds.test` | bool | true/false | テスト音の有効/無効 |
| `volumes.startup` | int | 0-100 | 起動音音量（%） |
| `volumes.click` | int | 0-100 | クリック音音量（%） |
| `volumes.error` | int | 0-100 | エラー音音量（%） |
| `volumes.test` | int | 0-100 | テスト音音量（%） |

## 🎮 操作方法

M5ボタンを押すと6つのモードが順次実行されます：

### モード0-3: 各種音のテスト
- **モード0**: クリック音 (volumes.click使用)
- **モード1**: 音階テスト (volumes.test使用)
- **モード2**: 周波数スイープ (volumes.test使用)
- **モード3**: 接続音 (volumes.click使用)

### モード4-5: 全体音量調節
- **モード4**: 音量UP（25% → 50% → 75% → 100%）
- **モード5**: 音量DOWN（100% → 75% → 50% → 25%）

## 📊 推奨設定例

### 静音設定
```json
{
  "enabled": false
}
```

### 低音量設定
```json
{
  "enabled": true,
  "master_volume": 15,
  "volumes": {
    "startup": 30,
    "click": 20,
    "error": 50,
    "test": 25
  }
}
```

### クリック音のみ
```json
{
  "enabled": true,
  "master_volume": 30,
  "sounds": {
    "startup": false,
    "click": true,
    "error": false,
    "test": false
  },
  "volumes": {
    "click": 50
  }
}
```

### デバッグ用（高音量）
```json
{
  "enabled": true,
  "master_volume": 80,
  "volumes": {
    "startup": 70,
    "click": 60,
    "error": 90,
    "test": 75
  }
}
```

## 🚀 実装のメリット

1. **設定項目数**: 17項目 → **10項目** (約40%削減)
2. **ネスト深度**: 4階層 → **3階層**
3. **理解しやすさ**: 音の種類と音量が明確に分離
4. **保守性向上**: 音種の追加/削除が容易

---

**ボリューム項目が大幅にシンプルになり、管理しやすくなりました！** 🎵