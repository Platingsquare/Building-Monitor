
# Building-Monitor

IoT pipeline for building environment monitoring — ESP32 + sensors → MQTT → REST API → database → Grafana dashboard.

## Overview
Skalbar IoT-lösning för miljöövervakning i fastigheter: en ESP32 samlar in sensordata (temperatur/luftfuktighet), publicerar den som JSON över MQTT till en egen broker, där en API-server lagrar datan i en databas och exponerar den via REST. Allt visualiseras i Grafana.

Byggt som projekt i kursen Nätverk och systemintegration (IoT25).

## Arkitektur
​```
[DHT11] → [ESP32] --MQTT--> [Mosquitto broker] --> [API-server] --> [SQLite]
                                                      ↓
                                                 REST API (token)
​```

​```
Building-Monitor/
├── backend/            # Flask API + MQTT-konsument + SQLite
├── esp32/              # C++-firmware: DHT11 → JSON → MQTT
├── mosquitto/config/   # broker-konfiguration
└── docker-compose.yml  # Mosquitto-broker
​```
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

## Sprint-logg

### Sprint 1 — Infrastruktur (2026-09-08)

**Nytt:**
- Mosquitto-broker konfigurerad via Docker Compose (port 1883, TLS-port förberedd på 8883)
- Flask-backend: MQTT-konsument → validering (Pydantic) → SQLite → REST-API
- `GET /api/health` — status, meddelanderäknare, tid sedan senaste mätvärde
- `GET /api/readings/latest` — Bearer-token-skyddad
- ESP32-firmware-skelett: Wi-Fi-retry med exponentiell backoff, hardware watchdog, JSON-telemetri var 5:e sekund
- Hemligheter (token, Wi-Fi) flyttade ur koden till miljövariabler — `.env` i `.gitignore`

**Verifierat:**
- [ ] Broker startar: `docker compose up -d` → `iot-mosquitto` Up
- [ ] `GET /api/health` svarar `"status": "stale"` (väntar på sensor-data)

## Tech Used

Docker · Docker Compose · Mosquitto (MQTT) · ESP32 (C++) · Python · Flask · SQLite · Pydantic · Grafana & Tailscale (planerad)

## Kom igång

Se [backend/README.md](backend/README.md) för fullständig setup.
