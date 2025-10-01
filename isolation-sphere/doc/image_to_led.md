
 #isolation-sphere #M5atomS3R #M5GFX #PSRAM #FastLED  

# 画像 → LED マッピング（JPEG → LED RGB）

このドキュメントは、JPEG（や他のラスタ画像）をどのように各 LED の RGB 値に変換するか、PSRAM と DMA が処理パイプラインでどのように関与するか、そして procedural（プログラム生成）パターンと画像レイヤを合成するための推奨クラス構成を示します。

## 1. 全体フロー

1. JPEG 画像を読み込むまたはストリーミングで受け取る。
2. JPEG をデコードしてピクセル（RGB888 または RGB565）を得る。デコード先は PSRAM を使ったフレームバッファでも、直接 LED バッファへストリーム書き込みしてもよい。
3. 各 LED に対して空間変換（`led_layout.csv` の座標 + 必要なら IMU による回転）を適用して画像上の UV 座標を算出する。
4. デコード済み画像から UV 座標をサンプリング（最近傍 / バイリニア）して色を取得する。
5. （任意）procedural パターンは同じ座標系で描画し、画像色と合成（アルファや加算）する。
6. 最終的な CRGB 値を `StripController` のバックバッファに書き、バッファを入れ替えてハードウェア出力（`FastLED.show()` や RMT/I2S ドライバ）をトリガする。

## 2. デコード戦略

- フルフレームを PSRAM にデコードする方法
  - JPEG を PSRAM 上のフレームバッファに完全デコードします（RGB888 推奨）。同じ画像を複数フレーム使い回す場合や、複数回のスケーリング／サンプリングが必要な場合に適しています。
  - メモリ: 幅 × 高さ × 3 バイト（RGB888）。ESP32 では `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` を使って PSRAM に確保します。

- ストリーミングデコード（直接 LED バッファへ書く）
  - ブロック単位のデコーダ（例: TJpg_Decoder）のコールバックを利用して、デコードされたブロックを直接 per-LED 一時バッファへ書き込みます。メモリ消費を抑えられますが、スケーリングや合成の扱いが複雑になります。

選択は画像解像度と利用可能な PSRAM 容量によります。本プロジェクトのターゲット（例: 320×160）では PSRAM を用いる方式を推奨します。

## 3. LED マッピング（座標 → UV 投影）

- `data/led_layout.csv` の (x,y,z) 座標を使います。事前に正規化ベクトルや球面座標（θ, φ）などを計算して保持しておくと高速です。
- ランタイムでは IMU の回転を 3D 座標へ行列掛けしてから UV に投影します。これにより IMU による球の回転が画像に反映されます。
- サンプリングは最近傍（nearest）またはバイリニア（bilinear）を選べます。バイリニアは滑らかですが 4 値の補間が必要です。

## 4. PSRAM と DMA の関係

- PSRAM（ESP32）
  - 大きなフレームバッファや複数バッファ（ダブルバッファ）は PSRAM に置くのが実用的です。`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` で確保し、NULL チェックしてください。
  - 断片化を避けるため、小さな割当を多数行うよりも可能な限り大きな連続確保を使う方が安全です。

- DMA / 周辺ハードウェアのオフロード
  - WS2812/NeoPixel 系: ESP32 では RMT（ハードウェアタイミング）を使うのが実質的なオフロード手段です。RMT は厳密には "DMA to LED" ではなくタイミング生成を周辺で行う仕組みで、FastLED の ESP32 バックエンドが RMT を使うことがあります。
  - APA102 のようにデータ+クロックが分かれているストリップは SPI DMA を利用でき、高速かつ CPU 負荷が少ない転送が可能です。

- パイプライン上の相互作用
  - 典型的な流れ: JPEG（PSRAM framebuffer）→ per-LED sampling → CRGB バックバッファ（PSRAM）→ swap → RMT/SPI で送出。
  - ボトルネックはサンプリング／合成の CPU 処理であることが多く、送出は RMT/SPI で可能な限りオフロードして CPU を開放します。

## 5. クラス／モジュール設計（推奨）

- `LedLayout`（データ）
  - 役割: `led_layout.csv` を読み、各 LED の `Coord3` を生成、投影用の UV を事前計算して提供する。

- `StripController`（現状スケルトン実装あり）
  - 役割: ストリップ単位のバッファ割当（PSRAM オプション）、ダブルバッファ、setPixel/fill/show、FastLED 用の front/back ポインタ公開。

- `ImageRenderer`
  - 役割: JPEG をファイルやストリームから読み込み、PSRAM フレームバッファへデコードするかストリーミングでレンダリングし、`renderTo(CRGB* out, const LedLayout&, Transform)` を提供する。

- `ProceduralRenderer`
  - 役割: パレットやノイズ、モーションなどの procedural 生成を行い、同一 API で `renderTo(CRGB* out, const LedLayout&, Transform)` を提供する。

- `Compositor`
  - 役割: 複数レイヤ（image / procedural / UI overlay）を受け取り、blend ルール（alpha/add）で合成し、`StripController::getRenderBuffer()` へ出力する。

- `DisplayOrchestrator` / `RendererTask`
  - 役割: FreeRTOS タスクでデコード／レンダリング／合成のスケジューリングを行い、バッファスワップと `StripController::show()` の呼出を管理、各処理時間を計測する。

## 6. API 契約（最小）

- ImageRenderer
  - `loadFromFile(const char* path) -> bool`
  - `renderTo(CRGB* out, const LedLayout& layout, const Transform& t)`

- ProceduralRenderer
  - `setPalette(...)`, `setParams(...)`
  - `renderTo(CRGB* out,...)`

- Compositor
  - `addLayer(Renderer*, blend_mode, alpha)`
  - `compose(CRGB* out)`

## 7. パフォーマンス & テストチェックリスト

- 計測: `micros()` で `FastLED.show()` と `renderTo()` をラップし、フレーム毎の時間を取得する。
- テスト: ストリーミングデコードと PSRAM フルデコードの時間差を比較する。
- エッジ: PSRAM 確保失敗時のフォールバック挙動を検証する。

## 8. 次の実装ステップ（計画）

1. `LedLayout` ローダー（CSV -> `vector<Coord3>`）を実装し、選んだ投影法で UV マップを事前計算する。
2. `TJpg_Decoder` を使った `ImageRenderer` を実装し、PSRAM フレームバッファへのフォールバック／ストリーミングの両方をサポートする。
3. シンプルな `ProceduralRenderer`（パレット/虹）と `Compositor` を実装して合成を確認する。
4. `StripController` に接続して `RendererTask` を作り、レンダリング時間を計測しながら反復する。

---
このドキュメントは LED マッピングとレンダリング実装のためのガイドとして作成されました。

