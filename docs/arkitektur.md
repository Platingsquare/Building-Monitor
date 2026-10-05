# Arkitektur

## Syfte

IoT-lösning för miljöövervakning i fastigheter: en ESP32-C3 med en DHT11-sensor
samlar in temperatur och luftfuktighet, publicerar mätvärdena som strukturerad
JSON via MQTT till en Mosquitto-broker. En backend-tjänst prenumererar på
datan, validerar den, lagrar den i SQLite och exponerar den via ett
token-skyddat REST-API.

## Översiktsdiagram

```
[DHT11] → [ESP32-C3] --MQTT (pub)--> [Mosquitto broker] --(sub)--> [Backend]
                                                                   |
                                                           [SQLite-lagring]
                                                                   |
                                                       [REST-API, token-skyddat]
                                                                   ▲
                                                     (GET /api/readings/latest)
                                                               [Konsument]
```

## Komponenter

| Komponent | Ansvar | Protokoll/Port |
|-----------|--------|----------------|
| DHT11 | Fysisk mätning av temperatur och luftfuktighet | 1-wire mot GPIO |
| ESP32-C3 | Läsa sensor, bygga JSON, MQTT-publicering, watchdog, reconnect med backoff | MQTT/TCP 1883 |
| Mosquitto broker | Meddelandespridning mellan pub och sub | MQTT/TCP 1883 |
| Backend (Flask) | MQTT-konsument, Pydantic-validering, SQLite-lagring, REST-endpoints | HTTP/TCP 5000 |
| SQLite | Persistens av telemetri | Fil (telemetry.db) |

## Kommunikationsmodell

- **ESP32 → Broker: Publish/Subscribe.** Sensorn publicerar utan att känna till
  mottagare, vilket håller den löst kopplad från backenden. Om backenden
  startar om fortsätter sensorn publicera obehindrat.
- **Konsument → Backend: Request/Response.** Klienten initierar GET-anrop och
  får direkt svar med statuskod, vilket ger enkel felhantering.

## Datamodell (MQTT-payload, JSON)

Publiceras på topic `building/telemetry`:

```json
{
  "device_id": "esp-c3-01",
  "seq": 42,
  "uptime_s": 12345,
  "metrics": {
    "temperature": 21.7,
    "humidity": 55.3
  }
}
```

Backend validerar payloaden mot Pydantic-modellen `TelemetryPayload`.
`seq` är ett löpnummer som möjliggör detektering av förlorade eller
dubbletterade meddelanden. Servertidsstämpel läggs till av backenden vid
insättning i databasen.

## Dataflöde steg för steg

1. ESP32-C3 läser DHT11-periodiskt
2. Sensordatan serialiseras som JSON och publiceras på `building/telemetry`
3. Backend prenumererar på topicen hos brokern (localhost:1883)
4. Mottagen data valideras – ogiltig data loggas och sparas aldrig
5. Godkänd data skrivs till SQLite-tabellen `readings`
6. Konsumenter hämtar data via `GET /api/readings/latest` (Bearer-token krävs)

## Beteende vid fel (designval)

- **Broker nere:** ESP32:ns firmware försöker återansluta med backoff
  (implementerat i `esp32/main.cpp`).
- **Backend nere:** brokern tar emot publiceringar oberoende – sensorn påverkas
  inte. När backenden återstartar återupptas lagringen.
- **Ogiltig payload:** loggas som valideringsfel, skrivs aldrig till databasen.
- **Ingen data på länge:** `GET /api/health` svarar `"status": "stale"`
  (övervakningsmått, se `docs/api.md`).

## Kända begränsningar

- MQTT körs utan TLS (port 1883) – TLS på 8883 är förberett i konfigurationen
  men inte aktiverat.
- Brokern tillåter anonyma anslutningar (labbsyfte).
- REST-API:t exponerar bara senaste läsningen, ingen historik-endpoint.