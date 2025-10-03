#!/usr/bin/env python3
"""
座標変換の理論分析
atan2の引数順序と座標系の関係を確認
"""

import math

# 実際のFaceIDデータから代表例を抽出
test_coords = [
    {"faceID": 7, "x": 0.031, "y": 0.094, "z": -0.995},
    {"faceID": 79, "x": 0.031, "y": -0.094, "z": -0.995},
    {"faceID": 81, "x": -0.023, "y": -0.291, "z": -0.956},
]

def analyze_coordinate_conversion():
    print("=== 座標変換の理論分析 ===")
    print("X=0付近の座標に対する経度計算")
    print()
    
    for coord in test_coords:
        x, y, z = coord["x"], coord["y"], coord["z"]
        faceID = coord["faceID"]
        
        # 現在の実装（間違い？）
        longitude_current = math.atan2(z, x)  # ESP32コードと同じ
        
        # 正しい実装候補1: atan2(y, x) - 標準的なXY平面の角度
        longitude_xy = math.atan2(y, x)
        
        # 正しい実装候補2: atan2(x, z) - ZX平面の角度
        longitude_zx = math.atan2(x, z)
        
        # 正しい実装候補3: atan2(-z, x) - Z軸反転
        longitude_flip = math.atan2(-z, x)
        
        print(f"FaceID {faceID}: XYZ({x:.3f}, {y:.3f}, {z:.3f})")
        print(f"  現在: atan2(z,x) = {longitude_current:.3f} rad = {math.degrees(longitude_current):.1f}°")
        print(f"  候補1: atan2(y,x) = {longitude_xy:.3f} rad = {math.degrees(longitude_xy):.1f}°") 
        print(f"  候補2: atan2(x,z) = {longitude_zx:.3f} rad = {math.degrees(longitude_zx):.1f}°")
        print(f"  候補3: atan2(-z,x) = {longitude_flip:.3f} rad = {math.degrees(longitude_flip):.1f}°")
        
        # UV変換
        u_current = (longitude_current + math.pi) / (2.0 * math.pi)
        u_xy = (longitude_xy + math.pi) / (2.0 * math.pi) 
        u_zx = (longitude_zx + math.pi) / (2.0 * math.pi)
        u_flip = (longitude_flip + math.pi) / (2.0 * math.pi)
        
        print(f"  U座標:")
        print(f"    現在: {u_current:.3f} → ピクセル{int(u_current * 319)}")
        print(f"    候補1: {u_xy:.3f} → ピクセル{int(u_xy * 319)}")
        print(f"    候補2: {u_zx:.3f} → ピクセル{int(u_zx * 319)}")
        print(f"    候補3: {u_flip:.3f} → ピクセル{int(u_flip * 319)}")
        print()
    
    print("=== X=0の理論位置 ===")
    print("X=0の場合の期待される経度:")
    print("- Z>0, X=0: 経度90° (東) → U=0.75 → ピクセル240")
    print("- Z<0, X=0: 経度270° (-90°) → U=0.25 → ピクセル80")
    print("- Z=0, X>0: 経度0° → U=0.5 → ピクセル160")
    print("- Z=0, X<0: 経度180° → U=0.0/1.0 → ピクセル0/320")
    print()
    
    print("=== 座標系の考察 ===")
    print("1. 現在のatan2(z,x)は ZX平面での角度を計算している")
    print("2. X=0付近でZ≠0なので、90°や270°付近の値になる")
    print("3. これがU≈0.25（ピクセル81）になる原因")
    print("4. 正しくはatan2(x,z)またはatan2(y,x)を使うべき？")

def analyze_sphere_mapping():
    print("\n=== 球面→パノラマ マッピング理論 ===")
    print("標準的な球面座標系:")
    print("- 経度 φ = atan2(y, x)  # XY平面での角度")
    print("- 緯度 θ = asin(z/r)     # Z軸方向の角度")
    print()
    print("パノラマ画像での一般的なマッピング:")
    print("- U = φ/(2π) + 0.5      # 経度 → 水平座標")
    print("- V = θ/π + 0.5         # 緯度 → 垂直座標")
    print()
    print("X=0平面（YZ平面）での期待:")
    print("- Y>0, Z=0: φ=90°  → U=0.75")
    print("- Y<0, Z=0: φ=270° → U=0.25") 
    print("- Y=0, Z>0: φ=0°   → U=0.5")
    print("- Y=0, Z<0: φ=180° → U=0.0")

if __name__ == "__main__":
    analyze_coordinate_conversion()
    analyze_sphere_mapping()