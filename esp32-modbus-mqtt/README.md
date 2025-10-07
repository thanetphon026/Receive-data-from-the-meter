# ESP32 Modbus RTU Master → MQTT

**TH | ภาษาไทย** · [EN below](#english--en)

โค้ดนี้ทำให้ ESP32 อ่านค่าจาก **มิเตอร์ Modbus RTU (ผ่าน RS-485)** หรือจาก **โปรแกรมจำลองมิเตอร์** ที่คุณรันบนคอม แล้ว **ส่งข้อมูลขึ้น MQTT** (ไปยัง Raspberry Pi 4 หรือโบรคเกอร์ใด ๆ) เป็น JSON

โครง: คอม (Modbus Slave จำลอง) → USB‑RS485 → RS485↔TTL → ESP32 (Master) → MQTT Broker (Raspberry Pi 4)

---

## การเตรียมฮาร์ดแวร์
- ESP32 (เช่น DOIT ESP32 DEVKIT V1)
- โมดูล RS485↔TTL (เช่น MAX485) ต่อดังนี้
  - RS485 A/B ↔ สายบัส A/B
  - RO → ESP32 RX (ค่าเริ่ม: GPIO16)
  - DI → ESP32 TX (ค่าเริ่ม: GPIO17)
  - DE และ RE **ผูกเข้าด้วยกัน** → ESP32 (ค่าเริ่ม: GPIO4)
  - ร่วม GND ระหว่างอุปกรณ์ทุกตัว
- ปลายบัสใส่ตัวต้านทาน **120Ω** (กรณีสายยาว/มีสัญญาณก้อง)

> ถ้าคุณใช้โปรแกรมจำลอง (จากโปรเจกต์ `modbus-meter-sim.zip`) ให้ตั้งค่า serial/baud/UnitID ในสองฝั่งให้ตรงกัน

---

## ติดตั้งไลบรารี
### ตัวเลือก A: PlatformIO (แนะนำ)
1. ติดตั้ง VS Code + ส่วนขยาย PlatformIO
2. เปิดโฟลเดอร์ `platformio/`
3. ไลบรารีจะถูกติดตั้งอัตโนมัติจาก `platformio.ini`:
   - `ModbusMaster`
   - `PubSubClient`
4. แก้ค่าใน `include/config.h` ให้ตรงเครือข่าย/พิน/พอร์ต
5. Upload ไปยัง ESP32

### ตัวเลือก B: Arduino IDE
1. ติดตั้ง **ESP32 boards** ใน Boards Manager
2. ติดตั้งไลบรารี: **ModbusMaster**, **PubSubClient**
3. เปิดสเก็ตช์ `arduino/esp32_modbus_mqtt/esp32_modbus_mqtt.ino`
4. แก้ค่าที่ส่วน `USER CONFIG`
5. อัพโหลดสเก็ตช์

---

## การตั้งค่า (สำคัญ)
ไฟล์ `platformio/include/config.h` (หรือค่าที่หัวสเก็ตช์ Arduino):
- WiFi: `WIFI_SSID`, `WIFI_PASSWORD`
- MQTT: `MQTT_HOST`, `MQTT_PORT`, (ถ้ามี) `MQTT_USERNAME`, `MQTT_PASSWORD`, `MQTT_BASE_TOPIC`
- RS‑485:
  - `RS485_TX_PIN` (เริ่ม 17), `RS485_RX_PIN` (เริ่ม 16), `RS485_DE_RE_PIN` (เริ่ม 4)
  - `MODBUS_BAUD` (เริ่ม 9600), `MODBUS_SERIALCFG` (เริ่ม `SERIAL_8N1`)
  - `MODBUS_UNIT_ID` (เริ่ม 1) ให้ตรงกับมิเตอร์/โปรแกรมจำลอง
- Mapping รีจิสเตอร์: ถูกตั้งให้ตรงกับโปรแกรมจำลอง (IR addr 0..7)

> ถ้าเป็น **มิเตอร์จริง**: ให้ปรับ `MODBUS_BAUD`, `MODBUS_SERIALCFG`, `MODBUS_UNIT_ID`, และ **ที่อยู่รีจิสเตอร์** ตามเอกสารของผู้ผลิต

---

## รูปแบบข้อมูล MQTT
- LWT/สถานะ: `<baseTopic>/state` → `"online"` / `"offline"`
- ข้อมูลเทเลเมทรี: `<baseTopic>/telemetry` → JSON เช่น
```json
{
  "ts": 1234,
  "energy_Wh": 12345,
  "water_L": 67890,
  "voltage_V": 230.5,
  "current_A": 1.23,
  "power_W": 284,
  "flow_Lpm": 8.4
}
```
- คุณสมบัติอุปกรณ์: `<baseTopic>/attributes` (retain) → JSON พารามิเตอร์หลัก

---

## วิธีกำหนดให้ใช้กับมิเตอร์จริง
1. เปิดคู่มือมิเตอร์ หาหัวข้อ **Modbus Map** ระบุที่อยู่รีจิสเตอร์/สเกล/ยูนิต
2. เปลี่ยนค่าคงที่ใน `config.h` (หรือในสเก็ตช์ Arduino):
   - เปลี่ยน `IR_ADDR_START`, `IR_COUNT` และแม็พ index ที่อ่าน
   - อัปเดตการสเกล (เช่น x10/x100) ในโค้ดตอนคำนวณ
3. ปรับ `MODBUS_UNIT_ID` ให้ตรงกับที่ตั้งในมิเตอร์
4. ปรับ `MODBUS_BAUD` และ `MODBUS_SERIALCFG` ให้ตรง เช่น `SERIAL_8E1`

---

## การดีบัก
- เปิด Serial Monitor ที่ 115200
- ถ้าอ่านไม่ได้:
  - เช็ค A/B สลับไหม, DE/RE ควบคุมถูกไหม
  - ใช้ terminator 120Ω
  - baud/parity/stopbits/UnitID ตรงกันหรือไม่
  - มีอุปกรณ์อื่นจับพอร์ต RS485 อยู่หรือเปล่า

---

## License
MIT

---

## English · EN

This ESP32 app reads **Modbus RTU** data from a real meter or the **PC simulator** and publishes JSON to **MQTT** (e.g., a Raspberry Pi broker).

### Hardware
- ESP32 dev board
- RS485↔TTL module (MAX485 or similar)
  - RO → ESP32 RX (default GPIO16)
  - DI → ESP32 TX (default GPIO17)
  - DE & RE tied → GPIO4 (TX enable)
  - Common GND
  - Optional 120Ω termination

### Libraries
- PlatformIO: open `platformio/` (libs auto-installed from `platformio.ini`)
- Arduino IDE: install **ModbusMaster** and **PubSubClient**

### Configure
`include/config.h` (PlatformIO) or the `USER CONFIG` block (Arduino):
- WiFi SSID/Password
- MQTT broker host/port/credentials, `MQTT_BASE_TOPIC`
- RS485 pins (`TX=17`, `RX=16`, `DE/RE=4`), baud (9600), serial config (`SERIAL_8N1`)
- Modbus unit id (1)
- Register map (defaults to simulator IR 0..7)

### MQTT Topics
- `<baseTopic>/state` (LWT): `"online"`/`"offline"`
- `<baseTopic>/telemetry` (JSON)
- `<baseTopic>/attributes` (device params, retained)

### Switching to a real meter
- Update Modbus map, scaling, unit id, baud/parity/stopbits to match vendor docs.
- Adjust `IR_ADDR_START`, `IR_COUNT`, and field indices.
- Rebuild & flash.

### Troubleshooting
- Check A/B wiring, DE/RE control, termination resistor.
- Ensure matching serial params and unit id.
- Monitor serial logs at 115200.

### License
MIT
