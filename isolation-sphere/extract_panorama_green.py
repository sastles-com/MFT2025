#!/usr/bin/env python3
"""
initializeTestPanorama()で作成されたパノラマ画像のGreen値を320x160のCSV形式で出力

テストパノラマ構成:
- 横320ピクセル × 縦160ピクセル
- u=0.25 (x=80) に緑ライン: RGB(0, 255, 0)  → G=255
- u=0.75 (x=240) に赤ライン: RGB(255, 0, 0) → G=0
- その他の領域: RGB(0, 0, 0) → G=0
"""

import csv

def create_test_panorama_green_csv():
    """
    initializeTestPanorama()と同じロジックでパノラマのGreen値配列を生成してCSV出力
    """
    PANORAMA_WIDTH = 320
    PANORAMA_HEIGHT = 160
    
    # パノラマ配列初期化（RGB全て0）
    panorama_rgb = [[[0, 0, 0] for _ in range(PANORAMA_WIDTH)] for _ in range(PANORAMA_HEIGHT)]
    
    # テストライン設定（initializeTestPanorama()と同じ）
    u25_x = int(0.25 * (PANORAMA_WIDTH - 1))  # u=0.25 → x=79 (0-indexed: x=80 in 1-indexed)
    u75_x = int(0.75 * (PANORAMA_WIDTH - 1))  # u=0.75 → x=239 (0-indexed: x=240 in 1-indexed)
    
    print(f"📐 パノラマサイズ: {PANORAMA_WIDTH}x{PANORAMA_HEIGHT}")
    print(f"🟢 u=0.25ライン位置: x={u25_x} (緑ライン RGB(0,255,0))")
    print(f"🔴 u=0.75ライン位置: x={u75_x} (赤ライン RGB(255,0,0))")
    
    # 縦ライン設定
    for y in range(PANORAMA_HEIGHT):
        # u=0.25 緑ライン
        panorama_rgb[y][u25_x] = [0, 255, 0]  # Green line
        # u=0.75 赤ライン  
        panorama_rgb[y][u75_x] = [255, 0, 0]  # Red line
    
    # Green値のみを抽出して2D配列を作成
    green_values = []
    for y in range(PANORAMA_HEIGHT):
        row = []
        for x in range(PANORAMA_WIDTH):
            green_val = panorama_rgb[y][x][1]  # G値 (RGB[1])
            row.append(green_val)
        green_values.append(row)
    
    return green_values, u25_x, u75_x

def export_green_csv():
    """
    パノラマのGreen値を320x160 CSVファイルとして出力
    """
    green_values, u25_x, u75_x = create_test_panorama_green_csv()
    
    output_filename = 'panorama_green_320x160.csv'
    
    with open(output_filename, 'w', newline='', encoding='utf-8') as csvfile:
        writer = csv.writer(csvfile)
        
        # ヘッダー行（列番号: 0-319）
        header = [f'X{x:03d}' for x in range(320)]
        writer.writerow(header)
        
        # データ行（Y座標と各X位置のGreen値）
        for y, row in enumerate(green_values):
            data_row = [f'Y{y:03d}'] + row
            writer.writerow(data_row)
    
    print(f"\n✅ CSV出力完了: {output_filename}")
    print(f"📁 出力パス: /Users/katano/Documents/PlatformIO/Projects/MFT2025/isolation-sphere/{output_filename}")
    
    # 統計情報
    total_pixels = 320 * 160
    green_pixels = sum(row.count(255) for row in green_values)  # G=255のピクセル数
    black_pixels = sum(row.count(0) for row in green_values)    # G=0のピクセル数
    
    print(f"\n📊 パノラマ統計:")
    print(f"総ピクセル数: {total_pixels}")
    print(f"緑ピクセル(G=255): {green_pixels} ({green_pixels/total_pixels*100:.1f}%)")
    print(f"黒ピクセル(G=0): {black_pixels} ({black_pixels/total_pixels*100:.1f}%)")
    print(f"緑ライン位置: x={u25_x}, x={u75_x}")
    
    return output_filename

def preview_green_array():
    """
    Green配列の一部をプレビュー表示
    """
    green_values, u25_x, u75_x = create_test_panorama_green_csv()
    
    print(f"\n🔍 Green配列プレビュー（y=80付近、x={u25_x-5}～{u75_x+5}）:")
    print("Y\\X  ", end="")
    for x in range(max(0, u25_x-5), min(320, u75_x+6)):
        print(f"{x:3d}", end=" ")
    print()
    
    for y in range(78, 83):  # y=78-82 (160の中央付近)
        if y < len(green_values):
            print(f"{y:3d}: ", end="")
            for x in range(max(0, u25_x-5), min(320, u75_x+6)):
                if x < len(green_values[y]):
                    val = green_values[y][x]
                    if val == 255:
                        print(f"\033[92m{val:3d}\033[0m", end=" ")  # 緑色表示
                    else:
                        print(f"{val:3d}", end=" ")
            print()

if __name__ == "__main__":
    print("🖼️  テストパノラマのGreen値配列をCSV出力")
    print("=" * 60)
    
    # Green値配列をプレビュー
    preview_green_array()
    
    # CSV出力実行
    output_file = export_green_csv()
    
    print(f"\n🎯 CSVファイルの使用方法:")
    print(f"1. Excelで開く: {output_file}")
    print(f"2. 条件付き書式でG=255を緑色に設定")
    print(f"3. u=0.25 (X079列) とu=0.75 (X239列) の緑ラインを視覚確認")
    print(f"4. 320x160の全体画像として緑ラインパターンを検証")