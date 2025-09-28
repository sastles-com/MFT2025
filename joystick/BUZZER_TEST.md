# パッシブブザー動作確認手順

M5Stack Atom JoyStickの内蔵パッシブブザー@5020の動作確認手順です。

## 🔊 重要な発見

**M5Stack Atom JoyStick (K137)** には**内蔵パッシブブザー@5020**が搭載されています！

- **公式仕様**: [M5Stack Documentation](https://docs.m5stack.com/en/app/Atom%20JoyStick)
- **ブザー仕様**: Built-in Passive Buzzer@5020
- **制御方法**: ESP32 PWM制御 (GPIO5経由)

## ⚡ クイックテスト

### 1. ファームウェア書き込み

```bash
cd /Users/katano/Documents/PlatformIO/Projects/MFT2025/joystick
pio run --target upload
```

### 2. シリアルモニター開始

```bash
pio device monitor -b 115200
```

### 3. 期待される動作

**初期化時の自動テスト**:
1. **音階テスト**: ドレミファソラシド (C4-C5)
2. **周波数スイープ**: 200Hz→2000Hz連続変化
3. **起動メロディ**: C-E-G-Cアルペジオ

**ボタンテスト**:
- M5ボタンを押すたびに4種類のテストモードが切り替わります

## 📋 動作確認チェックリスト

### ✅ 正常動作の場合

- [ ] 起動時にメロディが聞こえる
- [ ] シリアル出力に `[JoystickBuzzer] PWM initialized on GPIO5` 表示
- [ ] ボタン押下で異なる音が再生される
- [ ] 音階テストでドレミファソラシドが聞こえる

### ❌ 音が出ない場合の対処

1. **M5Speakerテスト確認**
   ```
   [Main] Testing M5 Speaker first...
   [M5SpeakerBuzzer] M5 Speaker not available on this device
   ```

2. **PWM初期化確認**
   ```
   [JoystickBuzzer] PWM initialized on GPIO5, channel 0
   ```

3. **音量設定確認**
   - `config.json` の `buzzer_volume` が 0 より大きいか確認
   - デフォルト: 50%

4. **ハードウェア確認**
   - M5Stack Atom JoystickのGPIO5が内蔵ブザーに接続されているか
   - 基板上の@5020ブザー部品の実装確認

## 🎵 テスト音の詳細

### 音階テスト (playScaleTest)
```
C4: 262Hz (ド)
D4: 294Hz (レ) 
E4: 330Hz (ミ)
F4: 349Hz (ファ)
G4: 392Hz (ソ)
A4: 440Hz (ラ)
B4: 494Hz (シ)
C5: 523Hz (ド)
```

### 周波数スイープ (playFrequencySweep)
```
200Hz → 300Hz → 400Hz → ... → 2000Hz
(100Hz刻みで連続変化)
```

### メロディサンプル (playStartupMelody)
```
C5 (523Hz) 200ms
E5 (659Hz) 200ms  
G5 (784Hz) 200ms
C6 (1047Hz) 300ms
```

## 🛠️ トラブルシューティング

### GPIO5が他の用途で使用されている場合

別のGPIOピンに変更する場合:

```cpp
// include/buzzer/JoystickBuzzer.h
static const int BUZZER_PIN = 26;  // GPIO26に変更例
```

### 外部パッシブブザー接続テスト

内蔵ブザーの動作確認ができない場合、外部パッシブブザーで検証:

```
ESP32 GPIO5 ----[+] パッシブブザー [-]---- GND
              (推奨: TMB12A05, MLT-5020等)
```

### デバッグ出力の活用

詳細なログを確認:
```cpp
Serial.printf("[JoystickBuzzer] Playing tone: %dHz, duty: %d, duration: %dms\n", 
              frequency, dutyCycle, duration);
```

## 📊 成功時のシリアル出力例

```
==============================
 MFT2025 Joystick (Composition)
==============================
[Main] LittleFS mounted
[Main] Config loaded successfully
[Main] WiFiManager initialized
[Main] Testing M5 Speaker first...
[M5SpeakerBuzzer] M5 Speaker not available on this device  
[Main] M5SpeakerBuzzer failed, trying GPIO5 PWM...
[JoystickBuzzer] PWM initialized on GPIO5, channel 0
[Main] JoystickBuzzer (GPIO5) initialized
[Main] Testing Passive Buzzer on GPIO5...
[JoystickBuzzer] Playing musical scale test (passive buzzer)
[JoystickBuzzer] Playing C4 (262Hz)
[JoystickBuzzer] Playing D4 (294Hz)
[JoystickBuzzer] Playing E4 (330Hz)
[JoystickBuzzer] Playing F4 (349Hz)
[JoystickBuzzer] Playing G4 (392Hz)  
[JoystickBuzzer] Playing A4 (440Hz)
[JoystickBuzzer] Playing B4 (494Hz)
[JoystickBuzzer] Playing C5 (523Hz)
[JoystickBuzzer] Playing frequency sweep test (passive buzzer)
[JoystickBuzzer] Playing tone: 200Hz, duty: 64, duration: 100ms
[JoystickBuzzer] Playing tone: 300Hz, duty: 64, duration: 100ms
...
[Main] Passive Buzzer test completed
```

---

この手順により、M5Stack Atom JoyStickの内蔵パッシブブザーの動作確認が可能です。