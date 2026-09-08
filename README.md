
# Building-Monitor

IoT pipeline for building environment monitoring — ESP32 + sensors → MQTT → REST API → database → Grafana dashboard.

## Overview
Skalbar IoT-lösning för miljöövervakning i fastigheter: en ESP32 samlar in sensordata (temperatur/luftfuktighet), publicerar den som JSON över MQTT till en egen broker, där en API-server lagrar datan i en databas och exponerar den via REST. Allt visualiseras i Grafana.

Byggt som projekt i kursen Nätverk och systemintegration (IoT25).

## Arkitektur