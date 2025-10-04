#!/usr/bin/env python3
"""
Red値（X240）とGreen値（X80）を比較検証
"""

import csv

def extract_red_channel():
    """
    Red値を320x160のCSV形式で出力（X240の赤ラインを確認）
    """
    PANORAMA_WIDTH = 320
    PANORAMA_HEIGHT = 160
    
    # パノラマ配列初期化（RGB全て0）
    panorama_rgb = [[[0, 0, 0] for _ in range(PANORAMA_WIDTH)] for _ in range(PANORAMA_HEIGHT)]
    
    # テストライン設定
    u25_x = int(0.25 * (PANORAMA_WIDTH - 1))  # u=0.25 → x=79
    u75_x = int(0.75 * (PANORAMA_WIDTH - 1))  # u=0.75 → x=239
    
    print(f"🟢 u=0.25ライン位置: x={u25_x} (緑ライン RGB(0,255,0))")
    print(f"🔴 u=0.75ライン位置: x={u75_x} (赤ライン RGB(255,0,0))")
    
    # 縦ライン設定
    for y in range(PANORAMA_HEIGHT):
        # u=0.25 緑ライン
        panorama_rgb[y][u25_x] = [0, 255, 0]  # Green line
        # u=0.75 赤ライン  
        panorama_rgb[y][u75_x] = [255, 0, 0]  # Red line
    
    # Red値のみを抽出
    red_values = []
    for y in range(PANORAMA_HEIGHT):
        row = []
        for x in range(PANORAMA_WIDTH):
            red_val = panorama_rgb[y][x][0]  # R値 (RGB[0])
            row.append(red_val)
        red_values.append(row)
    
    # Red値CSV出力
    output_filename = 'panorama_red_320x160.csv'
    with open(output_filename, 'w', newline='', encoding='utf-8') as csvfile:
        writer = csv.writer(csvfile)
        
        # ヘッダー行
        header = [f'X{x:03d}' for x in range(320)]
        writer.writerow(header)
        
        # データ行
        for y, row in enumerate(red_values):
            data_row = [f'Y{y:03d}'] + row
            writer.writerow(data_row)
    
    print(f"✅ Red値CSV出力完了: {output_filename}")
    
    return red_values, u25_x, u75_x

def analyze_led_mapping_issue():
    """
    LEDマッピングの問題を詳細分析
    """
    print("🔍 LEDマッピング問題の詳細分析")
    print("=" * 60)
    
    # CSV読み込み
    led_data = []
    with open('scripts/led_layout.csv', 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            face_id = int(row['FaceID'])
            x = float(row['x'])
            y = float(row['y'])
            z = float(row['z'])
            led_data.append({'face_id': face_id, 'x': x, 'y': y, 'z': z})
    
    # 近似式でUV変換
    from debug_uv_conversion import spherical_to_uv_fast_approximation
    
    # u≈0.25 と u≈0.75 付近のLEDを詳細分析
    u25_leds = []
    u75_leds = []
    tolerance = 0.02  # ±0.02の範囲
    
    for led in led_data:
        u, v = spherical_to_uv_fast_approximation(led['x'], led['y'], led['z'])
        px = int(u * 319)
        py = int(v * 159)
        
        if abs(u - 0.25) < tolerance:
            u25_leds.append({
                'face_id': led['face_id'],
                'u': u, 'v': v, 'px': px, 'py': py,
                'x': led['x'], 'y': led['y'], 'z': led['z']
            })
        
        if abs(u - 0.75) < tolerance:
            u75_leds.append({
                'face_id': led['face_id'],
                'u': u, 'v': v, 'px': px, 'py': py,
                'x': led['x'], 'y': led['y'], 'z': led['z']
            })
    
    print(f"📊 u≈0.25付近のLED数: {len(u25_leds)}")
    print(f"📊 u≈0.75付近のLED数: {len(u75_leds)}")
    
    # 詳細ログ（最初の10個）
    print(f"\n🟢 u≈0.25付近のLED詳細（最初の10個）:")
    print("FaceID | U値      | V値      | PixelX | PixelY | X座標    | Y座標    | Z座標")
    print("-" * 80)
    for i, led in enumerate(u25_leds[:10]):
        print(f"{led['face_id']:6d} | {led['u']:8.3f} | {led['v']:8.3f} | {led['px']:6d} | {led['py']:6d} | {led['x']:8.3f} | {led['y']:8.3f} | {led['z']:8.3f}")
    
    print(f"\n🔴 u≈0.75付近のLED詳細（最初の10個）:")
    print("FaceID | U値      | V値      | PixelX | PixelX | X座標    | Y座標    | Z座標")
    print("-" * 80)
    for i, led in enumerate(u75_leds[:10]):
        print(f"{led['face_id']:6d} | {led['u']:8.3f} | {led['v']:8.3f} | {led['px']:6d} | {led['py']:6d} | {led['x']:8.3f} | {led['y']:8.3f} | {led['z']:8.3f}")
    
    # PixelX分布を分析
    u25_px_dist = {}
    u75_px_dist = {}
    
    for led in u25_leds:
        px = led['px']
        u25_px_dist[px] = u25_px_dist.get(px, 0) + 1
    
    for led in u75_leds:
        px = led['px']
        u75_px_dist[px] = u75_px_dist.get(px, 0) + 1
    
    print(f"\n📈 u≈0.25のPixelX分布:")
    for px in sorted(u25_px_dist.keys()):
        print(f"  X{px:03d}: {u25_px_dist[px]}個のLED")
    
    print(f"\n📈 u≈0.75のPixelX分布:")
    for px in sorted(u75_px_dist.keys()):
        print(f"  X{px:03d}: {u75_px_dist[px]}個のLED")
    
    # 問題の分析
    print(f"\n❗ 問題分析:")
    print(f"1. CSVのジャスト位置: X079=u0.25, X239=u0.75")
    print(f"2. 実際のLED分布: 前後に幅がある（tolerance={tolerance}）")
    print(f"3. LEDは複数のPixelXに分散している")
    
    # 理論値との比較
    theoretical_u25_px = int(0.25 * 319)  # = 79
    theoretical_u75_px = int(0.75 * 319)  # = 239
    
    u25_has_79 = 79 in u25_px_dist
    u75_has_239 = 239 in u75_px_dist
    
    print(f"4. 理論値X079にLED存在: {'✅' if u25_has_79 else '❌'}")
    print(f"5. 理論値X239にLED存在: {'✅' if u75_has_239 else '❌'}")
    
    return u25_leds, u75_leds

def check_panorama_buffer_access():
    """
    パノラマバッファアクセスの問題を分析
    """
    print(f"\n🖼️ パノラマバッファアクセス分析")
    print("=" * 60)
    
    # testPanoramaRGBの実際のアクセスパターンをシミュレート
    PANORAMA_WIDTH = 320
    PANORAMA_HEIGHT = 160
    
    # 問題のケース：LEDがpx=78,80,81にマッピングされる場合
    test_cases = [
        {'px': 78, 'py': 80, 'description': 'u=0.25近傍LED（px=78）'},
        {'px': 79, 'py': 80, 'description': 'u=0.25ジャスト（px=79）'},
        {'px': 80, 'py': 80, 'description': 'u=0.25近傍LED（px=80）'},
        {'px': 238, 'py': 80, 'description': 'u=0.75近傍LED（px=238）'},
        {'px': 239, 'py': 80, 'description': 'u=0.75ジャスト（px=239）'},
        {'px': 240, 'py': 80, 'description': 'u=0.75近傍LED（px=240）'},
    ]
    
    print("ケース | PixelX | PixelY | 配列インデックス | 期待RGB値")
    print("-" * 60)
    
    for case in test_cases:
        px, py = case['px'], case['py']
        array_index = py * PANORAMA_WIDTH + px
        
        # 期待値判定
        if px == 79:
            expected_rgb = "RGB(0,255,0)"
        elif px == 239:
            expected_rgb = "RGB(255,0,0)"
        else:
            expected_rgb = "RGB(0,0,0)"
        
        print(f"{case['description'][:20]:20s} | {px:6d} | {py:6d} | {array_index:12d} | {expected_rgb}")
    
    print(f"\n💡 重要な気づき:")
    print(f"1. LEDは理論値±1-2ピクセルの範囲に分散している")
    print(f"2. CSVは理論値のみに色を設定している")
    print(f"3. 実際のLEDは理論値以外の位置も参照している")
    print(f"4. → 理論値以外の位置は黒色(0,0,0)になる！")

if __name__ == "__main__":
    print("🔍 リング表示問題の根本原因分析")
    print("=" * 70)
    
    # 1. Red値のCSV出力
    extract_red_channel()
    
    # 2. LEDマッピングの詳細分析
    u25_leds, u75_leds = analyze_led_mapping_issue()
    
    # 3. パノラマバッファアクセスの問題分析
    check_panorama_buffer_access()
    
    print(f"\n🎯 結論:")
    print(f"リングが表示されない理由:")
    print(f"1. LEDは理論値（79,239）の前後に分散している")
    print(f"2. パノラマは理論値のみに色を設定している")
    print(f"3. 理論値以外の位置は黒色のまま")
    print(f"4. → 大半のLEDが黒色を参照して消灯している！")
    
    print(f"\n✅ 解決策:")
    print(f"1. パノラマに太いライン（複数ピクセル幅）を設定")
    print(f"2. または、LEDマッピング時に最近傍ピクセルを参照")