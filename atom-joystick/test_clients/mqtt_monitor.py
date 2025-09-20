#!/usr/bin/env python3
"""
mqtt_monitor.py
MQTT通信監視・デバッグツール

isolation-sphere Atom-JoyStickのMQTT通信をリアルタイム監視
Topic別フィルタリング・統計情報・ログ記録機能
"""

import time
import json
import logging
from datetime import datetime
from collections import defaultdict, deque
import paho.mqtt.client as mqtt

# ログ設定
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('logs/mqtt_monitor.log'),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger('MQTTMonitor')

class MQTTMonitor:
    def __init__(self, broker_ip="192.168.100.1", broker_port=1883):
        self.broker_ip = broker_ip
        self.broker_port = broker_port
        
        # 統計情報
        self.stats = {
            "total_messages": 0,
            "messages_per_topic": defaultdict(int),
            "messages_per_second": deque(maxlen=60),  # 1分間の履歴
            "start_time": time.time(),
            "last_message_time": 0
        }
        
        # フィルター設定
        self.topic_filters = {
            "joystick": ["isolation-sphere/input/joystick"],
            "playback": ["isolation-sphere/ui/playback/", "isolation-sphere/cmd/playback/"],
            "brightness": ["isolation-sphere/ui/settings/brightness", "isolation-sphere/cmd/brightness"],
            "device": ["isolation-sphere/device/"],
            "hub": ["isolation-sphere/hub/status"],
            "imu": ["isolation-sphere/imu/"],
            "system": ["isolation-sphere/cmd/system/"]
        }
        
        # 表示制御
        self.show_all = True
        self.show_categories = set()
        self.max_payload_length = 200
        
        # MQTTクライアント
        self.client = mqtt.Client(client_id="mqtt_monitor")
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect
        
        # メッセージ履歴（MacBook用表示）
        self.message_history = deque(maxlen=1000)

    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print(f"\n🎯 MQTT Monitor 開始")
            print(f"📡 ブローカー: {self.broker_ip}:{self.broker_port}")
            print(f"⏰ 開始時刻: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
            print(f"{'='*80}")
            
            # 全Topicを監視（ワイルドカード）
            client.subscribe("isolation-sphere/#", qos=0)
            logger.info("✅ MQTT監視開始 - 全Topic購読")
            
        else:
            logger.error(f"❌ MQTT接続失敗: RC={rc}")

    def on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload.decode('utf-8', errors='replace')
        timestamp = datetime.now()
        
        # 統計更新
        self.stats["total_messages"] += 1
        self.stats["messages_per_topic"][topic] += 1
        self.stats["last_message_time"] = time.time()
        
        # メッセージ履歴追加
        message_data = {
            "timestamp": timestamp,
            "topic": topic,
            "payload": payload,
            "payload_size": len(payload)
        }
        self.message_history.append(message_data)
        
        # フィルタリング判定
        if not self.should_display_message(topic):
            return
            
        # カテゴリ判定
        category = self.get_message_category(topic)
        
        # ペイロード表示制限
        display_payload = payload
        if len(payload) > self.max_payload_length:
            display_payload = payload[:self.max_payload_length] + "..."
            
        # JSON整形表示
        if self.is_json_payload(payload):
            try:
                parsed_json = json.loads(payload)
                if isinstance(parsed_json, dict) and len(payload) < 500:  # 短いJSONは整形表示
                    display_payload = json.dumps(parsed_json, indent=2, ensure_ascii=False)
            except:
                pass
        
        # カテゴリ別色分け表示（MacBook用）
        color_codes = {
            "joystick": "\033[94m",    # Blue
            "playback": "\033[92m",    # Green  
            "brightness": "\033[93m",   # Yellow
            "device": "\033[95m",      # Magenta
            "hub": "\033[96m",         # Cyan
            "imu": "\033[91m",         # Red
            "system": "\033[90m",      # Dark Gray
            "other": "\033[97m"        # White
        }
        reset_color = "\033[0m"
        
        color = color_codes.get(category, color_codes["other"])
        
        # メッセージ表示
        print(f"{color}[{timestamp.strftime('%H:%M:%S.%f')[:-3]}] "
              f"📂{category.upper():10} | {topic}")
        print(f"📄 {display_payload}{reset_color}")
        print("-" * 80)
        
        # 高頻度メッセージの統計表示
        if topic.endswith("/joystick") and self.stats["messages_per_topic"][topic] % 50 == 0:
            self.display_joystick_stats()

    def on_disconnect(self, client, userdata, rc):
        logger.warning(f"⚠️ MQTT接続切断: RC={rc}")

    def should_display_message(self, topic):
        """メッセージ表示判定"""
        if self.show_all:
            return True
            
        for category in self.show_categories:
            if category in self.topic_filters:
                for filter_pattern in self.topic_filters[category]:
                    if filter_pattern in topic:
                        return True
        return False

    def get_message_category(self, topic):
        """Topic→カテゴリ分類"""
        for category, patterns in self.topic_filters.items():
            for pattern in patterns:
                if pattern in topic:
                    return category
        return "other"

    def is_json_payload(self, payload):
        """JSONペイロード判定"""
        return payload.strip().startswith('{') or payload.strip().startswith('[')

    def display_joystick_stats(self):
        """Joystick統計表示"""
        joystick_topic = "isolation-sphere/input/joystick"
        count = self.stats["messages_per_topic"][joystick_topic]
        elapsed = time.time() - self.stats["start_time"]
        rate = count / elapsed if elapsed > 0 else 0
        
        print(f"\033[94m🎮 Joystickメッセージ統計: {count}件, {rate:.1f}Hz\033[0m")

    def display_overall_stats(self):
        """全体統計表示"""
        elapsed = time.time() - self.stats["start_time"]
        total_rate = self.stats["total_messages"] / elapsed if elapsed > 0 else 0
        
        print(f"\n{'='*80}")
        print(f"📊 MQTT Monitor 統計情報")
        print(f"{'='*80}")
        print(f"⏱️ 監視時間: {elapsed:.1f}秒")
        print(f"📨 総メッセージ数: {self.stats['total_messages']}")
        print(f"📈 平均レート: {total_rate:.2f} msg/sec")
        print()
        
        # Topic別統計
        print("📂 Topic別メッセージ数:")
        sorted_topics = sorted(
            self.stats["messages_per_topic"].items(),
            key=lambda x: x[1],
            reverse=True
        )
        
        for topic, count in sorted_topics[:10]:  # 上位10個
            rate = count / elapsed if elapsed > 0 else 0
            category = self.get_message_category(topic)
            print(f"  {category:10} | {count:5d} ({rate:5.1f}/sec) | {topic}")
        
        if len(sorted_topics) > 10:
            print(f"  ... 他{len(sorted_topics) - 10}個のTopic")
        
        print("="*80)

    def interactive_mode(self):
        """対話モード"""
        print(f"\n🎛️ 対話モード開始")
        print("コマンド:")
        print("  stats  - 統計情報表示")
        print("  filter - フィルター設定")
        print("  clear  - 画面クリア")
        print("  help   - ヘルプ表示")  
        print("  quit   - 終了")
        print()
        
        while True:
            try:
                command = input("monitor> ").strip().lower()
                
                if command == "stats":
                    self.display_overall_stats()
                    
                elif command == "filter":
                    self.configure_filters()
                    
                elif command == "clear":
                    import os
                    os.system('clear' if os.name == 'posix' else 'cls')
                    
                elif command == "help":
                    self.display_help()
                    
                elif command in ["quit", "exit", "q"]:
                    break
                    
                elif command == "":
                    continue
                    
                else:
                    print(f"❌ 不明なコマンド: {command}")
                    
            except KeyboardInterrupt:
                break
            except EOFError:
                break

    def configure_filters(self):
        """フィルター設定"""
        print(f"\n🔧 フィルター設定")
        print("カテゴリ:")
        for i, category in enumerate(self.topic_filters.keys(), 1):
            status = "✅" if category in self.show_categories else "❌"
            print(f"  {i}. {status} {category}")
        
        print("  0. 全て表示")
        print()
        
        try:
            choice = input("選択 (数字またはカテゴリ名): ").strip()
            
            if choice == "0":
                self.show_all = True
                self.show_categories.clear()
                print("✅ 全て表示に設定")
                
            elif choice.isdigit():
                idx = int(choice) - 1
                categories = list(self.topic_filters.keys())
                if 0 <= idx < len(categories):
                    category = categories[idx]
                    if category in self.show_categories:
                        self.show_categories.remove(category)
                        print(f"❌ {category} フィルター無効")
                    else:
                        self.show_all = False
                        self.show_categories.add(category)
                        print(f"✅ {category} フィルター有効")
                        
            elif choice in self.topic_filters:
                if choice in self.show_categories:
                    self.show_categories.remove(choice)
                    print(f"❌ {choice} フィルター無効")
                else:
                    self.show_all = False
                    self.show_categories.add(choice)
                    print(f"✅ {choice} フィルター有効")
            else:
                print("❌ 無効な選択")
                
        except ValueError:
            print("❌ 数字を入力してください")

    def display_help(self):
        """ヘルプ表示"""
        print(f"\n📖 MQTT Monitor ヘルプ")
        print("="*50)
        print("📡 機能:")
        print("  - isolation-sphere全Topic監視")
        print("  - カテゴリ別色分け表示") 
        print("  - リアルタイム統計情報")
        print("  - JSON整形表示")
        print("  - メッセージ履歴記録")
        print()
        print("🎨 カテゴリ色分け:")
        color_map = [
            ("joystick", "青", "Joystick入力"),
            ("playback", "緑", "動画再生制御"),
            ("brightness", "黄", "明度・設定"),
            ("device", "マゼンタ", "デバイス状態"),
            ("hub", "シアン", "ハブ状態"),
            ("imu", "赤", "IMUセンサー"),
            ("system", "グレー", "システム管理")
        ]
        
        for category, color, desc in color_map:
            print(f"  {category:10} ({color:4}) - {desc}")

    def start(self):
        """MQTT監視開始"""
        try:
            import os
            os.makedirs("logs", exist_ok=True)
            
            logger.info(f"🚀 MQTT Monitor 起動")
            
            # MQTT接続
            self.client.connect(self.broker_ip, self.broker_port, 60)
            
            # バックグラウンドでMQTTループ
            import threading
            mqtt_thread = threading.Thread(target=self.client.loop_forever, daemon=True)
            mqtt_thread.start()
            
            # 対話モード開始
            self.interactive_mode()
            
        except Exception as e:
            logger.error(f"❌ 起動エラー: {e}")
        finally:
            self.display_overall_stats()
            self.client.disconnect()

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="MQTT通信監視ツール")
    parser.add_argument("--broker", default="192.168.100.1", help="MQTTブローカーIP")
    parser.add_argument("--port", type=int, default=1883, help="MQTTポート")
    parser.add_argument("--filter", help="初期フィルター設定")
    args = parser.parse_args()
    
    monitor = MQTTMonitor(broker_ip=args.broker, broker_port=args.port)
    
    if args.filter:
        monitor.show_all = False
        monitor.show_categories.add(args.filter)
    
    try:
        monitor.start()
    except KeyboardInterrupt:
        print("\n🔚 監視終了")