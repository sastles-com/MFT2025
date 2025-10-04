#!/usr/bin/env python3
"""
LEDの重複マッピング問題を詳細調査
u=0.25とu=0.75が同じLEDにマッピングされる原因を特定
"""

import csv
from debug_uv_conversion import spherical_to_uv_fast_approximation

def analyze_led_overlap():
    """
    LEDの重複マッピングを詳細分析
    """
    print("🔍 LEDの重複マッピング問題調査")
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
    
    # 全LEDのUV変換
    led_mappings = []
    for led in led_data:
        u, v = spherical_to_uv_fast_approximation(led['x'], led['y'], led['z'])
        px = int(u * 319)
        py = int(v * 159)
        
        led_mappings.append({
            'face_id': led['face_id'],
            'x': led['x'], 'y': led['y'], 'z': led['z'],
            'u': u, 'v': v,
            'px': px, 'py': py
        })
    
    # 同じピクセル位置にマッピングされるLEDを検出
    pixel_to_leds = {}
    for mapping in led_mappings:
        key = (mapping['px'], mapping['py'])
        if key not in pixel_to_leds:
            pixel_to_leds[key] = []
        pixel_to_leds[key].append(mapping)
    
    # 重複ピクセルを特定
    overlapping_pixels = {k: v for k, v in pixel_to_leds.items() if len(v) > 1}
    
    print(f"📊 総LED数: {len(led_data)}")
    print(f"📊 ユニークピクセル数: {len(pixel_to_leds)}")
    print(f"📊 重複ピクセル数: {len(overlapping_pixels)}")
    print(f"📊 重複率: {len(overlapping_pixels)/len(pixel_to_leds)*100:.1f}%")
    
    # 重複の詳細分析
    total_overlapping_leds = 0
    for pixel, leds in overlapping_pixels.items():
        total_overlapping_leds += len(leds)
    
    print(f"📊 重複するLED総数: {total_overlapping_leds}")
    print(f"📊 重複LED率: {total_overlapping_leds/len(led_data)*100:.1f}%")
    
    return overlapping_pixels, led_mappings

def check_u025_u075_overlap():
    """
    u=0.25とu=0.75領域の重複を特定
    """
    print(f"\n🎯 u=0.25とu=0.75領域の重複分析")
    print("=" * 60)
    
    # 前回の分析結果を使用
    u25_range = range(74, 86)   # X074～X085
    u75_range = range(233, 246) # X233～X245
    
    # 重複範囲をチェック
    overlap_x = set(u25_range) & set(u75_range)
    
    print(f"🟢 u≈0.25範囲: X{min(u25_range):03d}～X{max(u25_range):03d}")
    print(f"🔴 u≈0.75範囲: X{min(u75_range):03d}～X{max(u75_range):03d}")
    print(f"⚠️  X軸重複: {list(overlap_x) if overlap_x else 'なし'}")
    
    if overlap_x:
        print(f"❌ X軸で重複が発生！これが黄色表示の原因")
        return True
    else:
        print(f"✅ X軸重複なし。別の原因を調査必要")
        return False

def analyze_spherical_geometry():
    """
    球面幾何学的に u=0.25 と u=0.75 が同じLEDになるかチェック
    """
    print(f"\n🌐 球面幾何学分析")
    print("=" * 60)
    
    # u=0.25 と u=0.75 は球面上で正反対位置
    # u座標系: u = (longitude + π) / (2π)
    # u=0.25 → longitude = -π/2 (西側)
    # u=0.75 → longitude = π/2  (東側)
    
    import math
    
    u025_longitude = -math.pi/2  # 西側
    u075_longitude = math.pi/2   # 東側
    
    print(f"u=0.25の経度: {u025_longitude:.3f} rad = {math.degrees(u025_longitude):.1f}°")
    print(f"u=0.75の経度: {u075_longitude:.3f} rad = {math.degrees(u075_longitude):.1f}°")
    print(f"角度差: {abs(u075_longitude - u025_longitude):.3f} rad = {math.degrees(abs(u075_longitude - u025_longitude)):.1f}°")
    
    # 理論的に正反対（180°差）なので、同じLEDにマッピングされるべきではない
    if abs(math.degrees(abs(u075_longitude - u025_longitude)) - 180.0) < 0.1:
        print(f"✅ 理論的に正反対位置（180°差）")
        print(f"❌ 同じLEDにマッピングされるのは異常")
        return True
    else:
        print(f"⚠️  理論値と実際の角度差に問題")
        return False

def find_problematic_leds():
    """
    問題のあるLED（u=0.25とu=0.75両方にマッピング）を特定
    """
    print(f"\n🔍 問題LED特定")
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
    
    # u=0.25とu=0.75両方の条件にマッチするLEDを検索
    tolerance = 0.02
    u025_leds = []
    u075_leds = []
    
    for led in led_data:
        u, v = spherical_to_uv_fast_approximation(led['x'], led['y'], led['z'])
        
        if abs(u - 0.25) < tolerance:
            u025_leds.append((led['face_id'], u, v, led['x'], led['y'], led['z']))
        
        if abs(u - 0.75) < tolerance:
            u075_leds.append((led['face_id'], u, v, led['x'], led['y'], led['z']))
    
    # 同じFaceIDが両方のリストに含まれるかチェック
    u025_face_ids = set(led[0] for led in u025_leds)
    u075_face_ids = set(led[0] for led in u075_leds)
    
    overlap_face_ids = u025_face_ids & u075_face_ids
    
    print(f"u≈0.25のLED数: {len(u025_leds)}")
    print(f"u≈0.75のLED数: {len(u075_leds)}")
    print(f"重複FaceID数: {len(overlap_face_ids)}")
    
    if overlap_face_ids:
        print(f"\n❌ 重複LED発見！")
        print("FaceID | U025値  | U075値  | X座標    | Y座標    | Z座標")
        print("-" * 60)
        
        for face_id in sorted(overlap_face_ids):
            # u025_ledsから該当LED検索
            u025_led = next(led for led in u025_leds if led[0] == face_id)
            u075_led = next(led for led in u075_leds if led[0] == face_id)
            
            print(f"{face_id:6d} | {u025_led[1]:7.3f} | {u075_led[1]:7.3f} | {u025_led[3]:8.3f} | {u025_led[4]:8.3f} | {u025_led[5]:8.3f}")
    else:
        print(f"✅ FaceIDレベルでの重複なし")
    
    return overlap_face_ids

if __name__ == "__main__":
    print("🔍 LEDの重複マッピング問題：根本原因調査")
    print("=" * 70)
    
    # 1. 全体的な重複分析
    overlapping_pixels, led_mappings = analyze_led_overlap()
    
    # 2. u=0.25とu=0.75の範囲重複チェック
    x_axis_overlap = check_u025_u075_overlap()
    
    # 3. 球面幾何学的妥当性チェック
    geometry_issue = analyze_spherical_geometry()
    
    # 4. 問題LEDの特定
    problematic_leds = find_problematic_leds()
    
    print(f"\n🎯 総合診断:")
    if x_axis_overlap:
        print(f"❌ 太いライン範囲がX軸で重複している")
        print(f"💡 解決策: u=0.25とu=0.75の範囲を分離する")
    elif len(problematic_leds) > 0:
        print(f"❌ 同じLEDがu=0.25とu=0.75両方にマッピングされている")
        print(f"💡 解決策: UV変換または許容範囲を調整する")
    elif len(overlapping_pixels) > 0:
        print(f"⚠️  複数LEDが同じピクセルにマッピングされている")
        print(f"💡 解決策: より高解像度パノラマまたはLED分散を改善")
    else:
        print(f"✅ 重複問題なし。別の原因を調査必要")