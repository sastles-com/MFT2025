#!/usr/bin/env python3
"""
network_tester.py
ネットワーク疎通・性能測定ツール

isolation-sphere Atom-JoyStick WiFi APの接続確認・性能測定
MacBook用ネットワーク診断・デバッグ支援
"""

import time
import socket
import subprocess
import statistics
import threading
from datetime import datetime
import json
import requests

class NetworkTester:
    def __init__(self, target_ip="192.168.100.1"):
        self.target_ip = target_ip
        self.mqtt_port = 1883
        self.web_port = 80
        self.test_results = {
            "ping_tests": [],
            "tcp_tests": [],
            "http_tests": [],
            "mqtt_tests": []
        }

    def ping_test(self, count=10, timeout=5):
        """Ping接続テスト"""
        print(f"🏓 Ping テスト開始: {self.target_ip}")
        print(f"   パケット数: {count}, タイムアウト: {timeout}秒")
        
        ping_times = []
        success_count = 0
        
        for i in range(count):
            try:
                # macOS/Linux用pingコマンド
                result = subprocess.run([
                    "ping", "-c", "1", "-W", str(timeout * 1000), self.target_ip
                ], capture_output=True, text=True, timeout=timeout + 1)
                
                if result.returncode == 0:
                    # ping時間抽出 (macOS形式: time=1.234ms)
                    output = result.stdout
                    if "time=" in output:
                        time_str = output.split("time=")[1].split("ms")[0]
                        ping_time = float(time_str)
                        ping_times.append(ping_time)
                        success_count += 1
                        print(f"  [{i+1:2d}/{count}] ✅ {ping_time:6.2f}ms")
                    else:
                        print(f"  [{i+1:2d}/{count}] ❌ 応答なし")
                else:
                    print(f"  [{i+1:2d}/{count}] ❌ タイムアウト")
                    
            except subprocess.TimeoutExpired:
                print(f"  [{i+1:2d}/{count}] ⏱️ タイムアウト")
            except Exception as e:
                print(f"  [{i+1:2d}/{count}] ❌ エラー: {e}")
                
            time.sleep(0.2)  # 200ms間隔
        
        # 結果統計
        if ping_times:
            avg_time = statistics.mean(ping_times)
            min_time = min(ping_times)
            max_time = max(ping_times)
            packet_loss = (count - success_count) / count * 100
            
            result_data = {
                "timestamp": datetime.now(),
                "target": self.target_ip,
                "count": count,
                "success": success_count,
                "packet_loss_percent": packet_loss,
                "avg_ms": avg_time,
                "min_ms": min_time,
                "max_ms": max_time
            }
            
            self.test_results["ping_tests"].append(result_data)
            
            print(f"\n📊 Ping統計:")
            print(f"   成功率: {success_count}/{count} ({100-packet_loss:.1f}%)")
            print(f"   応答時間: 平均{avg_time:.2f}ms, 最小{min_time:.2f}ms, 最大{max_time:.2f}ms")
            
            # 性能評価
            if packet_loss == 0 and avg_time < 10:
                print(f"   評価: 🟢 優秀 (低遅延・安定)")
            elif packet_loss < 10 and avg_time < 50:
                print(f"   評価: 🟡 良好 (実用レベル)")
            else:
                print(f"   評価: 🔴 要改善 (高遅延・不安定)")
                
        else:
            print(f"\n❌ 全て失敗 - ネットワーク接続を確認してください")
            
        return ping_times

    def tcp_port_test(self, ports=None):
        """TCPポート疎通テスト"""
        if ports is None:
            ports = [self.mqtt_port, self.web_port, 8000, 22, 80]
            
        print(f"\n🔌 TCPポートテスト: {self.target_ip}")
        
        results = {}
        
        for port in ports:
            start_time = time.time()
            
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(5)
                result = sock.connect_ex((self.target_ip, port))
                connect_time = (time.time() - start_time) * 1000
                sock.close()
                
                if result == 0:
                    print(f"  ポート {port:5d}: ✅ 開放 ({connect_time:.2f}ms)")
                    results[port] = {"status": "open", "time_ms": connect_time}
                else:
                    print(f"  ポート {port:5d}: ❌ 閉鎖/フィルタード")
                    results[port] = {"status": "closed", "time_ms": None}
                    
            except socket.timeout:
                print(f"  ポート {port:5d}: ⏱️ タイムアウト")
                results[port] = {"status": "timeout", "time_ms": None}
            except Exception as e:
                print(f"  ポート {port:5d}: ❌ エラー: {e}")
                results[port] = {"status": "error", "time_ms": None}
        
        self.test_results["tcp_tests"].append({
            "timestamp": datetime.now(),
            "target": self.target_ip,
            "results": results
        })
        
        return results

    def http_test(self, endpoints=None):
        """HTTP接続テスト"""
        if endpoints is None:
            endpoints = [
                f"http://{self.target_ip}/",
                f"http://{self.target_ip}:8000/",
                f"http://{self.target_ip}:8000/api/system/status"
            ]
            
        print(f"\n🌐 HTTP接続テスト:")
        
        results = {}
        
        for endpoint in endpoints:
            try:
                start_time = time.time()
                response = requests.get(endpoint, timeout=10)
                response_time = (time.time() - start_time) * 1000
                
                print(f"  {endpoint}")
                print(f"    ステータス: {response.status_code} ({response_time:.2f}ms)")
                
                # レスポンス内容確認
                if response.headers.get('content-type', '').startswith('application/json'):
                    try:
                        json_data = response.json()
                        print(f"    JSON: {json_data}")
                    except:
                        print(f"    サイズ: {len(response.text)} bytes")
                else:
                    content_preview = response.text[:100].replace('\n', ' ')
                    print(f"    内容: {content_preview}...")
                
                results[endpoint] = {
                    "status_code": response.status_code,
                    "response_time_ms": response_time,
                    "content_length": len(response.text),
                    "success": response.status_code < 400
                }
                
            except requests.exceptions.Timeout:
                print(f"  {endpoint}")
                print(f"    ❌ タイムアウト")
                results[endpoint] = {"status": "timeout", "success": False}
                
            except requests.exceptions.ConnectionError:
                print(f"  {endpoint}")
                print(f"    ❌ 接続エラー")
                results[endpoint] = {"status": "connection_error", "success": False}
                
            except Exception as e:
                print(f"  {endpoint}")
                print(f"    ❌ エラー: {e}")
                results[endpoint] = {"status": "error", "success": False}
        
        self.test_results["http_tests"].append({
            "timestamp": datetime.now(),
            "results": results
        })
        
        return results

    def mqtt_connection_test(self):
        """MQTT接続テスト"""
        print(f"\n📡 MQTT接続テスト: {self.target_ip}:{self.mqtt_port}")
        
        try:
            import paho.mqtt.client as mqtt
            
            connection_result = {"connected": False, "error": None}
            
            def on_connect(client, userdata, flags, rc):
                if rc == 0:
                    print(f"  ✅ MQTT接続成功 (RC={rc})")
                    connection_result["connected"] = True
                    client.publish("test/connection", "test_message", qos=1)
                else:
                    print(f"  ❌ MQTT接続失敗 (RC={rc})")
                    connection_result["error"] = f"Connection failed with RC={rc}"
            
            def on_message(client, userdata, msg):
                print(f"  📨 メッセージ受信: {msg.topic} = {msg.payload.decode()}")
            
            client = mqtt.Client(client_id="network_tester")
            client.on_connect = on_connect
            client.on_message = on_message
            
            start_time = time.time()
            client.connect(self.target_ip, self.mqtt_port, 10)
            client.subscribe("test/connection", qos=1)
            
            # 接続待機（最大10秒）
            client.loop_start()
            
            for i in range(100):  # 10秒間待機
                if connection_result["connected"]:
                    break
                time.sleep(0.1)
                
            connect_time = (time.time() - start_time) * 1000
            
            client.loop_stop()
            client.disconnect()
            
            if connection_result["connected"]:
                print(f"  接続時間: {connect_time:.2f}ms")
                result = {"success": True, "connect_time_ms": connect_time}
            else:
                print(f"  接続失敗: {connection_result.get('error', '不明')}")
                result = {"success": False, "error": connection_result.get('error')}
                
        except ImportError:
            print(f"  ⚠️ paho-mqtt ライブラリが必要です")
            print(f"  インストール: pip install paho-mqtt")
            result = {"success": False, "error": "paho-mqtt not installed"}
            
        except Exception as e:
            print(f"  ❌ MQTT接続エラー: {e}")
            result = {"success": False, "error": str(e)}
        
        self.test_results["mqtt_tests"].append({
            "timestamp": datetime.now(),
            "target": f"{self.target_ip}:{self.mqtt_port}",
            "result": result
        })
        
        return result

    def wifi_info(self):
        """WiFi接続情報表示"""
        print(f"\n📶 WiFi接続情報:")
        
        try:
            # macOS用WiFi情報取得
            result = subprocess.run([
                "networksetup", "-getairportnetwork", "en0"
            ], capture_output=True, text=True)
            
            if result.returncode == 0:
                network_info = result.stdout.strip()
                print(f"  現在の接続: {network_info}")
                
                # SSID確認
                if "IsolationSphere-Direct" in network_info:
                    print(f"  ✅ Atom-JoyStick WiFiに接続済み")
                else:
                    print(f"  ⚠️ Atom-JoyStick WiFiに未接続")
                    print(f"  推奨: SSID 'IsolationSphere-Direct' に接続してください")
                    
            # IP情報取得
            result = subprocess.run([
                "ifconfig", "en0"
            ], capture_output=True, text=True)
            
            if result.returncode == 0:
                ifconfig_output = result.stdout
                # IPアドレス抽出
                for line in ifconfig_output.split('\n'):
                    if 'inet ' in line and 'netmask' in line:
                        ip_info = line.strip().split()
                        ip_addr = ip_info[1]
                        print(f"  IPアドレス: {ip_addr}")
                        
                        # IPアドレス範囲確認
                        if ip_addr.startswith("192.168.100."):
                            print(f"  ✅ IsolationSphere-Direct 範囲内")
                        elif ip_addr.startswith("192.168.49."):
                            print(f"  🔄 ESP32-P2P-Direct 範囲（レガシー）")
                        else:
                            print(f"  ⚠️ 想定外のネットワーク範囲")
                        break
                        
        except Exception as e:
            print(f"  ❌ WiFi情報取得エラー: {e}")

    def comprehensive_test(self):
        """総合ネットワークテスト"""
        print(f"\n{'='*60}")
        print(f"🔍 isolation-sphere ネットワーク総合テスト")
        print(f"対象: {self.target_ip} (Atom-JoyStick)")
        print(f"開始: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"{'='*60}")
        
        # 1. WiFi接続確認
        self.wifi_info()
        
        # 2. Ping テスト
        ping_times = self.ping_test(count=5)
        
        # 3. TCPポート テスト
        tcp_results = self.tcp_port_test()
        
        # 4. HTTP テスト
        http_results = self.http_test()
        
        # 5. MQTT テスト
        mqtt_result = self.mqtt_connection_test()
        
        # 総合評価
        self.display_summary()

    def display_summary(self):
        """テスト結果サマリー表示"""
        print(f"\n{'='*60}")
        print(f"📋 テスト結果サマリー")
        print(f"{'='*60}")
        
        # Ping評価
        if self.test_results["ping_tests"]:
            ping_result = self.test_results["ping_tests"][-1]
            success_rate = (ping_result["count"] - ping_result["success"]) / ping_result["count"] * 100
            avg_time = ping_result.get("avg_ms", 0)
            
            print(f"🏓 Ping: ", end="")
            if success_rate < 5 and avg_time < 20:
                print(f"🟢 優秀 ({100-success_rate:.0f}%, {avg_time:.1f}ms)")
            elif success_rate < 20 and avg_time < 100:
                print(f"🟡 良好 ({100-success_rate:.0f}%, {avg_time:.1f}ms)")
            else:
                print(f"🔴 要改善 ({100-success_rate:.0f}%, {avg_time:.1f}ms)")
        
        # TCP評価
        if self.test_results["tcp_tests"]:
            tcp_result = self.test_results["tcp_tests"][-1]
            open_ports = sum(1 for r in tcp_result["results"].values() if r["status"] == "open")
            total_ports = len(tcp_result["results"])
            
            print(f"🔌 TCP: ", end="")
            if open_ports >= 2:
                print(f"🟢 良好 ({open_ports}/{total_ports}ポート開放)")
            elif open_ports >= 1:
                print(f"🟡 部分的 ({open_ports}/{total_ports}ポート開放)")
            else:
                print(f"🔴 問題あり (ポート閉鎖)")
        
        # MQTT評価
        if self.test_results["mqtt_tests"]:
            mqtt_result = self.test_results["mqtt_tests"][-1]
            if mqtt_result["result"]["success"]:
                connect_time = mqtt_result["result"].get("connect_time_ms", 0)
                print(f"📡 MQTT: 🟢 接続成功 ({connect_time:.1f}ms)")
            else:
                print(f"📡 MQTT: 🔴 接続失敗")
        
        print(f"\n💡 推奨事項:")
        print(f"  1. WiFi: 'IsolationSphere-Direct' に接続")
        print(f"  2. IP範囲: 192.168.100.x を確認")
        print(f"  3. Atom-JoyStick: 電源・起動状態確認")
        print(f"  4. ファイアウォール: ポート1883許可")

    def save_results(self, filename="network_test_results.json"):
        """テスト結果保存"""
        import os
        os.makedirs("logs", exist_ok=True)
        
        # datetime オブジェクトを文字列に変換
        def serialize_datetime(obj):
            if isinstance(obj, datetime):
                return obj.isoformat()
            raise TypeError(f"Object of type {type(obj)} is not JSON serializable")
        
        filepath = f"logs/{filename}"
        with open(filepath, 'w', encoding='utf-8') as f:
            json.dump(self.test_results, f, indent=2, ensure_ascii=False, default=serialize_datetime)
        
        print(f"\n💾 テスト結果保存: {filepath}")

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="ネットワーク疎通テストツール")
    parser.add_argument("--target", default="192.168.100.1", help="テスト対象IP")
    parser.add_argument("--ping-only", action="store_true", help="Pingテストのみ")
    parser.add_argument("--mqtt-only", action="store_true", help="MQTTテストのみ")
    parser.add_argument("--count", type=int, default=10, help="Pingパケット数")
    args = parser.parse_args()
    
    tester = NetworkTester(target_ip=args.target)
    
    try:
        if args.ping_only:
            tester.ping_test(count=args.count)
        elif args.mqtt_only:
            tester.mqtt_connection_test()
        else:
            tester.comprehensive_test()
        
        tester.save_results()
        
    except KeyboardInterrupt:
        print("\n🔚 テスト中断")