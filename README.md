# Building-Monitor

IoT pipeline for building environment monitoring — ESP32 + sensors → MQTT → REST API → database → Grafana dashboard.

## Overview

Skalbar IoT-lösning för miljöövervakning i fastigheter: en ESP32 samlar in sensordata (temperatur/luftfuktighet), publicerar den som JSON över MQTT till en egen broker, där en API-server lagrar datan i en databas och exponerar den via REST. Allt visualiseras i Grafana.

Byggt som projekt i kursen **Nätverk och systemintegration** (IoT25, STI).

**Status: fullt fungerande pipeline** — verifierad med 800+ mätvärden från fysisk sensor (se skärmdump under [Verifiering](#verifiering)).

## Arkitektur

```
[DHT11] → [ESP32-C6] --MQTT--> [Mosquitto broker] --> [API-server Flask] --> [SQLite]
                                  (Docker)                │
                                                          ├── REST /api/health (publik)
                                                          ├── REST /api/readings (token)
                                                          └── [Grafana dashboard]
```

### Komponenter

| Komponent | Ansvar | Protokoll/Port |
|---|---|---|
| **ESP32-C6-DevKitM-1** | Sensordatainsamling (DHT11), JSON-paketering, MQTT-publicering | MQTT / TCP 1883 |
| **Mosquitto broker** | Meddelandespridning, pub/sub | MQTT / TCP 1883 (TLS på 8883 planerad) |
| **API-server (Flask)** | Validering, lagring, REST-endpoints | HTTP / TCP 5000 |
| **Databas (SQLite)** | Persistens av telemetri | SQLite-fil |
| **Grafana** | Visualisering | HTTP / TCP 3000 |

### Dataflöde

Enhetens firmware (Arduino/PlatformIO) läser DHT11 var 5:e sekund, packar mätvärdena som JSON med sekvensnummer och uptime:

```json
{
  "device_id": "ESP32_Climate_Node_01",
  "seq": 607,
  "uptime_s": 3035,
  "metrics": { "temperature": 21.7, "humidity": 45.3 }
}
```

Meddelandet publiceras på topic `building/telemetry`. Backend prenumererar, validerar med Pydantic, lagrar i SQLite och exponerar data via token-skyddat REST-API.

## Repo-struktur

```
Building-Monitor/
├── backend/                  # Flask API + MQTT-konsument + SQLite
│   ├── main.py               # Server + validering + REST
│   ├── requirements.txt
│   ├── .env.example          # Mall — kopiera till .env (committas aldrig!)
│   └── README.md             # Detaljerade startinstruktioner
├── esp32/                    # C++-firmware: DHT11 → JSON → MQTT
│   ├── src/main.cpp          # Arduino/PlatformIO-kod, watchdog, backoff
│   └── platformio.ini        # Board-konfig (ESP32-C6, pioarduino-plattform)
├── mosquitto/
│   └── config/
│       └── mosquitto.conf    # Broker-konfiguration (TLS förberedd)
├── docs/
│   ├── arkitektur.md
│   ├── api.md
│   ├── sakerhet.md
│   ├── felsokning.md         # 6 dokumenterade felsökningsfall
│   └── images/
│       └── api-health.png    # Skärmdump av health-endpoint (se Verifiering)
├── tests/
│   └── testprotokoll.md      # End-to-end-tester med exakta kommandon
├── docker-compose.yml        # Mosquitto-broker i container
├── .gitignore
└── README.md                 # Den här filen
```

## Sprints

| Sprint | Status | Beskrivning |
|---|---|---|
| Initial | ✅ | Repo-struktur, Docker Compose, Mosquitto-konfig |
| Backend | ✅ | Flask-API: MQTT-konsument, Pydantic-validering, SQLite, token-skyddad REST, /api/health |
| Wi-Fi | ✅ | ESP32-C6 ansluter 2,4 GHz-nätverk med retry/backoff |
| Sensor + MQTT | ✅ | DHT11 publicerar JSON via MQTT var 5:e sekund — 800+ meddelanden verifierade |
| API services | ⬜ | Externa API:er (väder) |
| Grafana | ⬜ | Dashboard |
| Säkerhet | ✅ | Token-auth på REST ✔ — TLS (8883) + broker-lösenord återstår |
| Robusthet | ✅ | Watchdog + backoff i firmware ✔, felinjektionstester + felsökningsdok ✔ — ytterligare felinjektion återstår |
## Kom igång

### Förhandskrav

- Docker Desktop
- Python 3.10+
- PlatformIO (VS Code-extension rekommenderas)
- ESP32-C6/C3 och DHT11 (eller motsvarande sensor)

### 1. Broker (Mosquitto)

```bash
docker compose up -d
docker ps   # Kontrollera: broker-containern Up, 0.0.0.0:1883->1883/tcp
```

### 2. Backend

```powershell
cd backend
pip install -r requirements.txt

# Skapa .env (se .env.example)
notepad .env   # Innehåll: APP_TOKEN=... / MQTT_BROKER=localhost / MQTT_PORT=1883

# Starta servern (Windows PowerShell-syntax)
$env:APP_TOKEN="din-token"; $env:MQTT_BROKER="localhost"; $env:MQTT_PORT="1883"; python main.py

# Testa
curl.exe http://localhost:5000/api/health   # Ska svara {"status": "stale", ...} innan sensordata kommit
```

### 3. ESP32-firmware

Konfigurera credentials i `esp32/src/main.cpp`:

```cpp
const char* WIFI_SSID = "DITT_SSID";        // OBS: 2.4 GHz — ESP32 har ingen 5 GHz-radio
const char* WIFI_PASS = "DITT_LOSENORD";
const char* MQTT_HOST = "DIN_DATORS_IP";    // IPv4 på Wireless LAN-adaptern (kör ipconfig)
```

> **Säkerhet:** Riktiga credentials fylls i lokalt och committas **aldrig** till repot.

Bygg och ladda upp (från `esp32/`):

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -t upload
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor
```

Förväntad utskrift i monitorn:

```
[WDT] Timeout omkonfigurerat till 30 s
[WIFI] ANSLUTEN! IP: 192.168.1.115 | RSSI: -58 dBm
[MQTT] Försöker ansluta till 192.168.1.126:1883 ... ANSLOTEN!
[MQTT] Publicerar seq=1 (ok)
```

## Verifiering

När hela kedjan kör svarar health-endpointen:

```powershell
curl.exe http://localhost:5000/api/health
```

```json
{"last_seq": 630, "seconds_since_last_reading": 4.14, "status": "healthy", "total_messages": 865}
```

![API health-check — 842 mätvärden från fysisk sensor](docs/images/api-health.png)<img width="1920" height="1032" alt="Screenshot 2026-11-04 232836" src="https://github.com/user-attachments/assets/6d9efb17-168e-4516-b057-3ca124dd01c4" />
![API health-check — 842 mätvärden från fysisk sensor]()


*(Notera: `total_messages` > `last_seq` är korrekt — seq räknas i firmware och börjar om vid reboot, medan total_messages ackumuleras i databasen.)*

## Säkerhet

- **Token-auth** på REST-endpoints mot data (`.env`-baserad, aldrig i git)
- **Validering** av alla inkommande MQTT-meddelanden (Pydantic) — ogiltig data avvisas
- **Credentials** utanför versionshantering
- **TLS på broker (port 8883)** samt broker-autentisering: planerat nästa steg (se `docs/sakerhet.md`)

## Felsökning

Sex fullständigt dokumenterade felsökningsfall (symptom → identifiering → verktyg → orsak → åtgärd → verifiering) finns i **`docs/felsokning.md`**, inklusive bland annat:

- MQTT-host pekat på fel nätverksadapter (WSL vs Wi-Fi)
- ESP32 kan inte se 5 GHz-nätverk (SSID/band-miss)
- Watchdog-trigging vid blockerande kod i Arduino 3.x
- Serial-utskrifter omdirigerade av fel USB-mode-flagga

## Kända begränsningar

1. ESP32-C6/C3 stöder endast 2.4 GHz Wi-Fi
2. MQTT körs ännu utan TLS (port 1883) — se säkerhetsavsnitt
3. DHT11 har begränsad precision (±2 °C); uppgradering till t.ex. HS3003 är planerad
4. `total_messages` i API:t representerar sessionen sedan senaste DB-rens

## Tekniska val

- **MQTT framför HTTP-pollning:** push-modell, låg overhead, asynkron frikoppling mellan sensor och backend
- **PlatformIO framför Arduino IDE:** projektstruktur, beroendehantering, versionsvänligt
- **Watchdog (TWDT):** automatisk reboot vid hängd firmware — driftsäkerhet för obevakade noder
- **Docker för broker:** reproducerbar drift, enkelt att sätta upp hos annan utvecklare

## Framtida utveckling

- TLS på broker (port 8883) + broker-autentisering
- Grafana-dashboard med historiska grafer
- Integration mot externt väder-API
- Ytterligare felinjektionstester (t.ex. brokersvikt under pågående publicering)
