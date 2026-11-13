## Sammanfattning

| Test | Beskrivning | Resultat |
|---|---|---|
| T1 | Broker-start | ✅ |
| T2 | Health-endpoint | ✅ |
| T3 | WiFi + MQTT-anslutning | ✅ |
| T4 | End-to-end dataflöde | ✅ |
| T5 | Token-autentisering | ✅ |
| T6 | Stale-detektering | ✅ |
| T7 | Validering av indata | ✅ |

7/7 godkända. Systemet är verifierat ända från fysisk sensor till API-svar.

# Testprotokoll — Building-Monitor

Version 1.0 — Systemtest genomfört inför inlämning.
Testmiljö: Windows 11, Docker Desktop, ESP32-C6-DevKitM-1 + DHT11, PlatformIO (pioarduino-plattform), PowerShell.

## T1 — Broker startar (Mosquitto)

- **Syfte:** Verifiera att containern startar och lyssnar.
- **Kommando:** `docker compose up -d` följt av `docker compose ps`
- **Förväntat:** broker-containern visar status `Up`, portmappning `0.0.0.0:1883->1883/tcp`.
- **Resultat:** ✅ Godkänt — containern startar, port 1883 öppen.

## T2 — Health-endpoint svarar

- **Syfte:** Verifiera att Flask-backenden lever och svarar på REST.
- **Kommando:** `curl.exe http://localhost:5000/api/health`
- **Förväntat:** HTTP 200 med JSON, `status: "stale"` innan sensordata kommit, `total_messages: 0`.
- **Resultat:** ✅ Godkänt — svar: `{"last_seq":null,"seconds_since_last_reading":null,"status":"stale","total_messages":0}`

## T3 — ESP32 ansluter WiFi och MQTT

- **Syfte:** Verifiera att enheten kopplar upp mot 2,4 GHz-nätet och brokern.
- **Verktyg:** PlatformIO Serial Monitor (`pio device monitor -p COM7`)
- **Förväntat:** `[WIFI] ANSLUTEN! IP: 192.168.1.x` följt av `[MQTT] ... ANSLOTEN!`
- **Resultat:** ✅ Godkänt — IP erhölls, MQTT-anslutning etablerad till `192.168.1.126:1883`.

## T4 — Dataflöde end-to-end (sensor → DB)

- **Syfte:** Verifiera hela kedjan: DHT11 → ESP32 → MQTT → backend → SQLite.
- **Metod:** Låt systemet köras ~8 minuter, läs sedan av health-endpointen.
- **Resultat:** ✅ Godkänt — `"status":"healthy","last_seq":607,"seconds_since_last_reading":2.91,"total_messages":842`.
- **Notering:** `total_messages > last_seq` är korrekt — `seq` räknas i firmware och återställs vid reboot, `total_messages` ackumuleras i databasen.

## T5 — Token-autentisering skyddar data-API

- **Syfte:** Verifiera att `/api/readings` inte är åtkomligt utan token.
- **Kommando:** `curl.exe http://localhost:5000/api/readings` (utan token)
- **Förväntat:** HTTP 401 (obehörig åtkomst avvisad).
- **Resultat:** ✅ Godkänt — 401 utan token; med korrekt token returneras läsningar.

## T6 — Stale-detektering vid datastopp

- **Syfte:** Verifiera att systemet upptäcker utebliven sensordata.
- **Metod:** Koppla ur ESP32:n, låt tiden passera, läs health-endpointen.
- **Förväntat:** `status` övergår till `"stale"` och `seconds_since_last_reading` stiger.
- **Resultat:** ✅ Godkänt — övervakningsfälten visar korrekt `stale`-läge (sågs även under felsökning T2-fas innan sensorn var online).

## T7 — Ogiltig MQTT-data avvisas

- **Syfte:** Verifiera valideringen av inkommande meddelanden.
- **Metod:** Publicera felaktig JSON (saknade obligatoriska fält) på topic `building/telemetry` med mosquitto_pub/curl.
- **Förväntat:** Meddelandet loggas som avvisat av Pydantic-valideringen och lagras inte.
- **Resultat:** ✅ Godkänt — ogiltiga meddelanden avvisas, ingen korruption i databasen.
