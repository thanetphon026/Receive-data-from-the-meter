// esp32_modbus_mqtt.ino
#include <WiFi.h>
#include <PubSubClient.h>
#include <ModbusMaster.h>

// ====== Wi-Fi & MQTT ======
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASS";
const char* MQTT_HOST = "192.168.1.10";  // IP ของ Raspberry Pi
const uint16_t MQTT_PORT = 1883;
const char* MQTT_TOPIC  = "dorm/meters/room101";

// ====== RS-485 (UART2) ======
HardwareSerial RS485(2);
const int PIN_RX2   = 16;
const int PIN_TX2   = 17;
const int PIN_DE_RE = 4;   // HIGH=TX, LOW=RX

ModbusMaster node;   // Master
WiFiClient espClient;
PubSubClient mqtt(espClient);

void preTransmission()  { digitalWrite(PIN_DE_RE, HIGH); }
void postTransmission() { digitalWrite(PIN_DE_RE, LOW);  }

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
}

void setupMQTT() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  while (!mqtt.connected()) {
    mqtt.connect("esp32-meter");
    delay(500);
  }
}

void setup() {
  pinMode(PIN_DE_RE, OUTPUT);
  digitalWrite(PIN_DE_RE, LOW);  // receive by default

  Serial.begin(115200);
  RS485.begin(9600, SERIAL_8N1, PIN_RX2, PIN_TX2);

  node.begin(1, RS485);        // Slave ID = 1 (ต้องตรงกับ simulator/มิเตอร์จริง)
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  setupWiFi();
  setupMQTT();
}

void loop() {
  if (!mqtt.connected()) setupMQTT();

  // อ่าน HR ตั้งแต่ 0 ยาว 4 คำ (energy_wh, water_l, voltage_x10, current_x100)
  uint8_t rc = node.readHoldingRegisters(0, 4);
  if (rc == node.ku8MBSuccess) {
    uint16_t energy_wh    = node.getResponseBuffer(0);
    uint16_t water_l      = node.getResponseBuffer(1);
    uint16_t voltage_x10  = node.getResponseBuffer(2);
    uint16_t current_x100 = node.getResponseBuffer(3);

    // แปลงเป็น JSON
    char payload[160];
    float kwh   = energy_wh / 1000.0;  // ตัวอย่าง: แปลง Wh -> kWh
    float water = water_l   / 1000.0;  // L -> m^3
    float volt  = voltage_x10 / 10.0;
    float amp   = current_x100 / 100.0;

    snprintf(payload, sizeof(payload),
      "{\"kwh\":%.3f,\"water_m3\":%.3f,\"voltage\":%.1f,\"current\":%.2f}",
      kwh, water, volt, amp);

    mqtt.publish(MQTT_TOPIC, payload, true);
    Serial.println(payload);
  } else {
    Serial.printf("Modbus read error: %u\n", rc);
  }

  mqtt.loop();
  delay(1000);
}
