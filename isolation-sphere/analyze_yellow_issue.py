#!/usr/bin/env python3
"""
黄色表示問題の詳細分析
太いライン範囲での重複ピクセル問題を調査
"""

import csv
from debug_uv_conversion import spherical_to_uv_fast_approximation

def analyze_thick_line_overlap():
    """
    太いライン範囲内でのピクセル重複を詳細分析
    """
    print("🔍 太いライン範囲での重複ピクセル分析")
    print("=" * 60)
    
    # LED座標読み込み
    led_data = []
    with open('scripts/led_layout.csv', 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            face_id = int(row['FaceID'])
            x = float(row['x'])
            y = float(row['y'])
            z = float(row['z'])
            led_data.append({'face_id': face_id, 'x': x, 'y': y, 'z': z})
    
    # 太いライン範囲
    u25_range = range(74, 86)   # X074～X085
    u75_range = range(233, 246) # X233～X245
    
    # 太いライン範囲内のLEDのみを抽出
    u25_leds = []
    u75_leds = []
    
    for led in led_data:
        u, v = spherical_to_uv_fast_approximation(led['x'], led['y'], led['z'])
        px = int(u * 319)
        py = int(v * 159)
        
        if px in u25_range:
            u25_leds.append({
                'face_id': led['face_id'],
                'px': px, 'py': py,
                'u': u, 'v': v,
                'x': led['x'], 'y': led['y'], 'z': led['z']
            })
        
        if px in u75_range:
            u75_leds.append({
                'face_id': led['face_id'],
                'px': px, 'py': py,
                'u': u, 'v': v,
                'x': led['x'], 'y': led['y'], 'z': led['z']
            })
    
    print(f"🟢 u25範囲のLED数: {len(u25_leds)}")
    print(f"🔴 u75範囲のLED数: {len(u75_leds)}")
    
    # 各範囲内でのピクセル重複をチェック
    def check_pixel_overlap_in_range(leds, color_name):
        pixel_to_leds = {}
        for led in leds:
            key = (led['px'], led['py'])
            if key not in pixel_to_leds:
                pixel_to_leds[key] = []
            pixel_to_leds[key].append(led)
        
        overlapping_pixels = {k: v for k, v in pixel_to_leds.items() if len(v) > 1}
        
        print(f"\n{color_name}範囲内の重複:")
        print(f"  ユニークピクセル数: {len(pixel_to_leds)}")
        print(f"  重複ピクセル数: {len(overlapping_pixels)}")
        
        if overlapping_pixels:
            print(f"  重複詳細:")
            for (px, py), leds_list in list(overlapping_pixels.items())[:5]:  # 最初の5件
                face_ids = [led['face_id'] for led in leds_list]
                print(f"    Pixel({px},{py}): {len(leds_list)}個LED {face_ids}")
        
        return overlapping_pixels
    
    u25_overlaps = check_pixel_overlap_in_range(u25_leds, "🟢 u25")
    u75_overlaps = check_pixel_overlap_in_range(u75_leds, "🔴 u75")
    
    return u25_overlaps, u75_overlaps, u25_leds, u75_leds

def check_cross_range_collision():
    """
    異なる範囲間でのピクセル衝突をチェック
    （u25のLEDとu75のLEDが同じピクセルを参照するか）
    """
    print(f"\n🔍 範囲間でのピクセル衝突チェック")
    print("=" * 60)
    
    # LED座標読み込み
    led_data = []
    with open('scripts/led_layout.csv', 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            face_id = int(row['FaceID'])
            x = float(row['x'])
            y = float(row['y'])
            z = float(row['z'])
            led_data.append({'face_id': face_id, 'x': x, 'y': y, 'z': z})
    
    # 全LEDのピクセルマッピング
    all_pixel_mappings = {}
    
    for led in led_data:
        u, v = spherical_to_uv_fast_approximation(led['x'], led['y'], led['z'])
        px = int(u * 319)
        py = int(v * 159)
        
        key = (px, py)
        if key not in all_pixel_mappings:
            all_pixel_mappings[key] = []
        
        # LEDがどの範囲に属するかを判定
        range_type = "other"
        if 74 <= px <= 85:
            range_type = "u25"
        elif 233 <= px <= 245:
            range_type = "u75"
        
        all_pixel_mappings[key].append({
            'face_id': led['face_id'],
            'range_type': range_type,
            'px': px, 'py': py,
            'u': u, 'v': v
        })
    
    # 異なる範囲のLEDが同じピクセルを共有するケースを検索
    cross_collisions = {}
    for pixel, leds_list in all_pixel_mappings.items():
        if len(leds_list) > 1:
            range_types = set(led['range_type'] for led in leds_list)
            if len(range_types) > 1 and ('u25' in range_types or 'u75' in range_types):
                cross_collisions[pixel] = leds_list
    
    print(f"範囲間ピクセル衝突数: {len(cross_collisions)}")
    
    if cross_collisions:
        print(f"\n❌ 範囲間衝突発見！")
        print("Pixel(X,Y) | LEDs | 範囲タイプ")
        print("-" * 50)
        
        for (px, py), leds_list in list(cross_collisions.items())[:10]:  # 最初の10件
            face_ids = [led['face_id'] for led in leds_list]
            range_types = [led['range_type'] for led in leds_list]
            print(f"({px:3d},{py:3d})   | {face_ids} | {range_types}")
        
        # 黄色表示の原因となりうるケースを特定
        yellow_causes = []
        for pixel, leds_list in cross_collisions.items():
            range_types = [led['range_type'] for led in leds_list]
            if 'u25' in range_types and 'u75' in range_types:
                yellow_causes.append((pixel, leds_list))
        
        if yellow_causes:
            print(f"\n🟡 黄色表示の原因ケース: {len(yellow_causes)}件")
            for (px, py), leds_list in yellow_causes[:5]:
                print(f"  Pixel({px},{py}): u25とu75のLEDが同じピクセルを参照")
    else:
        print(f"✅ 範囲間衝突なし")
    
    return cross_collisions

def suggest_solution():
    """
    解決策を提案
    """
    print(f"\n💡 解決策の提案")
    print("=" * 60)
    
    print(f"🎯 問題: 太いライン範囲でLEDが密集し、同じピクセルを複数LEDが参照")
    print(f"")
    print(f"✅ 解決策1: ライン幅を調整")
    print(f"   - u25範囲: X074～X085 → X078～X082 (中央5ピクセル)")
    print(f"   - u75範囲: X233～X245 → X237～X241 (中央5ピクセル)")
    print(f"")
    print(f"✅ 解決策2: パノラマ解像度向上")
    print(f"   - 320x160 → 640x320 (2倍解像度)")
    print(f"   - より細かいピクセルマッピング")
    print(f"")
    print(f"✅ 解決策3: LED選択アルゴリズム改善")
    print(f"   - 同じピクセルに複数LEDがある場合、代表的な1個のみ選択")
    print(f"   - 最近傍LED選択ロジック")

if __name__ == "__main__":
    print("🔍 黄色表示問題：太いライン範囲での重複分析")
    print("=" * 70)
    
    # 1. 太いライン範囲内での重複分析
    u25_overlaps, u75_overlaps, u25_leds, u75_leds = analyze_thick_line_overlap()
    
    # 2. 範囲間でのピクセル衝突チェック
    cross_collisions = check_cross_range_collision()
    
    # 3. 解決策提案
    suggest_solution()
    
    print(f"\n🎯 診断結果:")
    if len(cross_collisions) > 0:
        print(f"❌ 範囲間ピクセル衝突が黄色表示の原因")
        print(f"💡 太いライン範囲を狭めるか、解像度を上げる必要")
    elif len(u25_overlaps) > 0 or len(u75_overlaps) > 0:
        print(f"⚠️  範囲内でのLED密集が問題")
        print(f"💡 LED選択アルゴリズムの改善が必要")
    else:
        print(f"✅ ピクセル重複問題なし")