# M5Stack Atom-JoyStick 単体テスト実行ガイド

## 概要

Phase 4.9・4.10で実装したオープニング演出・ブザーシステム・設定管理機能が実機で動作しない問題を解決するため、各機能を個別にテストする単体テストスケッチを作成しました。

## 🎯 テストの目的

1. **機能単位での問題切り分け**: 複合システムから個別機能を分離してテスト
2. **ハードウェア動作確認**: GPIO5ブザー・LCD表示・SPIFFS機能の確認
3. **ライブラリ統合確認**: TJpg_Decoder・M5Unified・ArduinoJsonの動作検証
4. **デバッグ情報収集**: シリアル出力によるエラー原因特定

## 📂 テストスケッチ構成

```
atom-joystick/test_sketches/
├── buzzer_unit_test/                    # ブザー機能単体テスト
│   └── buzzer_unit_test.ino            # GPIO5 PWMブザー・音色・音量テスト
├── opening_display_unit_test/           # オープニング表示単体テスト
│   ├── opening_display_unit_test.ino   # JPEG画像・SPIFFS・LCD表示テスト
│   └── data/images/                     # テスト用JPEGファイル
└── config_unit_test/                    # 設定管理単体テスト
    ├── config_unit_test.ino            # JSON設定・SPIFFS読み書きテスト
    └── data/config.json                 # テスト用設定ファイル
```

## 🚀 テスト実行手順

### 1. ブザー機能テスト (`buzzer_unit_test`)

**目的**: GPIO5 PWMブザー・音色システムの動作確認

**実行内容**:
- Phase 0: ブザー初期化テスト
- Phase 1: 基本ビープ音テスト 
- Phase 2: 起動音テスト
- Phase 3: WiFi接続音テスト
- Phase 4: エラー音テスト
- Phase 5: 完了音テスト
- Phase 6: オープニングメロディテスト
- Phase 7: 音量・有効無効テスト
- Phase 8: 統計表示テスト

**Arduino IDE操作**:
1. `buzzer_unit_test.ino`を開く
2. M5Stack Atom-JoyStickを選択
3. アップロード実行
4. シリアルモニタでログ確認（115200bps）
5. ボタンAでPhase切り替え可能

**期待結果**:
- 各Phaseで音が鳴る
- LCD表示で現在Phase確認可能
- シリアル出力で動作ログ確認

### 2. オープニング表示テスト (`opening_display_unit_test`)

**目的**: SPIFFS・JPEG表示・TJpg_Decoderの動作確認

**実行内容**:
- Phase 0: SPIFFS・表示システム初期化
- Phase 1: SPIFFS内容確認・ファイル一覧表示
- Phase 2: 個別JPEG画像テスト（flare-01.jpg）
- Phase 3: 個別JPEG画像テスト（flare-06.jpg）
- Phase 4: 完全オープニングシーケンス実行
- Phase 5: 統計情報表示・リセットテスト

**Arduino IDE操作**:
1. `opening_display_unit_test.ino`を開く
2. **重要**: Tools → ESP32 Sketch Data Uploadを実行（SPIFFSにデータアップロード）
3. M5Stack Atom-JoyStickを選択
4. アップロード実行
5. シリアルモニタでログ確認
6. ボタンAでPhase切り替え可能

**期待結果**:
- SPIFFS内ファイル一覧表示
- JPEG画像がLCDに表示される
- オープニングシーケンス（6フレーム連続表示）
- プログレスバー・完了メッセージ

### 3. 設定管理テスト (`config_unit_test`)

**目的**: config.json読み書き・設定管理システムの動作確認

**実行内容**:
- Phase 0: SPIFFS・設定システム初期化
- Phase 1: デフォルト設定生成・表示
- Phase 2: 設定ファイル読み込みテスト
- Phase 3: 設定変更・保存テスト
- Phase 4: 設定検証・エラーハンドリング
- Phase 5: 統計情報表示
- Phase 6: 設定リセット・復元テスト

**Arduino IDE操作**:
1. `config_unit_test.ino`を開く
2. **重要**: Tools → ESP32 Sketch Data Uploadを実行
3. M5Stack Atom-JoyStickを選択
4. アップロード実行
5. シリアルモニタでログ確認
6. ボタンAでPhase切り替え可能

**期待結果**:
- config.json読み込み・表示成功
- SSID・IP・音量等の設定変更成功
- 不正値のバリデーション動作
- デフォルト設定復元機能

## 📊 テスト結果評価基準

### ✅ 成功条件
- **ブザー**: 各Phaseで明確に異なる音が鳴る
- **オープニング**: JPEG画像がLCDに正しく表示される
- **設定管理**: JSON読み書き・設定変更が正常動作

### ❌ 失敗条件と対処
- **音が鳴らない** → GPIO5配線・PWM設定・ブザーハードウェア確認
- **画像が表示されない** → SPIFFS Upload・TJpg_Decoderライブラリ・LCD設定確認
- **設定読み込み失敗** → SPIFFS・ArduinoJsonライブラリ・ファイルパス確認

## 🔧 トラブルシューティング

### よくある問題と解決

#### 1. 「SPIFFS初期化失敗」エラー
**原因**: データアップロード未実行
**解決**: Arduino IDE → Tools → ESP32 Sketch Data Upload実行

#### 2. 「TJpg_Decoder not found」コンパイルエラー
**原因**: ライブラリ未インストール
**解決**: Arduino IDE → Library Manager → "TJpg_Decoder" by Bodmer インストール

#### 3. 音が全く鳴らない
**原因**: ブザー配線・GPIO設定問題
**解決**: 
- GPIO5ピン配線確認
- M5Stack Atom-JoyStick仕様書でピン配置確認
- PWM設定・周波数確認

#### 4. シリアルモニタに何も出力されない
**原因**: ボーレート設定・USB接続問題
**解決**: 
- シリアルモニタ115200bps設定確認
- USB-Cケーブル・接続確認
- デバイスポート選択確認

## 📈 テスト完了後の次のステップ

### 成功した場合
1. 動作する機能を確認してメインスケッチに統合
2. 問題の起きている統合部分を特定・修正
3. Phase 4.10システムの完全動作確認

### 失敗した場合
1. 失敗Phase・エラーメッセージを記録
2. ハードウェア・ライブラリ・設定を再確認
3. 段階的に問題を解決してから統合

## 💡 重要な注意点

1. **データアップロード必須**: SPIFFS使用テストでは必ずData Upload実行
2. **ライブラリ依存**: TJpg_Decoder・ArduinoJson・M5Unifiedが必要
3. **シリアル出力重要**: エラー原因は主にシリアル出力で確認
4. **段階的テスト**: 1つずつ機能を確認してから統合
5. **ハードウェア確認**: 音・表示が出ない場合は配線・設定確認

**このテスト結果により、Phase 4.9・4.10機能の問題箇所を特定し、確実に動作するシステムを完成させることができます。** 🎯