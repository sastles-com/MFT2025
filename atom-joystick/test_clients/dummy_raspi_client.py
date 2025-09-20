#!/usr/bin/env python3
"""
dummy_raspi_client.py
raspiシステム動作シミュレーター

isolation-sphere Atom-JoyStickのテスト用ダミーraspiクライアント
WebUI・動画管理・UDP通信・MQTT統合を模擬
"""

import time
import json
import threading
import logging
from datetime import datetime
from flask import Flask, jsonify, request
import paho.mqtt.client as mqtt

# ログ設定
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('logs/dummy_raspi.log'),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger('DummyRaspi')

class DummyRaspiClient:
    def __init__(self, broker_ip="192.168.100.1", broker_port=1883, webui_port=8000):
        self.broker_ip = broker_ip
        self.broker_port = broker_port
        self.webui_port = webui_port
        
        # raspiシステム状態
        self.system_state = {
            "hostname": "isolation-pi",
            "ip": "192.168.100.10",
            "uptime_hours": 48,
            "cpu_usage": 25.5,
            "memory_usage": 60.2,
            "disk_usage": 45.8,
            "temperature": 52.3
        }
        
        # 動画管理状態
        self.video_state = {
            "current_video_id": 1,
            "current_video_name": "sample_sphere_demo.mp4",
            "playback_state": "stopped",  # playing, paused, stopped
            "position_seconds": 0,
            "duration_seconds": 120,
            "volume": 75,
            "playlist": [
                {"id": 1, "name": "sample_sphere_demo.mp4", "duration": 120},
                {"id": 2, "name": "led_pattern_test.mp4", "duration": 60}, 
                {"id": 3, "name": "color_gradient.mp4", "duration": 90}
            ]
        }
        
        # システム設定
        self.settings = {
            "led_brightness": 128,
            "rotation_offset": {"x": 0.0, "y": 0.0, "z": 0.0},
            "auto_play": True,
            "loop_mode": False
        }
        
        # MQTT クライアント
        self.mqtt_client = mqtt.Client(client_id="raspi_isolation_pi")
        self.mqtt_client.on_connect = self.on_connect
        self.mqtt_client.on_message = self.on_message
        
        # Flask WebUI
        self.app = Flask(__name__)
        self.setup_webui_routes()
        
        self.running = False

    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            logger.info(f"✅ MQTTブローカー接続成功: {self.broker_ip}:{self.broker_port}")
            
            # UI状態管理Topic購読
            topics = [
                "isolation-sphere/ui/playback/state",
                "isolation-sphere/ui/video/current", 
                "isolation-sphere/ui/settings/brightness",
                "isolation-sphere/ui/settings/rotation_offset",
                "isolation-sphere/cmd/playback/toggle",
                "isolation-sphere/cmd/playback/next",
                "isolation-sphere/cmd/settings/brightness_adjust",
                "isolation-sphere/hub/status"
            ]
            
            for topic in topics:
                client.subscribe(topic, qos=1)
                logger.info(f"📡 Topic購読: {topic}")
                
            # raspi初期状態配信
            self.publish_initial_state()
            
        else:
            logger.error(f"❌ MQTT接続失敗: RC={rc}")

    def on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload.decode('utf-8')
        
        logger.info(f"📨 受信: {topic} = {payload}")
        
        try:
            # 再生制御
            if "playback/toggle" in topic:
                self.handle_playback_toggle()
                
            elif "playback/next" in topic:
                self.handle_next_video()
                
            # 明度調整
            elif "brightness_adjust" in topic:
                self.handle_brightness_adjust(payload)
                
            # UI状態同期
            elif "ui/playback/state" in topic:
                self.video_state["playback_state"] = payload
                logger.info(f"🎬 再生状態同期: {payload}")
                
            elif "ui/settings/brightness" in topic:
                self.settings["led_brightness"] = int(payload)
                logger.info(f"💡 明度設定同期: {payload}")
                
            # ハブ状態監視
            elif "hub/status" in topic:
                hub_data = json.loads(payload)
                self.handle_hub_status(hub_data)
                
        except Exception as e:
            logger.error(f"❌ メッセージ処理エラー: {e}")

    def publish_initial_state(self):
        """初期状態配信"""
        # 現在動画情報
        self.mqtt_client.publish(
            "isolation-sphere/ui/video/current",
            json.dumps({
                "id": self.video_state["current_video_id"],
                "name": self.video_state["current_video_name"],
                "duration": self.video_state["duration_seconds"]
            }),
            qos=1, retain=True
        )
        
        # 再生状態
        self.mqtt_client.publish(
            "isolation-sphere/ui/playback/state",
            self.video_state["playback_state"],
            qos=1, retain=True
        )
        
        # 明度設定
        self.mqtt_client.publish(
            "isolation-sphere/ui/settings/brightness",
            str(self.settings["led_brightness"]),
            qos=1, retain=True
        )

    def handle_playback_toggle(self):
        """再生/停止切り替え"""
        if self.video_state["playback_state"] == "playing":
            self.video_state["playback_state"] = "paused"
        else:
            self.video_state["playback_state"] = "playing"
            
        logger.info(f"⏯️ 再生状態変更: {self.video_state['playback_state']}")
        
        # 状態配信
        self.mqtt_client.publish(
            "isolation-sphere/ui/playback/state",
            self.video_state["playback_state"],
            qos=1, retain=True
        )

    def handle_next_video(self):
        """次の動画"""
        current_index = next(
            i for i, v in enumerate(self.video_state["playlist"]) 
            if v["id"] == self.video_state["current_video_id"]
        )
        
        next_index = (current_index + 1) % len(self.video_state["playlist"])
        next_video = self.video_state["playlist"][next_index]
        
        self.video_state["current_video_id"] = next_video["id"]
        self.video_state["current_video_name"] = next_video["name"]
        self.video_state["duration_seconds"] = next_video["duration"]
        self.video_state["position_seconds"] = 0
        
        logger.info(f"⏭️ 次の動画: {next_video['name']}")
        
        # 動画情報配信
        self.mqtt_client.publish(
            "isolation-sphere/ui/video/current",
            json.dumps({
                "id": next_video["id"],
                "name": next_video["name"], 
                "duration": next_video["duration"]
            }),
            qos=1, retain=True
        )

    def handle_brightness_adjust(self, payload):
        """明度調整"""
        if payload.startswith("+"):
            adjustment = int(payload[1:])
            self.settings["led_brightness"] = min(255, self.settings["led_brightness"] + adjustment)
        elif payload.startswith("-"):
            adjustment = int(payload[1:])
            self.settings["led_brightness"] = max(0, self.settings["led_brightness"] - adjustment)
        else:
            self.settings["led_brightness"] = int(payload)
            
        logger.info(f"💡 明度調整: {self.settings['led_brightness']}")
        
        # 設定配信
        self.mqtt_client.publish(
            "isolation-sphere/ui/settings/brightness",
            str(self.settings["led_brightness"]),
            qos=1, retain=True
        )

    def handle_hub_status(self, hub_data):
        """ハブ状態処理"""
        connected_devices = hub_data.get("connected_devices", 0)
        uptime_ms = hub_data.get("uptime_ms", 0)
        
        logger.info(f"🔗 ハブ状態: {connected_devices}台接続, 稼働時間: {uptime_ms}ms")

    def setup_webui_routes(self):
        """WebUI APIルート設定"""
        
        @self.app.route("/")
        def index():
            return jsonify({
                "service": "isolation-sphere raspi WebUI",
                "version": "1.0.0",
                "status": "running",
                "endpoints": [
                    "/api/system/status",
                    "/api/video/list",
                    "/api/video/current", 
                    "/api/video/control",
                    "/api/settings"
                ]
            })
        
        @self.app.route("/api/system/status")
        def system_status():
            return jsonify(self.system_state)
            
        @self.app.route("/api/video/list")
        def video_list():
            return jsonify({"videos": self.video_state["playlist"]})
            
        @self.app.route("/api/video/current")
        def video_current():
            return jsonify({
                "video_id": self.video_state["current_video_id"],
                "name": self.video_state["current_video_name"],
                "state": self.video_state["playback_state"],
                "position": self.video_state["position_seconds"],
                "duration": self.video_state["duration_seconds"],
                "volume": self.video_state["volume"]
            })
            
        @self.app.route("/api/video/control", methods=["POST"])
        def video_control():
            data = request.get_json()
            action = data.get("action")
            
            if action == "play":
                self.video_state["playback_state"] = "playing"
            elif action == "pause":
                self.video_state["playback_state"] = "paused"
            elif action == "stop":
                self.video_state["playback_state"] = "stopped"
                self.video_state["position_seconds"] = 0
            elif action == "seek":
                position = data.get("position", 0)
                self.video_state["position_seconds"] = position
                
            return jsonify({"success": True, "action": action})
            
        @self.app.route("/api/settings")
        def get_settings():
            return jsonify(self.settings)
            
        @self.app.route("/api/settings", methods=["POST"])
        def update_settings():
            data = request.get_json()
            self.settings.update(data)
            
            # MQTT経由で設定同期
            for key, value in data.items():
                if key == "led_brightness":
                    self.mqtt_client.publish(
                        "isolation-sphere/ui/settings/brightness",
                        str(value), qos=1, retain=True
                    )
                    
            return jsonify({"success": True, "settings": self.settings})

    def video_playback_simulation(self):
        """動画再生シミュレーション"""
        while self.running:
            if self.video_state["playback_state"] == "playing":
                # 再生位置更新
                self.video_state["position_seconds"] += 1
                
                # 動画終了チェック
                if self.video_state["position_seconds"] >= self.video_state["duration_seconds"]:
                    if self.settings["loop_mode"]:
                        self.video_state["position_seconds"] = 0
                    elif self.settings["auto_play"]:
                        self.handle_next_video()
                    else:
                        self.video_state["playback_state"] = "stopped"
                        self.video_state["position_seconds"] = 0
                        
            time.sleep(1)  # 1秒間隔

    def system_monitoring(self):
        """システム監視ループ"""
        import random
        
        while self.running:
            # システム状態更新
            self.system_state["uptime_hours"] += 1/3600  # 1秒増加
            self.system_state["cpu_usage"] = max(10, min(90, self.system_state["cpu_usage"] + random.uniform(-5, 5)))
            self.system_state["memory_usage"] = max(30, min(95, self.system_state["memory_usage"] + random.uniform(-2, 2)))
            self.system_state["temperature"] = max(40, min(65, self.system_state["temperature"] + random.uniform(-1, 1)))
            
            time.sleep(5)  # 5秒間隔

    def start(self):
        """ダミーraspi開始"""
        try:
            logger.info(f"🚀 ダミーraspi起動: {self.system_state['hostname']}")
            
            # MQTT接続
            logger.info(f"📡 MQTTブローカー接続: {self.broker_ip}:{self.broker_port}")
            self.mqtt_client.connect(self.broker_ip, self.broker_port, 60)
            
            self.running = True
            
            # MQTTループ開始
            mqtt_thread = threading.Thread(target=self.mqtt_client.loop_forever, daemon=True)
            mqtt_thread.start()
            
            # 動画再生シミュレーション開始
            video_thread = threading.Thread(target=self.video_playback_simulation, daemon=True)
            video_thread.start()
            
            # システム監視開始
            monitor_thread = threading.Thread(target=self.system_monitoring, daemon=True)
            monitor_thread.start()
            
            # WebUI開始
            logger.info(f"🌐 WebUI起動: http://0.0.0.0:{self.webui_port}")
            self.app.run(host="0.0.0.0", port=self.webui_port, debug=False)
            
        except Exception as e:
            logger.error(f"❌ 起動エラー: {e}")
            self.stop()

    def stop(self):
        """ダミーraspi停止"""
        logger.info("🛑 ダミーraspi停止")
        self.running = False
        if self.mqtt_client:
            self.mqtt_client.disconnect()

if __name__ == "__main__":
    import os
    import argparse
    
    # ログディレクトリ作成
    os.makedirs("logs", exist_ok=True)
    
    # コマンドライン引数
    parser = argparse.ArgumentParser(description="ダミーraspiクライアント")
    parser.add_argument("--broker", default="192.168.100.1", help="MQTTブローカーIP")
    parser.add_argument("--mqtt-port", type=int, default=1883, help="MQTTポート")
    parser.add_argument("--web-port", type=int, default=8000, help="WebUIポート")
    args = parser.parse_args()
    
    # ダミーraspi起動
    dummy_raspi = DummyRaspiClient(
        broker_ip=args.broker,
        broker_port=args.mqtt_port,
        webui_port=args.web_port
    )
    
    try:
        dummy_raspi.start()
    except KeyboardInterrupt:
        logger.info("🔚 ユーザー終了要求")
        dummy_raspi.stop()