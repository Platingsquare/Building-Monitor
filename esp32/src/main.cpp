// ESP32 climate node — publishes DHT11 telemetry as JSON over MQTT.
// Production build: WiFi "Peter" (2.4 GHz), MQTT to LAN broker, DHT11 sampling.
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <esp_task_wdt.h>

#define WDT_TIMEOUT_SECONDS 30
#define DHTPIN 4
#define DHTTYPE DHT11

// --- Credentials: keep out of git. Fill in locally and NEVER commit the change.
const char* WIFI_SSID = "DITT_SSID";// 2.4 GHz network — ESP32 cannot see 5 GHz
const char* WIFI_PASS = "DITT_LOSENORD";// Your WiFi password
const char* MQTT_HOST = "DIN_DATORS_IP";// Wireless LAN adapter Wi-Fi IPv4 (ipconfig)
const int   MQTT_PORT = 1883;
const char* MQTT_TOPIC = "building/telemetry_X"; // AVSIKTLIGT FEL: topic matchar inte backendens prenumeration
const char* MQTT_CLIENT_ID = "ESP32_Climate_Node_01";

DHT dht(DHTPIN, DHTTYPE);
WiFiClient netClient; // Switch to WiFiClientSecure for TLS later
PubSubClient mqtt(netClient);

unsigned long lastSampleTime = 0;
const unsigned long SAMPLE_INTERVAL_MS = 5000;
unsigned long messageCounter = 0;

unsigned long lastWifiRetry = 0;
unsigned long lastMqttRetry = 0;

// ---------------- WiFi ----------------

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  Serial.printf("[WIFI] Ansluter till '%s'...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Vänta max 30 sekunder — mata WDT medan vi väntar (annars triggar watchdogen)
  unsigned long timeout = millis() + 30000;
  while (WiFi.status() != WL_CONNECTED && millis() < timeout) {
    esp_task_wdt_reset();
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WIFI] ANSLUTEN! IP: %s | RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    // WL_CONNECTED=3, WL_NO_SSID_AVAIL=1, WL_CONNECT_FAILED=4, WL_DISCONNECTED=6
    Serial.printf("[WIFI] Misslyckades — status=%d. Retry-loopen i loop() tar över.\n", WiFi.status());
  }
}

void ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastWifiRetry < 10000) return;  // max ett försök var 10:e sekund
  lastWifiRetry = millis();
  Serial.printf("[WIFI] Retry... status=%d\n", WiFi.status());
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

// ---------------- MQTT ----------------

void ensureMqttConnected() {
  if (WiFi.status() != WL_CONNECTED || mqtt.connected()) return;
  if (millis() - lastMqttRetry < 5000) return;  // max ett försök var 5:e sekund
  lastMqttRetry = millis();
  Serial.printf("[MQTT] Försöker ansluta till %s:%d ... ", MQTT_HOST, MQTT_PORT);
  if (mqtt.connect(MQTT_CLIENT_ID)) {
    Serial.println("ANSLOTEN!");
  } else {
    // PubSubClient states: -2 = server refused, -4 = timeout/unreachable host
    Serial.printf("MISSLYCKADES — state=%d\n", mqtt.state());
  }
}

// ---------------- Telemetry ----------------

void publishTelemetry(float temp, float humidity) {
  JsonDocument doc;
  doc["device_id"] = MQTT_CLIENT_ID;
  doc["seq"] = ++messageCounter;
  doc["uptime_s"] = millis() / 1000;

  JsonObject metrics = doc["metrics"].to<JsonObject>();
  metrics["temperature"] = serialized(String(temp, 1));
  metrics["humidity"] = serialized(String(humidity, 1));

  char buffer[256];
  serializeJson(doc, buffer);
  bool ok = mqtt.publish(MQTT_TOPIC, buffer);
  Serial.printf("[MQTT] Publicerar seq=%lu %s\n", messageCounter, ok ? "(ok)" : "(PUBLISH MISSLYCKADES!)");
}

// ---------------- Setup / Loop ----------------

void setup() {
  Serial.begin(115200);
  dht.begin();
  delay(2000);  // ge monitorn tid att hänga med

  // Arduino ESP32 v3.x: coren initierar TWDT själv vid boot — vi omkonfigurerar
  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = (WDT_TIMEOUT_SECONDS * 1000),
      .idle_core_mask = 0,
      .trigger_panic = true,
  };
  esp_err_t wdt_err = esp_task_wdt_reconfigure(&wdt_config);
  if (wdt_err == ESP_OK) {
    Serial.printf("[WDT] Timeout omkonfigurerat till %d s\n", WDT_TIMEOUT_SECONDS);
  } else {
    Serial.printf("[WDT] Kunde inte omkonfigurera: %d\n", wdt_err);
  }
  esp_task_wdt_add(NULL);

  setupWiFi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
}

void loop() {
  esp_task_wdt_reset();
  ensureWiFiConnected();

  if (WiFi.status() == WL_CONNECTED) {
    ensureMqttConnected();
    mqtt.loop();
  }

  unsigned long now = millis();
  if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    lastSampleTime = now;
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
      Serial.println("[DHT] Ogiltig avläsning, hoppar över");
    } else if (mqtt.connected()) {
      publishTelemetry(t, h);
    }
  }
}
