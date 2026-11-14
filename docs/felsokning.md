Felsökningsdokumentation — Building-Monitor
Totalt nio fel: tre medvetet injicerade (Del A, enligt uppgiftens krav på avsiktliga fel) och sex naturligt uppkomna under utvecklingen (Del B). Alla följer den sexpunktsstruktur som uppgiften kräver:

1. Symptom → 2. Identifiering → 3. Verktyg/loggar → 4. Orsak → 5. Åtgärd → 6. Verifiering.

Del A — Medvetet injicerade fel
Fel 7: Felaktig autentisering mot API (avsiktlig injektion)
Symptom: curl.exe http://localhost:5000/api/readings utan token avvisades omedelbart.
Identifiering: Jämförde svarskoder med/utan Authorization-header samt med fel token.
Verktyg: curl.exe, backendlogg.
Orsak: Data-API:t kräver token (från .env) — anropet injicerades avsiktligt för att bevisa att autentiseringen skyddar API:t.
Åtgärd: Ingen krävs — avvisandet är avsett beteende (se även T5 i testprotokollet).
Verifiering: HTTP 401 utan/fel token, HTTP 200 med korrekt token. .env committas aldrig till repot.
Fel 8: Fel MQTT-topic (avsiktlig injektion)
Symptom: Monitorn visade [MQTT] Publicerar seq=N (ok) var 5:e sekund — men /api/health rapporterade fryst total_messages och status gick mot stale. Ingen data nådde databasen trots "lyckad" publicering.
Identifiering: Kontrasten mellan två källor: monitorn = fungerande publicering, backendloggen = helt tyst. Jämförde topic-strängen i firmwaren mot backendens prenumeration.
Verktyg: Serial Monitor, curl.exe http://localhost:5000/api/health, docker compose logs backend --tail 5.
Orsak: Topicen ändrades avsiktligt från building/telemetry till building/telemetry_X. MQTT levererar meddelanden till en topic utan prenumeranter "framgångsrikt" — och de försvinner. Backend prenumerar på rätt topic och hör ingenting.
Åtgärd: Återställde topicen till building/telemetry och flashade om.
Verifiering: total_messages steg omedelbart igen vid nästa intervall; status återgick till healthy.
Lärdom: "Publiceringen lyckades" i MQTT bevisar bara att meddelandet lämnade enheten. End-to-end-verifiering mot databas/API är det enda riktiga beviset på dataflöde — därför finns /api/health.

Fel 9: Ogiltig JSON till backenden (avsiktlig injektion)
Symptom: Trasiga meddelanden på rätt topic loggades som valideringsfel; total_messages ökade inte.
Identifiering: Publicerade två avsiktligt ogiltiga meddelanden (saknade obligatoriska fält / icke-JSON) och följde backendloggen + räknaren.
Verktyg: docker compose exec broker mosquitto_pub -t building/telemetry -m '{"device_id": "test"}' och -m 'detta ar inte json', docker compose logs backend --tail 10.
Orsak: Meddelandena bröt mot datakontraktet. Injicerades för att testa indata-valideringen.
Åtgärd: Ingen krävs — valideringen (Pydantic) avvisade och loggade båda (se även T7).
Verifiering: Båda avvisade, räknaren oförändrad, ingen korruption i SQLite; giltiga meddelanden fortsatte accepteras normalt.
Del B — Naturligt uppkomna utvecklingsfel
Fel 1: MQTT_HOST pekade på fel nätverksadapter (fel brokeradress)
Symptom: WiFi kopplade upp, men brokern nåddes aldrig. Brokerlogg: endast backendens klient. API: total_messages: 0, status: "stale".
Identifiering: Jämförde docker compose logs broker med ipconfig — MQTT_HOST i firmwaren var satt till 172.22.80.1.
Verktyg: docker compose logs broker --tail 20, ipconfig, curl.exe.
Orsak: 172.22.80.1 är den virtuella WSL-adaptern (vEthernet (WSL Hyper-V firewall)) — oåtkomlig för en WiFi-ansluten ESP32. Korrekt rad i ipconfig är det fysiska kortet (Wireless LAN adapter Wi-Fi).
Åtgärd: Ändrade MQTT_HOST till LAN-IP:t 192.168.1.126, byggde och flashade om.
Verifiering: [MQTT] ANSLOTEN! i monitorn, ny klient i brokerloggen, total_messages började stiga.
Lärdom: Maskiner med Docker/WSL har flera nätverksidentiteter — peka IoT-enheter mot det fysiska kortets IP.

Fel 2: 5 GHz-SSID osynligt för 2,4 GHz-radion
Symptom: WiFi misslyckades gång på gång: E (1238) wifi: sta is connecting, return error, ingen IP.
Identifiering: Diagnosfirmware med WiFi.scanNetworks() listade Peter (kanal 13) men inte Peter5 som stod i koden.
Verktyg: Diagnosfirmware med WiFi.scanNetworks() och disconnect-event (reason=201, NO_AP_FOUND).
Orsak: Peter5 är routerns 5 GHz-nät; ESP32-C6 har bara 2,4 GHz-radio och kan fysiskt inte se det. Rätt SSID: Peter.
Åtgärd: Ändrade WIFI_SSID, byggde och flashade om.
Verifiering: WL_CONNECTED, IP 192.168.1.115, följt av MQTT-anslutning.
Lärdom: Syns inte SSID:t i scan-listan är det ett radio-/bandproblem, inte autentisering.

Fel 3: Watchdog-API-brott vid migrering till Arduino-core 3.x
Symptom: Kompileringsfel på esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true); efter lösning: runtime-felet esp_task_wdt_init(517): TWDT already initialized.
Identifiering: Byggfel i kompileringsutdata, runtime-fel i Serial Monitor. Projektet kör pioarduino-forken med Arduino-core 3.x (ESP-IDF 5.x).
Verktyg: pio run-utdata, Serial Monitor, Espressifs API-dokumentation.
Orsak: API-brott 2.x → 3.x: signaturen esp_task_wdt_init(timeout, panic) ersattes av struct-config, och i 3.x initierar kärnan TWDT själv — korrekt anrop är esp_task_wdt_reconfigure(&config).
Åtgärd: Skrev om till esp_task_wdt_config_t (timeout_ms, idle_core_mask, trigger_panic) med reconfigure().
Verifiering: Grönt bygge; [WDT] Timeout omkonfigurerat till 30 s vid varje boot, inga felrader.
Lärdom: Ramverksuppgraderingar ger både kompilerings- och semantiska API-brott — verifiera beteendet, inte bara att det kompilerar.

Fel 4: Diagnos-kod triggade watchdogen — reboot-loop
Symptom: Efter upload av diagnosfirmware bootade kortet om i oändlig slinga utan output.
Identifiering: Boot-logg visade watchdog-triggers + rst:0x1 (POWERON) i slinga — kortet dog i setup().
Verktyg: Serial Monitor (boot-rader + task_wdt-fel).
Orsak: Diagnoskoden blockerade (upp till 20 s vänteloop + ~5 s scan) utan att mata watchdogen (esp_task_wdt_reset()) medan timeouten var 10 s och tasken var TWDT-registrerad.
Åtgärd: La in esp_task_wdt_reset() i vänteloopen och höjde WDT_TIMEOUT_SECONDS till 30.
Verifiering: Kortet passerade setup, full output, ingen reboot-loop; därefter 800+ publicerade meddelanden utan watchdog-triggar.
Lärdom: Blockerande kod i en TWDT-registrerad task måste mata watchdogen — annars blir diagnosverktyget självt felet.

Fel 5: CDC_ON_BOOT skickade Serial till fel USB-port
Symptom: ROM-boot-rader syntes på COM7, men ingen applikationsutskrift alls — trots fungerande WiFi-anslutning.
Identifiering: Uploadens byte-räkning (~998 000 bytes) bevisade att ny firmware körde; pio device list visade COM7 = CP210x UART-brygga.
Verktyg: Serial Monitor, pio device list, upload-loggen.
Orsak: platformio.ini innehöll -DARDUINO_USB_CDC_ON_BOOT=1 — rätt för C3 SuperMini (native USB), fel för C6 DevKitM-1 (CP210x). Flaggan omdirigerade Serial till chipets native USB-CDC, som kabeln inte satt i. ROM-loggen går via UART0 och syntes därför ändå.
Åtgärd: Tog bort hela build_flags-blocket; Serial mappas nu till UART0 → CP210x → COM7.
Verifiering: === START === och all app-output inom ~3 s efter boot.
Lärdom: Build-flags är board-specifika. "ROM syns men app gör det inte" = fel Serial-mappning vid kortbyte.

Fel 6: COM-portspöken och avbruten anslutning
Symptom: (a) Could not open COM7, the port is busy vid upload; (b) FileNotFoundError — porten fanns inte; (c) DHT11 rapporterade NaN.
Identifiering: Mappade portarna (pio device list + Enhetshanteraren): COM1 = moderkort, COM3 = Bluetooth-spöke, COM7 = CP210x = kortet. Ur/in-test: dra ur kortet → COM7 försvinner. NaN-felet förklarades av att USB-sladden flyttats till annan enhet.
Verktyg: pio device list, Enhetshanteraren, Serial Monitor ([DHT] Ogiltig avläsning).
Orsak: Serial Monitor låser COM-porten som uploaden behöver; avbrutna sessioner lämnar porten halvdöd; fysisk anslutning bröts.
Åtgärd: Fast arbetsordning: Ctrl+C i monitorn före varje upload. Porten identifieras empiriskt, aldrig hårdkodat. Sensorkablaget säkrades.
Verifiering: Upload [SUCCESS] varje gång monitorn stängts först; DHT11 gav giltiga värden; 800+ meddelanden ackumulerades.
Lärdom: En serieport är en exklusiv resurs — fast arbetsordning eliminerar hela felklassen.

## Sammanfattning

| Fel | Kategori                     | Injicerad | Uppgiftens exempelfel             |
|-----|------------------------------|-----------|------------------------------------|
| 7   | Autentisering                | ✅        | ✅ "Felaktig autentisering"         |
| 8   | MQTT-topic                   | ✅        | ✅ "Fel MQTT-topic"                |
| 9   | Ogiltig JSON                 | ✅        | ✅ "Ogiltig JSON"                  |
| 1   | Nätverkskonfig               | —         | ✅ "Fel brokeradress"              |
| 2   | Radio/band                   | —         | — (unikt hårdvarufel)             |
| 3   | API-migration                | —         | — (unikt ramverksfel)             |
| 4   | Watchdog/blockering          | —         | — (unikt implementationfel)       |
| 5   | USB/Serial-mappning          | —         | — (unikt board-fel)               |
| 6   | Avbruten anslutning          | —         | ✅ "Avbruten nätverksanslutning"   |

Metod: loggar först, isolera en variabel i taget (scan, ur/in-test, topic-jämförelse), riktad åtgärd — och verifiera alltid varje fix med en ny mätning innan nästa steg.
