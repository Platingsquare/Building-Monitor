# Building-Monitor

IoT pipeline for building environment monitoring — ESP32 + sensors → MQTT → REST API → database → Grafana dashboard.

## Overview

Skalbar IoT-lösning för miljöövervakning i fastigheter: en ESP32 samlar in sensordata (temperatur/luftfuktighet), publicerar den som JSON över MQTT till en egen broker, där en API-server lagrar datan i en databas och exponerar den via REST. Allt visualiseras i Grafana.

Byggt som projekt i kursen **Nätverk och systemintegration** (IoT25, STI).

## Arkitektur
[DHT11] → [ESP32] --MQTT--> [Mosquitto broker] --> [API-server] --> [SQLite] ↓ REST API (token) ↓ [Grafana]


### Komponenter

| Komponent | Ansvar | Protokoll/Port |
|---|---|---|
| **ESP32** | Sensordatainsamling (DHT11), MQTT-publicering | MQTT / TCP 1883 |
| **Mosquitto broker** | Meddelandespridning, pub/sub | MQTT / TCP 1883 (TLS på 8883 planerad) |
| **API-server (Flask)** | Validering, lagring, REST-endpoints | HTTP / TCP 5000 |
| **Databas (SQLite)** | Persistence av telemetri | SQLite-fil |

## Repo-struktur
```
Building-Monitor/
├── backend/     # Flask API + MQTT-konsument + SQLite  
|  ├── main.py     # Server + validering + REST 
|  ├── requirements.txt │ 
|  ├── .env.example     # Mall — kopiera till .env (committas aldrig!) 
|  └── README.md     # Detaljerade startinstruktioner
├── esp32/ # C++-firmware: DHT11 → JSON → MQTT 
│ └── main.cpp # Arduino/PlatformIO-kod, watchdog, backoff ├── mosquitto/
│ └── config/ │ └── mosquitto.conf # Broker-konfiguration (TLS förberedd) 
├── docs/ 
│ ├── arkitektur.md 
│ ├── api.md 
│ ├── sakerhet.md 
│ └── felsokning.md 
├── docker-compose.yml     # Mosquitto-broker i container 
├── .gitignore 
└── README.md     # Den här filen
```

## Sprints

| Sprint | Status | Beskrivning |
|---|---|---|
| Initial | ✅ | Repo-struktur, Docker Compose, Mosquitto-konfig |
| Backend | ✅ | Flask-API: MQTT-konsument, Pydantic-validering, SQLite, token-skyddad REST, /api/health |
| Wi-Fi | ⬜ | ESP32 ansluter nätverket |
| Sensor + MQTT | ⬜ | DHT11 publicerar JSON via MQTT |
| API services | ⬜ | Externa API:er (väder) |
| Grafana | ⬜ | Dashboard |
| Säkerhet | ⬜ | TLS (8883), lösenord på broker, token redan på plats |
| Robusthet | ⬜ | Watchdog + backoff (finns i firmware), felinjektion + felsökningsdok |

## Kom igång

### 1. Broker (Mosquitto)

```bash
docker compose up -d
docker ps  # Kontrollera: iot-mosquitto / Up / 0.0.0.0:1883->1883/tcp

2. Backend
cd backend
pip install -r requirements.txt

# Skapa .env (se .env.example)
notepad .env # Innehåll: APP_TOKEN=... / MQTT_BROKER=localhost / MQTT_PORT=1883

# Starta servern (Windows PowerShell-syntax)
$env:APP_TOKEN="din-token"; $env:MQTT_BROKER="localhost"; $env:MQTT_PORT="1883"; python main.py

# Testa
curl http://localhost:5000/api/health # Ska svara {"status": "stale", ...}
