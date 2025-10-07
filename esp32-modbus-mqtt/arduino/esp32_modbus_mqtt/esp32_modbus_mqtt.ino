#include <WiFi.h>
#include <PubSubClient.h>
#include <ModbusMaster.h>

// ====== USER CONFIG ======
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const char* MQTT_HOST = "192.168.1.10";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USERNAME = "";
const char* MQTT_PASSWORD = "";
const char* MQTT_CLIENT_ID = "esp32-meter-01";
const char* MQTT_BASE_TOPIC = "dorm/meter/esp32-meter-01";

// Pins
#define RS485_TX_PIN 17
#define RS485_RX_PIN 16
#define RS485_DE_RE_PIN 4

#define MODBUS_BAUD 9600
#define MODBUS_SERIALCFG SERIAL_8N1
#define MODBUS_UNIT_ID 1

// IR map
#define IR_ADDR_START 0
#define IR_COUNT 8
#define IR_ENERGY_H 0
#define IR_ENERGY_L 1
#define IR_WATER_H 2
#define IR_WATER_L 3
#define IR_VOLT_X10 4
#define IR_CURR_X100 5
#define IR_POWER_W 6
#define IR_FLOW_X10 7

// =========================

ModbusMaster node;
WiFiClient espClient;
PubSubClient mqtt(espClient);
uint16_t irBuf[IR_COUNT];
unsigned long lastPoll = 0;
const unsigned long POLL_INTERVAL_MS = 1000;

String baseTopic = String(MQTT_BASE_TOPIC);
String topicState = baseTopic + "/state";
String topicData  = baseTopic + "/telemetry";
String topicAttrs = baseTopic + "/attributes";

uint32_t u32_from_words(uint16_t hi, uint16_t lo) {
  return ((uint32_t)hi << 16) | (uint32_t)lo;
}

void preTransmission() { digitalWrite(RS485_DE_RE_PIN, HIGH); delayMicroseconds(10); }
void postTransmission(){ delayMicroseconds(10); digitalWrite(RS485_DE_RE_PIN, LOW); }

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(); Serial.print("[WiFi] IP: "); Serial.println(WiFi.localIP());
}

void connectMQTT() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  while (!mqtt.connected()) {
    Serial.print("[MQTT] Connecting...");
    bool ok;
    if (strlen(MQTT_USERNAME)) ok = mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD, topicState.c_str(), 1, true, "offline");
    else ok = mqtt.connect(MQTT_CLIENT_ID, NULL, NULL, topicState.c_str(), 1, true, "offline");
    if (ok) { Serial.println("connected"); mqtt.publish(topicState.c_str(), "online", true); }
    else { Serial.print("failed rc="); Serial.println(mqtt.state()); delay(1000); }
  }
}

void setup() {
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, LOW);
  Serial.begin(115200);
  Serial2.begin(MODBUS_BAUD, MODBUS_SERIALCFG, RS485_RX_PIN, RS485_TX_PIN);
  node.begin(MODBUS_UNIT_ID, Serial2);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
  connectWiFi();
  connectMQTT();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();

  unsigned long now = millis();
  if (now - lastPoll >= POLL_INTERVAL_MS) {
    lastPoll = now;
    uint8_t result = node.readInputRegisters(IR_ADDR_START, IR_COUNT);
    if (result == node.ku8MBSuccess) {
      for (int i=0;i<IR_COUNT;i++) irBuf[i] = node.getResponseBuffer(i);
      uint32_t energyWh = u32_from_words(irBuf[IR_ENERGY_H], irBuf[IR_ENERGY_L]);
      uint32_t waterL   = u32_from_words(irBuf[IR_WATER_H], irBuf[IR_WATER_L]);
      float voltage     = irBuf[IR_VOLT_X10] / 10.0f;
      float current     = irBuf[IR_CURR_X100] / 100.0f;
      uint16_t powerW   = irBuf[IR_POWER_W];
      float flowLpm     = irBuf[IR_FLOW_X10] / 10.0f;

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
      Serial.print("[Modbus] read fail code="); Serial.println(result);
    }
  }
}
