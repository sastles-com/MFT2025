#!/usr/bin/env python3
"""
dummy_esp32_client.py
ESP32デバイス動作シミュレーター

isolation-sphere Atom-JoyStickのテスト用ダミーESP32クライアント
MQTT接続・LED制御受信・IMUデータ送信・状態報告を模擬
"""

import time
import json
import math
import random
import logging
import threading
from datetime import datetime
import paho.mqtt.client as mqtt

# ログ設定
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('logs/dummy_esp32.log'),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger('DummyESP32')

class DummyESP32Client:
    def __init__(self, device_id="esp32-display-main", broker_ip="192.168.100.1", broker_port=1883):
        self.device_id = device_id
        self.broker_ip = broker_ip
        self.broker_port = broker_port
        
        # デバイス状態
        self.device_state = {
            "mac": "AA:BB:CC:DD:EE:01",
            "ip": "192.168.100.20", 
            "online": True,
            "battery_level": 85,
            "cpu_temperature": 45.2,
            "led_brightness": 128,
            "led_color": "#FFFFFF",
            "imu_enabled": True
        }
        
        # MQTT クライアント
        self.client = mqtt.Client(client_id=f"{self.device_id}_client")
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect
        
        # IMUシミュレーション用
        self.imu_thread = None
        self.running = False
        self.sequence = 0
        
        # LED制御状態
        self.led_strip_count = 800
        self.current_pattern = "solid"

    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            logger.info(f"✅ MQTTブローカー接続成功: {self.broker_ip}:{self.broker_port}")
            
            # デバイス制御Topic購読
            topics = [
                f"isolation-sphere/device/{self.device_id}/cmd/brightness",
                f"isolation-sphere/device/{self.device_id}/cmd/pattern", 
                f"isolation-sphere/device/{self.device_id}/config/update",
                "isolation-sphere/cmd/brightness_adjust",
                "isolation-sphere/cmd/playback/toggle",
                "isolation-sphere/cmd/playback/next",
                "isolation-sphere/global/sync/timestamp"
            ]
            
            for topic in topics:
                client.subscribe(topic, qos=1)
                logger.info(f"📡 Topic購読: {topic}")
                
            # デバイス発見アナウンス
            self.announce_device()
            
        else:
            logger.error(f"❌ MQTT接続失敗: RC={rc}")

    def on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload.decode('utf-8')
        
        logger.info(f"📨 受信: {topic} = {payload}")
        
        try:
            # 明度調整コマンド
            if "brightness" in topic:
                if payload.startswith("+"):
                    adjustment = int(payload[1:])
                    self.device_state["led_brightness"] = min(255, self.device_state["led_brightness"] + adjustment)
                elif payload.startswith("-"):
                    adjustment = int(payload[1:])
                    self.device_state["led_brightness"] = max(0, self.device_state["led_brightness"] - adjustment)
                else:
                    self.device_state["led_brightness"] = int(payload)
                
                self.simulate_led_control()
                logger.info(f"💡 LED明度調整: {self.device_state['led_brightness']}")
                
            # パターン制御コマンド
            elif "pattern" in topic:
                pattern_data = json.loads(payload)
                self.device_state["led_color"] = pattern_data.get("color", "#FFFFFF")
                self.current_pattern = pattern_data.get("type", "solid")
                
                self.simulate_pattern_change()
                logger.info(f"🎨 LEDパターン変更: {self.current_pattern}, 色: {self.device_state['led_color']}")
                
            # 再生制御
            elif "playback/toggle" in topic:
                self.simulate_playback_toggle()
                logger.info("⏯️ 再生/停止切り替え")
                
            elif "playback/next" in topic:
                self.simulate_next_video()
                logger.info("⏭️ 次の動画")
                
            # 設定更新
            elif "config/update" in topic:
                config_data = json.loads(payload)
                self.update_device_config(config_data)
                logger.info(f"⚙️ 設定更新: {config_data}")
                
            # タイムスタンプ同期
            elif "sync/timestamp" in topic:
                timestamp = int(payload)
                self.sync_timestamp(timestamp)
                logger.info(f"🕒 時刻同期: {datetime.fromtimestamp(timestamp/1000)}")
                
        except Exception as e:
            logger.error(f"❌ メッセージ処理エラー: {e}")

    def on_disconnect(self, client, userdata, rc):
        logger.warning(f"⚠️ MQTT接続切断: RC={rc}")

    def announce_device(self):
        """デバイス発見アナウンス送信"""
        announce_data = {
            "device_id": self.device_id,
            "type": "led_display",
            "capabilities": ["led_control", "imu_sensor", "udp_receiver"],
            "mac": self.device_state["mac"],
            "ip": self.device_state["ip"],
            "version": "1.0.0",
            "timestamp": int(time.time() * 1000)
        }
        
        self.client.publish(
            "isolation-sphere/global/discovery/announce",
            json.dumps(announce_data),
            qos=1,
            retain=False
        )
        logger.info(f"📢 デバイス発見アナウンス送信")

    def publish_device_status(self):
        """デバイス状態定期送信"""
        status_data = {
            "online": self.device_state["online"],
            "last_seen": int(time.time() * 1000),
            "version": "1.0.0",
            "uptime_ms": int(time.time() * 1000),
            "battery_level": self.device_state["battery_level"],
            "cpu_temperature": self.device_state["cpu_temperature"],
            "led_brightness": self.device_state["led_brightness"],
            "led_pattern": self.current_pattern,
            "memory_usage": random.randint(40, 60)
        }
        
        self.client.publish(
            f"isolation-sphere/device/{self.device_id}/status/online",
            json.dumps(status_data),
            qos=1,
            retain=True
        )

    def publish_imu_data(self):
        """IMUデータ送信（30Hz）"""
        while self.running:
            if self.device_state["imu_enabled"]:
                # 模擬Quaternionデータ生成（回転シミュレーション）
                t = time.time() * 0.5  # ゆっくり回転
                quaternion = {
                    "w": math.cos(t / 2.0),
                    "x": 0.0,
                    "y": math.sin(t / 2.0), 
                    "z": 0.0,
                    "timestamp": int(time.time() * 1000),
                    "sequence": self.sequence,
                    "temperature": self.device_state["cpu_temperature"] + random.uniform(-1, 1)
                }
                
                self.client.publish(
                    "isolation-sphere/imu/quaternion",
                    json.dumps(quaternion),
                    qos=0,  # IMUデータはQoS 0
                    retain=False
                )
                
                # 校正データも送信（低頻度）
                if self.sequence % 30 == 0:  # 1秒間隔
                    calibration_data = {
                        "accel": 3, "gyro": 3, "mag": 3, "system": 3,
                        "timestamp": int(time.time() * 1000)
                    }
                    self.client.publish(
                        "isolation-sphere/imu/calibration",
                        json.dumps(calibration_data),
                        qos=1,
                        retain=True
                    )
                
                self.sequence += 1
            
            time.sleep(1.0 / 30.0)  # 30Hz

    def simulate_led_control(self):
        """LED制御シミュレーション"""
        # 実際のESP32では800個のLED制御
        logger.info(f"🌈 LED制御実行: {self.led_strip_count}個, 明度: {self.device_state['led_brightness']}")
        
        # 応答時間シミュレーション (15-30ms目標)
        response_time = random.uniform(0.015, 0.025)  # 15-25ms
        time.sleep(response_time)
        
        # 制御結果報告
        result_data = {
            "brightness": self.device_state["led_brightness"],
            "pattern": self.current_pattern,
            "color": self.device_state["led_color"],
            "response_time_ms": response_time * 1000,
            "timestamp": int(time.time() * 1000)
        }
        
        self.client.publish(
            f"isolation-sphere/device/{self.device_id}/status/led_control",
            json.dumps(result_data),
            qos=1
        )

    def simulate_pattern_change(self):
        """LEDパターン変更シミュレーション"""
        logger.info(f"✨ LEDパターン変更: {self.current_pattern}")
        time.sleep(0.02)  # 20ms処理時間

    def simulate_playback_toggle(self):
        """再生制御シミュレーション"""
        logger.info("🎬 再生制御実行")
        time.sleep(0.01)  # 10ms処理時間

    def simulate_next_video(self):
        """次動画制御シミュレーション"""
        logger.info("📹 次動画切り替え実行")  
        time.sleep(0.015)  # 15ms処理時間

    def update_device_config(self, config_data):
        """デバイス設定更新"""
        for key, value in config_data.items():
            if key in self.device_state:
                self.device_state[key] = value

    def sync_timestamp(self, timestamp):
        """時刻同期"""
        # 実際のデバイスでは内部時計同期
        pass

    def start(self):
        """ダミーESP32開始"""
        try:
            # MQTTブローカー接続
            logger.info(f"🚀 ダミーESP32起動: {self.device_id}")
            logger.info(f"📡 MQTTブローカー接続試行: {self.broker_ip}:{self.broker_port}")
            
            self.client.connect(self.broker_ip, self.broker_port, 60)
            self.running = True
            
            # IMUデータ送信スレッド開始
            self.imu_thread = threading.Thread(target=self.publish_imu_data, daemon=True)
            self.imu_thread.start()
            
            # 定期ステータス送信スレッド開始  
            status_thread = threading.Thread(target=self.status_loop, daemon=True)
            status_thread.start()
            
            # MQTTループ開始
            self.client.loop_forever()
            
        except Exception as e:
            logger.error(f"❌ 起動エラー: {e}")
            self.stop()

    def status_loop(self):
        """定期ステータス送信ループ"""
        while self.running:
            self.publish_device_status()
            
            # バッテリー・温度シミュレーション
            self.device_state["battery_level"] = max(20, self.device_state["battery_level"] - random.uniform(0, 0.1))
            self.device_state["cpu_temperature"] = 45.0 + random.uniform(-3, 3)
            
            time.sleep(30)  # 30秒間隔

    def stop(self):
        """ダミーESP32停止"""
        logger.info("🛑 ダミーESP32停止")
        self.running = False
        if self.client:
            self.client.disconnect()

if __name__ == "__main__":
    import os
    import argparse
    
    # ログディレクトリ作成
    os.makedirs("logs", exist_ok=True)
    
    # コマンドライン引数
    parser = argparse.ArgumentParser(description="ダミーESP32クライアント")
    parser.add_argument("--broker", default="192.168.100.1", help="MQTTブローカーIP")
    parser.add_argument("--port", type=int, default=1883, help="MQTTブローカーポート")
    parser.add_argument("--device", default="esp32-display-main", help="デバイスID")
    args = parser.parse_args()
    
    # ダミーESP32起動
    dummy_esp32 = DummyESP32Client(
        device_id=args.device,
        broker_ip=args.broker,
        broker_port=args.port
    )
    
    try:
        dummy_esp32.start()
    except KeyboardInterrupt:
        logger.info("🔚 ユーザー終了要求")
        dummy_esp32.stop()