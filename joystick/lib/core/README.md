# CoreTask 共通ライブラリ

## 概要

`CoreTask`は、isolation-sphereとjoystick両プロジェクトで共通利用する抽象基底クラスです。

## 設計方針

- **基底クラス**: `CoreTask` - 純粋仮想関数による抽象基底クラス
- **具象実装**: 各プロジェクト固有のCoreTask実装
  - `SphereCoreTask` (isolation-sphere)
  - `JoystickCoreTask` (joystick)

## ファイル構成

```
# 基底クラス（共通）
include/core/CoreTask.h     # 抽象基底クラス定義
src/core/CoreTask.cpp       # 共通実装

# プロジェクト固有実装
include/core/JoystickCoreTask.h  # Joystick固有実装
src/core/JoystickCoreTask.cpp    # Joystick固有実装
```

## 使用方法

```cpp
#include "core/CoreTask.h"           // 基底クラス
#include "core/JoystickCoreTask.h"   // Joystick固有実装

// インスタンス作成
JoystickCore0Task core0(config, configManager, storageManager, sharedState);
JoystickCore1Task core1(config, sharedState);

// タスク開始
core0.start();
core1.start();
```

## 共通機能

- FreeRTOSタスク管理
- setup()/loop()抽象インターフェース  
- Hooksシステム（テスト用）
- タスク設定管理

## プロジェクト固有機能

### JoystickCoreTask
- Core0: WiFi AP, 内蔵MQTTブローカー, UDP通信
- Core1: ブザー制御, LCD表示, IMUジェスチャー

### SphereCoreTask (isolation-sphere)  
- Core0: LED制御, 球体描画, 外部MQTT接続
- Core1: IMU姿勢制御, UI処理, メディア再生