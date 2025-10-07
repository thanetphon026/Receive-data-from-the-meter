# Modbus Meter Slave Simulator (RTU over RS‑485)

**TH | ภาษาไทย** · [EN below](#english--en)

โปรแกรมนี้จำลอง **มิเตอร์น้ำ/ไฟฟ้า** แบบ **Modbus RTU Slave** ที่รันบนคอมพิวเตอร์ของคุณและต่อออกทาง **USB‑to‑RS485** เพื่อให้ **ESP32 (Master)** อ่านค่าได้เหมือนมิเตอร์จริง เหมาะสำหรับทดสอบระบบก่อนนำมิเตอร์จริงมาต่อ โดยเลย์เอาต์ระบบตัวอย่างคือ:

คอมพิวเตอร์ (รันโปรแกรมนี้) → USB‑RS485 → RS485↔TTL → ESP32 (Modbus Master) → Raspberry Pi 4 (MQTT)

---

## คุณสมบัติ
- เปิด **Modbus RTU Slave** บนพอร์ตอนุกรมที่กำหนด (COMx หรือ /dev/ttyUSBx)
- ตารางรีจิสเตอร์จำลองทั้ง **Input Registers (fc=0x04)** และ **Holding Registers (fc=0x03/0x06/0x10)**
- มีโหมดอัปเดตค่าจำลองอัตโนมัติ (random‑walk) ให้ค่าคล้ายอุปกรณ์จริง
- เลือกพารามิเตอร์ serial ได้: baudrate, parity, stopbits, bytesize
- โค้ดรองรับ `pymodbus 3.x` (ทดสอบบน Python 3.9–3.12)

---

## การติดตั้ง (Install)
1) ติดตั้ง Python (แนะนำ 3.9–3.12)  
2) ติดตั้งไลบรารี
```bash
pip install -r requirements.txt
```
> Windows: ตรวจสอบพอร์ตใน Device Manager (เช่น `COM5`)  
> Linux: มักจะเป็น `/dev/ttyUSB0` หรือ `/dev/ttyACM0` (เช็คด้วย `ls /dev/tty*`)

---

## การใช้งาน (Run)
ตัวอย่างคำสั่ง:
```bash
# Windows
python modbus_meter_slave.py --port COM5 --baud 9600 --unit 1

# Linux
python modbus_meter_slave.py --port /dev/ttyUSB0 --baud 9600 --unit 1
```
พารามิเตอร์หลัก:
- `--port` : พอร์ตอนุกรม (จำเป็นต้องระบุ)
- `--baud` : ความเร็วบอดเรต (เริ่มต้น 9600)
- `--parity` : N/E/O (เริ่มต้น N)
- `--stopbits` : 1/2 (เริ่มต้น 1)
- `--bytesize` : 7/8 (เริ่มต้น 8)
- `--unit` : Modbus Unit ID (ค่าเริ่ม 1 — ในโหมด single slave ฝั่ง master ใช้ 1 ได้เลย)
- `--no-auto` : ปิดการอัปเดตค่าจำลองอัตโนมัติ

หยุดโปรแกรม: กด `Ctrl + C`

---

## ตารางรีจิสเตอร์ (zero‑based addressing)
### Input Registers (อ่านอย่างเดียว – FC 0x04)
| ชื่อ | Addr | ความหมาย | หน่วย/สเกล |
|---|---:|---|---|
| Energy_Wh_H | 0 | พลังงานสะสม (Wh) – word สูง | จับคู่กับ addr=1 เป็น 32‑bit |
| Energy_Wh_L | 1 | พลังงานสะสม (Wh) – word ต่ำ | — |
| Water_Liter_H | 2 | น้ำสะสม (ลิตร) – word สูง | จับคู่กับ addr=3 เป็น 32‑bit |
| Water_Liter_L | 3 | น้ำสะสม (ลิตร) – word ต่ำ | — |
| Voltage_x10 | 4 | แรงดัน ×10 | 230.5V ⇒ 2305 |
| Current_x100 | 5 | กระแส ×100 | 1.23A ⇒ 123 |
| Power_W | 6 | กำลังไฟฟ้า (W) | 0–65535 |
| Flow_Lpm_x10 | 7 | อัตราการไหลน้ำ ×10 | 12.3 L/min ⇒ 123 |

### Holding Registers (อ่าน/เขียน – FC 0x03/0x06/0x10)
| ชื่อ | Addr | ความหมาย | หมายเหตุ |
|---|---:|---|---|
| Device_Serial_H | 100 | Serial 32‑bit – word สูง | จับคู่กับ 101 |
| Device_Serial_L | 101 | Serial 32‑bit – word ต่ำ | — |
| Firmware_Ver | 102 | เวอร์ชันเฟิร์มแวร์ ×100 | v1.23 ⇒ 123 |
| Flags | 103 | บิตสถานะ (bit0=OK) | เขียนได้ |
| Reset_Counters | 104 | เขียนค่า `0xA5A5` เพื่อรีเซ็ต Energy/Water | ระบบจะล้างกลับ 0 อัตโนมัติ |

---

## วิธีทดสอบร่วมกับ ESP32 (Master)
1. ต่อสาย RS‑485: USB‑RS485 (คอม) **A↔A**, **B↔B** → โมดูล RS485↔TTL → ESP32 (UART, ร่วม GND)  
2. ตั้งค่า ESP32: UART 9600 8N1, Unit ID = 1  
3. คำสั่งอ่านที่แนะนำ:  
   - FC=4, addr=0, len=8 → ได้ชุดค่าพื้นฐาน (พลังงาน/น้ำสะสม, แรงดัน, กระแส, กำลัง, อัตราการไหล)  
   - รวม 32‑bit: `EnergyWh = (IR[0]<<16)|IR[1]`, `WaterL = (IR[2]<<16)|IR[3]`  
   - สเกล: `Voltage=IR[4]/10.0`, `Current=IR[5]/100.0`, `Flow=IR[7]/10.0`  
4. ทดลองเขียน HR: เขียน `0xA5A5` ไป `addr=104` → เคาน์เตอร์รีเซ็ต

> เมื่อเปลี่ยนไปใช้ **มิเตอร์จริง** ให้ปรับ `baud/parity/stopbits/unit` ให้ตรงกับสเปกมิเตอร์ และแม็ประบบอ่านค่าใน ESP32 ตามเอกสารของยี่ห้อ/รุ่นนั้น ๆ

---

## แก้ปัญหาทั่วไป
- อ่านค่าไม่ได้: ตรวจสอบพอร์ต, สาย A/B, terminator 120Ω ปลายบัส, ตรงกับ fc/addr/Unit ID  
- ค่ากระโดด/คงที่เกินไป: ใช้ `--no-auto` หรือปรับช่วงสุ่มในฟังก์ชัน `_step()`  
- ต้องการ TCP: เปลี่ยนเป็น `StartTcpServer` (ดูเอกสาร pymodbus)

---

## License
MIT

---

## English · EN

This app simulates a **Modbus RTU meter** as a **Slave** running on your computer over **USB‑to‑RS485**, so your **ESP32 Master** can read realistic data before connecting a real meter. Typical wiring:

PC (this program) → USB‑RS485 → RS485↔TTL → ESP32 (Master) → Raspberry Pi 4 (MQTT)

### Features
- Modbus **RTU Slave** on a given serial port
- Emulated **Input Registers (0x04)** and **Holding Registers (0x03/0x06/0x10)**
- Auto‑update (random‑walk) to mimic live readings
- Configurable serial params: baudrate/parity/stopbits/bytesize
- Works with `pymodbus 3.x` (tested on Python 3.9–3.12)

### Install
```bash
pip install -r requirements.txt
```

### Run
```bash
# Windows
python modbus_meter_slave.py --port COM5 --baud 9600 --unit 1

# Linux
python modbus_meter_slave.py --port /dev/ttyUSB0 --baud 9600 --unit 1
```
Args:
- `--port` (required), `--baud`=9600, `--parity`=N/E/O (default N), `--stopbits`=1/2 (default 1), `--bytesize`=7/8 (default 8)  
- `--unit`=1 (in single‑slave mode, master can use 1)  
- `--no-auto` to freeze values

Stop with `Ctrl + C`.

### Registers (zero‑based)
**Input (0x04)**:  
- `0` Energy_Wh_H, `1` Energy_Wh_L (32‑bit)  
- `2` Water_Liter_H, `3` Water_Liter_L (32‑bit)  
- `4` Voltage_x10, `5` Current_x100, `6` Power_W, `7` Flow_Lpm_x10

**Holding (0x03)**:  
- `100` Serial_H, `101` Serial_L (32‑bit)  
- `102` Firmware_Ver×100, `103` Flags, `104` Reset (write 0xA5A5)

### ESP32 Master quick test
- UART 9600 8N1, Unit ID = 1  
- Read FC=4, addr=0, len=8 → combine/scale as documented  
- Write 0xA5A5 to HR addr 104 to reset counters

### Troubleshooting
- No data? Check port name, A/B wiring, 120Ω termination, proper FC/addr/unit.  
- Values too jumpy? Use `--no-auto` or tweak `_step()` ranges.  
- Need TCP? Switch to `StartTcpServer` in pymodbus.

### License
MIT
