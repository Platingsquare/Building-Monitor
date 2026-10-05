# API-dokumentation

## Syfte

Backend-tjänsten exponerar sensordata via ett REST-API. Endast de senast
inspelade mätningarna är tillgängliga; historik är inte implementerad (planeras
i framtiden).

## Endpoints

| Metod | URL | Beskrivning | Autentisering |
|-------|-----|-------------|---------------|
| GET | `/api/health` | Hälsostatus + métrica | Ingen |
| GET | `/api/readings/latest` | Senaste mätningen | Bearer-token |

## Endpunktdetaljer

### Hälsokontroll

**GET** `/api/health`

Returnerar systemets hälsotillstånd samt métrica om mottagna meddelanden.

**Exempelrespons:**
```json
{
  "status": "healthy",
  "total_messages": 42,
  "seconds_since_last_reading": 5.2,
  "last_seq": 42
}