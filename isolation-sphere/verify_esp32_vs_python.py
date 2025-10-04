#!/usr/bin/env python3
"""
ESP32とPythonでのLED色配列比較検証スクリプト

ESP32のcube_neon_demo_main.cppと同じロジックを実装し、
LEDの色配列が一致するか検証する
"""

import csv
import math

# ESP32と同じ定数
PANORAMA_WIDTH = 320
PANORAMA_HEIGHT = 160
NUM_LEDS = 800

# LED座標データを読み込み (scripts/led_layout.csv のフォーマット: FaceID,strip,strip_num,x,y,z)
led_coords = []
with open('scripts/led_layout.csv', 'r') as f:
    reader = csv.reader(f)
    header = next(reader)  # ヘッダーをスキップ
    for row in reader:
        # 期待される行長は6: FaceID, strip, strip_num, x, y, z
        if len(row) >= 6:
            led_id = int(row[0])
            strip = int(row[1])
            strip_idx = int(row[2])
            x = float(row[3])
            y = float(row[4])
            z = float(row[5])
            led_coords.append((led_id, strip, strip_idx, x, y, z))

print(f"LED座標読み込み完了: {len(led_coords)}個")

# 球面座標→UV変換 (ESP32と同じロジック)
def sphericalToUV_Standard(x, y, z):
    """ESP32のspericalToUV_Standard()と同じ実装"""
    # 経度: atan2(z, x)
    longitude = math.atan2(z, x)
    
    # 緯度: asin(y) (yは既に正規化済み)
    latitude = math.asin(max(-1.0, min(1.0, y)))
    
    # UV座標に変換 [0,1]
    u = (longitude + math.pi) / (2.0 * math.pi)
    v = (latitude + math.pi/2.0) / math.pi
    
    return u, v

# テストパノラマ初期化 (ESP32と同じロジック)
def initializeTestPanorama():
    """ESP32のinitializeTestPanorama()と同じ実装"""
    # 320x160x3のRGB配列を初期化 (黒)
    panorama = [[[0, 0, 0] for _ in range(PANORAMA_WIDTH)] for _ in range(PANORAMA_HEIGHT)]
    
    # 太いライン範囲 (ESP32と同じ)
    u25_start, u25_end = 74, 85   # u≈0.25
    u75_start, u75_end = 233, 245 # u≈0.75
    
    # u≈0.25領域: RGB(0, 255, 0) - 緑
    for py in range(PANORAMA_HEIGHT):
        for px in range(u25_start, u25_end + 1):
            panorama[py][px] = [0, 255, 0]
    
    # u≈0.75領域: RGB(255, 0, 0) - 赤
    for py in range(PANORAMA_HEIGHT):
        for px in range(u75_start, u75_end + 1):
            panorama[py][px] = [255, 0, 0]
    
    print(f"テストパノラマ初期化完了")
    print(f"  u25領域 (緑): X{u25_start}-X{u25_end} ({u25_end-u25_start+1}px幅)")
    print(f"  u75領域 (赤): X{u75_start}-X{u75_end} ({u75_end-u75_start+1}px幅)")
    
    return panorama

# LED色決定ロジック (ESP32と同じ)
def determineLedColors(panorama):
    """ESP32と同じロジックでLEDの色を決定"""
    led_colors = []
    
    for i, (led_id, strip, strip_idx, x, y, z) in enumerate(led_coords):
        # 球面座標→UV変換
        u, v = sphericalToUV_Standard(x, y, z)

        # ピクセル座標計算
        px = int(u * PANORAMA_WIDTH)
        py = int(v * PANORAMA_HEIGHT)

        # 範囲チェック
        px = max(0, min(px, PANORAMA_WIDTH - 1))
        py = max(0, min(py, PANORAMA_HEIGHT - 1))

        # パノラマから色を取得
        r, g, b = panorama[py][px]

        # LED情報を追加
        led_colors.append((led_id, strip, strip_idx, x, y, z, u, v, px, py, r, g, b))

        # デバッグ出力 (最初の20個のみ)
        if i < 20:
            print(f"🔵 LED[{led_id}]: xyz({x:.3f},{y:.3f},{z:.3f}) → UV({u:.3f},{v:.3f}) → px({px},{py}) → RGB({r},{g},{b})")
    
    return led_colors

# メイン実行
if __name__ == "__main__":
    print("=== ESP32 vs Python LED色配列検証 ===\n")
    
    # 1. テストパノラマを生成
    panorama = initializeTestPanorama()
    
    # 2. 各LEDの色を決定
    print("\nLED色決定中...")
    led_colors = determineLedColors(panorama)
    
    # 3. 結果をCSVファイルに出力 (統一フォーマット)
    output_filename = 'python_led_colors.csv'
    with open(output_filename, 'w', newline='') as f:
        writer = csv.writer(f)
        # ヘッダー書き込み (scripts/led_layout.csv の strip_num に合わせて 'strip_num' とする)
        writer.writerow(['LED_ID', 'strip', 'strip_num', 'x', 'y', 'z', 'u', 'v', 'px', 'py', 'r', 'g', 'b'])
        # データ書き込み
        writer.writerows(led_colors)

    print(f"\n検証結果を '{output_filename}' に出力しました。")

    # 4. 色ごとのLED数を集計 (インデックス: r=10,g=11,b=12)
    red_count = sum(1 for c in led_colors if c[10] > 0 and c[11] == 0 and c[12] == 0)
    green_count = sum(1 for c in led_colors if c[11] > 0 and c[10] == 0 and c[12] == 0)
    black_count = sum(1 for c in led_colors if c[10] == 0 and c[11] == 0 and c[12] == 0)
    other_count = len(led_colors) - red_count - green_count - black_count

    print("\n--- 色別LED数 ---")
    print(f"🔴 赤色 (Red):   {red_count}個")
    print(f"🟢 緑色 (Green): {green_count}個")
    print(f"⚫️ 黒色 (Black): {black_count}個")
    print(f"⚪️ その他:     {other_count}個")
    print("--------------------")

    print("\nExcelや他のツールでESP32の出力と比較してください。")
    black_count = sum(1 for c in led_colors if c[10] == 0 and c[11] == 0)

    print(f"色分布統計:")
    print(f"  赤色LED: {red_count}個")
    print(f"  緑色LED: {green_count}個")
    print(f"  黒色LED: {black_count}個")
    print(f"  合計: {len(led_colors)}個")
    print()
    # u≈0.25とu≈0.75領域の分析
    u25_leds = [(led_id, px, r, g, b) for (led_id, strip, strip_idx, x, y, z, u, v, px, py, r, g, b) in led_colors if 0.2 <= u <= 0.3]
    u75_leds = [(led_id, px, r, g, b) for (led_id, strip, strip_idx, x, y, z, u, v, px, py, r, g, b) in led_colors if 0.7 <= u <= 0.8]
    
    print(f"u≈0.25領域のLED ({len(u25_leds)}個):")
    for i, px, r, g, b in u25_leds[:10]:  # 最初の10個のみ表示
        print(f"  LED[{i}]: px={px}, RGB({r},{g},{b})")
    
    print(f"\nu≈0.75領域のLED ({len(u75_leds)}個):")
    for i, px, r, g, b in u75_leds[:10]:  # 最初の10個のみ表示
        print(f"  LED[{i}]: px={px}, RGB({r},{g},{b})")
    
    # (CSVは既に保存済み)
    print("\n=== 検証完了 ===")