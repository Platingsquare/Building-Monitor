// ESP32 climate node — publishes DHT11 telemetry as JSON over MQTT.
// Dependencies (Arduino Library Manager): PubSubClient, ArduinoJson, DHT sensor library
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <esp_task_wdt.h>

#define WDT_TIMEOUT_SECONDS 10
#define DHTPIN 4
#define DHTTYPE DHT11

// --- Credentials: keep out of git. Either flash secrets.h separately
// or (simpler for school) fill in locally and NEVER commit the change.
const char* WIFI_SSID = "YOUR_WIFI";
const char* WIFI_PASS = "YOUR_PASS";
const char* MQTT_HOST = "192.168.1.X"; // Your PC's LAN IP (run `ipconfig`)
const int   MQTT_PORT = 1883;
const char* MQTT_TOPIC = "building/telemetry";
const char* MQTT_CLIENT_ID = "ESP32_Climate_Node_01";

DHT dht(DHTPIN, DHTTYPE);
WiFiClient netClient; // Switch to WiFiClientSecure for TLS later
PubSubClient mqtt(netClient);

unsigned long lastSampleTime = 0;
const unsigned long SAMPLE_INTERVAL_MS = 5000;
unsigned long messageCounter = 0;

unsigned long lastWifiRetry = 0;
unsigned long wifiBackoff = 1000;
const unsigned long MAX_BACKOFF = 60000;

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiBackoff = 1000;
    return;
  }
  unsigned long now = millis();
  if (now - lastWifiRetry > wifiBackoff) {
    lastWifiRetry = now;
    Serial.printf("[WIFI] Reconnecting... backoff: %lu ms\n", wifiBackoff);
    WiFi.disconnect();
    WiFi.reconnect();
    wifiBackoff = min(wifiBackoff * 2, MAX_BACKOFF);
  }
}

void ensureMqttConnected() {
  if (WiFi.status() != WL_CONNECTED || mqtt.connected()) return;
  if (mqtt.connect(MQTT_CLIENT_ID)) {
    Serial.println("[MQTT] Connected!");
  }
}

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
  mqtt.publish(MQTT_TOPIC, buffer);
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
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

    if (!isnan(h) && !isnan(t) && mqtt.connected()) {
      publishTelemetry(t, h);
    }
  }
}
