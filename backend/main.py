export $(cat .env | xargs) && python main.py
"""
Building-Monitor backend:
MQTT consumer -> validation -> SQLite -> REST API (token protected).
Reads config from environment variables (see .env.example).
"""
import json
import os
import sqlite3
import threading
import time
from functools import wraps

from flask import Flask, jsonify, request
from pydantic import BaseModel, ValidationError
import paho.mqtt.client as mqtt

APP_TOKEN = os.environ.get("APP_TOKEN", "change-me")
DB_PATH = "telemetry.db"
MQTT_BROKER = os.environ.get("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.environ.get("MQTT_PORT", 1883))
MQTT_TOPIC = "building/telemetry"

app = Flask(__name__)

health_state = {
    "total_messages": 0,
    "last_seen": None,
    "last_seq": None
}

class MetricsModel(BaseModel):
    temperature: float
    humidity: float

class TelemetryPayload(BaseModel):
    device_id: str
    seq: int
    uptime_s: int
    metrics: MetricsModel

def init_db():
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute("""
            CREATE TABLE IF NOT EXISTS readings (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                device_id TEXT,
                seq INTEGER,
                temperature REAL,
                humidity REAL
            )
        """)
        conn.commit()

def require_token(f):
    @wraps(f)
    def decorated(*args, **kwargs):
        header = request.headers.get("Authorization")
        if not header or not header.startswith("Bearer "):
            return jsonify({"error": "Missing/invalid token format"}), 401
        if header.split(" ")[1] != APP_TOKEN:
            return jsonify({"error": "Forbidden"}), 403
        return f(*args, **kwargs)
    return decorated

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        val = TelemetryPayload(**data)
        with sqlite3.connect(DB_PATH) as conn:
            conn.execute(
                "INSERT INTO readings (device_id, seq, temperature, humidity) VALUES (?, ?, ?, ?)",
                (val.device_id, val.seq, val.metrics.temperature, val.metrics.humidity)
            )
            conn.commit()
        health_state["total_messages"] += 1
        health_state["last_seen"] = time.time()
        health_state["last_seq"] = val.seq
    except (json.JSONDecodeError, ValidationError) as e:
        print(f"Validation error: {e}")

def start_mqtt():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_message = on_message
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.subscribe(MQTT_TOPIC)
    client.loop_forever()

@app.route("/api/health", methods=["GET"])
def health():
    now = time.time()
    diff = (now - health_state["last_seen"]) if health_state["last_seen"] else None
    return jsonify({
        "status": "healthy" if diff and diff < 30 else "stale",
        "total_messages": health_state["total_messages"],
        "seconds_since_last_reading": round(diff, 2) if diff else None,
        "last_seq": health_state["last_seq"]
    })

@app.route("/api/readings/latest", methods=["GET"])
@require_token
def latest():
    with sqlite3.connect(DB_PATH) as conn:
        conn.row_factory = sqlite3.Row
        cur = conn.cursor()
        cur.execute("SELECT * FROM readings ORDER BY timestamp DESC LIMIT 1")
        row = cur.fetchone()
        return jsonify(dict(row)) if row else (jsonify({"error": "No records"}), 404)

if __name__ == "__main__":
    init_db()
    threading.Thread(target=start_mqtt, daemon=True).start()
    app.run(host="0.0.0.0", port=5000)
