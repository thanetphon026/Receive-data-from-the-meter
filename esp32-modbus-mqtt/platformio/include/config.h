#pragma once

// ===== WiFi =====
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

// ===== MQTT =====
#define MQTT_HOST       "192.168.1.10"   // Raspberry Pi MQTT broker IP
#define MQTT_PORT       1883
#define MQTT_USERNAME   ""               // optional
#define MQTT_PASSWORD   ""               // optional
#define MQTT_CLIENT_ID  "esp32-meter-01"
#define MQTT_BASE_TOPIC "dorm/meter/esp32-meter-01"   // base topic

// ===== RS-485 / UART =====
// Use Serial2 on ESP32 (UART2). You can change pins as needed.
#define RS485_TX_PIN    17
#define RS485_RX_PIN    16
#define RS485_DE_RE_PIN 4    // DE & RE tied together to this GPIO (HIGH = transmit, LOW = receive)

#define MODBUS_BAUD     9600
// SERIAL_8N1, SERIAL_8E1, SERIAL_8O1, etc.
#define MODBUS_SERIALCFG SERIAL_8N1

// ===== Modbus parameters =====
#define MODBUS_UNIT_ID  1   // Unit ID (slave address)

// Input Registers (zero-based) mapping for the simulator
#define IR_ADDR_START           0
#define IR_COUNT                8
#define IR_ENERGY_H             0
#define IR_ENERGY_L             1
#define IR_WATER_H              2
#define IR_WATER_L              3
#define IR_VOLT_X10             4
#define IR_CURR_X100            5
#define IR_POWER_W              6
#define IR_FLOW_X10             7

// Holding Registers used (optional)
#define HR_SERIAL_H             100
#define HR_SERIAL_L             101
#define HR_FW_VER               102
#define HR_FLAGS                103
#define HR_RESET                104
