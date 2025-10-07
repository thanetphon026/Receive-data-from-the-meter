#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ModbusMaster.h>
#include "config.h"

// ===== RS485 DE/RE control =====
void preTransmission();
void postTransmission();

ModbusMaster node;
WiFiClient espClient;
PubSubClient mqtt(espClient);

unsigned long lastPoll = 0;
const unsigned long POLL_INTERVAL_MS = 1000;

uint16_t irBuf[IR_COUNT];
bool mqttConnectedOnce = false;

String baseTopic = String(MQTT_BASE_TOPIC);
String topicState = baseTopic + "/state";          // online/offline
String topicData  = baseTopic + "/telemetry";      // JSON payload
String topicAttrs = baseTopic + "/attributes";     // device attributes

// Helpers
static inline uint32_t u32_from_words(uint16_t hi, uint16_t lo) {
  return ((uint32_t)hi << 16) | (uint32_t)lo;
}

void logAttributesOnce() {
  StaticJsonDocument<256> doc;
  // We'll construct JSON manually to avoid ArduinoJson dependency
  String payload = "{";
  payload += "\"client_id\":\"" + String(MQTT_CLIENT_ID) + "\",";
  payload += "\"unit_id\":" + String(MODBUS_UNIT_ID) + ",";
  payload += "\"baud\":" + String(MODBUS_BAUD) + ",";
  payload += "\"pins\":{\"tx\":" + String(RS485_TX_PIN) + ",\"rx\":" + String(RS485_RX_PIN) + ",\"de_re\":" + String(RS485_DE_RE_PIN) + "}";
  payload += "}";

  mqtt.publish(topicAttrs.c_str(), payload.c_str(), true);
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("[WiFi] Connected, IP: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  while (!mqtt.connected()) {
    Serial.print("[MQTT] Connecting...");
    String clientId = MQTT_CLIENT_ID;
    bool ok;
    if (String(MQTT_USERNAME).length() > 0) {
      ok = mqtt.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD, topicState.c_str(), 1, true, "offline");
    } else {
      ok = mqtt.connect(clientId.c_str(), NULL, NULL, topicState.c_str(), 1, true, "offline");
    }
    if (ok) {
      Serial.println("connected");
      mqtt.publish(topicState.c_str(), "online", true);
      if (!mqttConnectedOnce) {
        logAttributesOnce();
        mqttConnectedOnce = true;
      }
    } else {
      Serial.print("failed, rc=");
      Serial.println(mqtt.state());
      delay(1000);
    }
  }
}

void setup() {
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, LOW); // receive by default

  Serial.begin(115200);
  delay(200);
  Serial.println("\nESP32 Modbus RTU Master + MQTT");

  // RS485 UART
  Serial2.begin(MODBUS_BAUD, MODBUS_SERIALCFG, RS485_RX_PIN, RS485_TX_PIN);

  node.begin(MODBUS_UNIT_ID, Serial2);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  connectWiFi();
  connectMQTT();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  if (!mqtt.connected()) {
    connectMQTT();
  }
  mqtt.loop();

  unsigned long now = millis();
  if (now - lastPoll >= POLL_INTERVAL_MS) {
    lastPoll = now;

    // Read IR block
    uint8_t result = node.readInputRegisters(IR_ADDR_START, IR_COUNT);
    if (result == node.ku8MBSuccess) {
      for (int i=0; i<IR_COUNT; ++i) {
        irBuf[i] = node.getResponseBuffer(i);
      }

      // Parse & scale
      uint32_t energyWh = u32_from_words(irBuf[IR_ENERGY_H], irBuf[IR_ENERGY_L]);
      uint32_t waterL   = u32_from_words(irBuf[IR_WATER_H], irBuf[IR_WATER_L]);
      float voltage     = irBuf[IR_VOLT_X10] / 10.0f;
      float current     = irBuf[IR_CURR_X100] / 100.0f;
      uint16_t powerW   = irBuf[IR_POWER_W];
      float flowLpm     = irBuf[IR_FLOW_X10] / 10.0f;

      // Build JSON manually
      String payload = "{";
      payload += "\"ts\":" + String((uint32_t)(millis()/1000)) + ",";
      payload += "\"energy_Wh\":" + String(energyWh) + ",";
      payload += "\"water_L\":" + String(waterL) + ",";
      payload += "\"voltage_V\":" + String(voltage, 1) + ",";
      payload += "\"current_A\":" + String(current, 2) + ",";
      payload += "\"power_W\":" + String(powerW) + ",";
      payload += "\"flow_Lpm\":" + String(flowLpm, 1);
      payload += "}";

      mqtt.publish(topicData.c_str(), payload.c_str(), false);
      Serial.println(payload);

    } else {
      Serial.print("[Modbus] Read IR failed, code=");
      Serial.println(result);
    }

    // Example: optional read of HR (device info) could be added if needed
    // uint8_t r2 = node.readHoldingRegisters(HR_SERIAL_H, 5);
  }
}

void preTransmission() {
  digitalWrite(RS485_DE_RE_PIN, HIGH);
  delayMicroseconds(10);
}

void postTransmission() {
  delayMicroseconds(10);
  digitalWrite(RS485_DE_RE_PIN, LOW);
}
