#!/usr/bin/env python3
"""
太いライン（複数ピクセル幅）でパノラマを作成
LEDの実際の分布に合わせて幅広ラインを設定
"""

import csv

def create_thick_line_panorama():
    """
    LEDの実際の分布に合わせて太いラインでパノラマを作成
    """
    PANORAMA_WIDTH = 320
    PANORAMA_HEIGHT = 160
    
    # パノラマ配列初期化（RGB全て0）
    panorama_rgb = [[[0, 0, 0] for _ in range(PANORAMA_WIDTH)] for _ in range(PANORAMA_HEIGHT)]
    
    # 実際のLED分布に基づく太いライン設定
    u25_range = range(74, 86)  # X074～X085（u≈0.25）
    u75_range = range(233, 246)  # X233～X245（u≈0.75）
    
    print(f"🟢 u≈0.25太いライン: X{min(u25_range):03d}～X{max(u25_range):03d} ({len(u25_range)}ピクセル幅)")
    print(f"🔴 u≈0.75太いライン: X{min(u75_range):03d}～X{max(u75_range):03d} ({len(u75_range)}ピクセル幅)")
    
    # 太い縦ライン設定
    for y in range(PANORAMA_HEIGHT):
        # u≈0.25 太い緑ライン
        for x in u25_range:
            if x < PANORAMA_WIDTH:
                panorama_rgb[y][x] = [0, 255, 0]  # Green thick line
        
        # u≈0.75 太い赤ライン
        for x in u75_range:
            if x < PANORAMA_WIDTH:
                panorama_rgb[y][x] = [255, 0, 0]  # Red thick line
    
    return panorama_rgb, u25_range, u75_range

def export_thick_line_csv():
    """
    太いライン版のパノラマをCSV出力
    """
    panorama_rgb, u25_range, u75_range = create_thick_line_panorama()
    
    # Green値とRed値を抽出
    green_values = []
    red_values = []
    
    for y in range(160):
        green_row = []
        red_row = []
        for x in range(320):
            green_val = panorama_rgb[y][x][1]  # G値
            red_val = panorama_rgb[y][x][0]    # R値
            green_row.append(green_val)
            red_row.append(red_val)
        green_values.append(green_row)
        red_values.append(red_row)
    
    # Green値CSV出力
    with open('panorama_thick_green_320x160.csv', 'w', newline='', encoding='utf-8') as csvfile:
        writer = csv.writer(csvfile)
        header = [f'X{x:03d}' for x in range(320)]
        writer.writerow(header)
        
        for y, row in enumerate(green_values):
            data_row = [f'Y{y:03d}'] + row
            writer.writerow(data_row)
    
    # Red値CSV出力
    with open('panorama_thick_red_320x160.csv', 'w', newline='', encoding='utf-8') as csvfile:
        writer = csv.writer(csvfile)
        header = [f'X{x:03d}' for x in range(320)]
        writer.writerow(header)
        
        for y, row in enumerate(red_values):
            data_row = [f'Y{y:03d}'] + row
            writer.writerow(data_row)
    
    # 統計
    total_pixels = 320 * 160
    green_pixels = sum(row.count(255) for row in green_values)
    red_pixels = sum(row.count(255) for row in red_values)
    
    print(f"\n✅ 太いライン版CSV出力完了:")
    print(f"📁 Green値: panorama_thick_green_320x160.csv")
    print(f"📁 Red値: panorama_thick_red_320x160.csv")
    
    print(f"\n📊 統計:")
    print(f"総ピクセル数: {total_pixels}")
    print(f"緑ピクセル数: {green_pixels} ({green_pixels/total_pixels*100:.1f}%)")
    print(f"赤ピクセル数: {red_pixels} ({red_pixels/total_pixels*100:.1f}%)")
    print(f"緑ライン幅: {len(u25_range)}ピクセル")
    print(f"赤ライン幅: {len(u75_range)}ピクセル")

def simulate_led_coverage():
    """
    太いライン版でのLEDカバレッジをシミュレート
    """
    print(f"\n🔍 太いライン版LEDカバレッジシミュレーション")
    print("=" * 60)
    
    # 実際のLED分布データ（前回の分析結果）
    u25_led_distribution = {
        74: 2, 75: 2, 76: 4, 78: 2, 79: 2, 80: 4, 81: 2, 82: 4, 83: 2, 84: 1, 85: 6
    }
    
    u75_led_distribution = {
        233: 4, 234: 1, 235: 2, 236: 4, 237: 2, 238: 2, 239: 2, 240: 4, 242: 4, 243: 2, 244: 2, 245: 2
    }
    
    # カバレッジ計算
    u25_covered = 0
    u25_total = sum(u25_led_distribution.values())
    
    for px, count in u25_led_distribution.items():
        if 74 <= px <= 85:  # 太いライン範囲
            u25_covered += count
    
    u75_covered = 0
    u75_total = sum(u75_led_distribution.values())
    
    for px, count in u75_led_distribution.items():
        if 233 <= px <= 245:  # 太いライン範囲
            u75_covered += count
    
    print(f"🟢 u≈0.25領域:")
    print(f"  カバーされるLED: {u25_covered}/{u25_total} ({u25_covered/u25_total*100:.1f}%)")
    
    print(f"🔴 u≈0.75領域:")
    print(f"  カバーされるLED: {u75_covered}/{u75_total} ({u75_covered/u75_total*100:.1f}%)")
    
    total_covered = u25_covered + u75_covered
    total_leds = u25_total + u75_total
    
    print(f"\n🎯 全体カバレッジ:")
    print(f"  カバーされるLED: {total_covered}/{total_leds} ({total_covered/total_leds*100:.1f}%)")
    
    return u25_covered == u25_total and u75_covered == u75_total

if __name__ == "__main__":
    print("🎨 太いライン（複数ピクセル幅）パノラマ作成")
    print("=" * 70)
    
    # 太いライン版パノラマ作成・出力
    export_thick_line_csv()
    
    # LEDカバレッジシミュレーション
    full_coverage = simulate_led_coverage()
    
    print(f"\n🎉 結果:")
    if full_coverage:
        print(f"✅ 完全カバレッジ達成！全LEDが正しい色を参照可能")
    else:
        print(f"⚠️  部分カバレッジ、一部LEDは改善が必要")
    
    print(f"\n🚀 次のステップ:")
    print(f"1. ESP32のinitializeTestPanorama()を太いライン版に更新")
    print(f"2. 実機でリング表示をテスト")
    print(f"3. 近似式を使用して高速処理を実現")