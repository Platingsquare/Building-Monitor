# Säkerhet

## Genomförda säkerhetsåtgärder

1. **Token-baserad autentisering** – REST-API:t kräver en Bearer-token på
   `/api/readings/*`. Token konfigureras via miljövariabeln `APP_TOKEN`
   och skickas i headern `Authorization: Bearer <token>`.

2. **Validering av inkommande data** – Alla MQTT-payloads valideras mot en
   Pydantic-modell (`TelemetryPayload`). Ogiltig data loggas men sparas
   aldrig i databasen (validation at boundaries).

3. **Hemligheter utanför versionshanteringen** – Token och
   miljökonfiguration ligger i `.env`, som ignoreras av git via
   `.gitignore`. `.env.example` committas som mall utan verkliga värden.

## Riskanalys

| Risk | Åtgärd | Kvarvarande begränsning |
|------|--------|-------------------------|
| Avlyssning av MQTT-trafik | Ingen (TLS planerad på port 8883) | MQTT körs utan kryptering i labbmiljön |
| Obehörig åtkomst till API | Bearer-token krävs | Ingen central användarhantering (OAuth) |
| Förfalskade sensordata | Schema-validering i backend | Ingen autentisering av själva sensorenheten |
| Läckta hemligheter i git | `.env` i `.gitignore` | Skyddar inte mot manuellt misstag |
| SQL-injection | Parameteriserade frågor (SQLite) | – |

## Hantering av känslig konfiguration

- Wi-Fi-uppgifter (SSID/lösenord) för ESP32 ligger i firmware-konfiguration
  och committas inte till repot
- `APP_TOKEN` genereras slumpmässigt (`secrets.token_hex`) och lagras i `.env`
- `.env`-filen återfinns endast lokalt på utvecklingsmaskinen