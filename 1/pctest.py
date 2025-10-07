# pc_modbus_sim.py  -- Python 3.x
# แนะนำ: ใช้ venv แล้วติดตั้ง
#   pip install "pymodbus==2.5.3" pyserial
# หรือถ้าจะใช้ 3.x:
#   pip install "pymodbus==3.*" pyserial

import os
import time
import logging
import platform
from threading import Thread

# ---- pymodbus imports (รองรับทั้ง 2.5.x / 3.x) ----
from pymodbus.datastore import (
    ModbusSequentialDataBlock, ModbusSlaveContext, ModbusServerContext
)

# StartSerialServer: 3.x = pymodbus.server, 2.5.x = pymodbus.server.sync
try:
    from pymodbus.server import StartSerialServer          # 3.x
except Exception:  # ImportError / AttributeError
    from pymodbus.server.sync import StartSerialServer     # 2.5.x

# ModbusRtuFramer: 3.x = pymodbus.framer.rtu_framer, 2.5.x = pymodbus.transaction
try:
    from pymodbus.framer.rtu_framer import ModbusRtuFramer  # 3.x
except Exception:
    from pymodbus.transaction import ModbusRtuFramer        # 2.5.x

# -------------------------------------------------------

logging.basicConfig(level=logging.INFO)
log = logging.getLogger("pc_modbus_sim")

# พอร์ตเริ่มต้น: Windows="COM5" | Linux/Raspberry Pi="/dev/ttyUSB0" | macOS="/dev/tty.usbserial-XXXX"
DEFAULT_PORT = "COM5" if platform.system() == "Windows" else "/dev/ttyUSB0"
PORT = os.getenv("MODBUS_PORT", DEFAULT_PORT)

# Holding Registers map (ตัวอย่าง):
# HR[0] = Energy (Wh)
# HR[1] = Water (Liters)
# HR[2] = Voltage*10   (เช่น 230.0V => 2300)
# HR[3] = Current*100  (เช่น 1.57A  => 157)

store = ModbusSlaveContext(
    di=ModbusSequentialDataBlock(0, [0] * 16),
    co=ModbusSequentialDataBlock(0, [0] * 16),
    hr=ModbusSequentialDataBlock(0, [0] * 64),
    ir=ModbusSequentialDataBlock(0, [0] * 64),
    zero_mode=True,  # ให้อ้างแอดเดรสเริ่มที่ 0
)
context = ModbusServerContext(slaves=store, single=True)

def simulator():
    energy_wh     = 12000   # 12.000 kWh
    water_l       = 3456    # 3.456 m^3
    voltage_x10   = 2300    # 230.0 V
    current_x100  = 157     # 1.57 A
    while True:
        energy_wh    += 5     # +5 Wh ทุก 1 วินาที
        water_l      += 1     # +1 L  ทุก 1 วินาที
        # 3 = Holding Registers (FC=3/6)
        store.setValues(3, 0, [energy_wh, water_l, voltage_x10, current_x100])
        time.sleep(1)

if __name__ == "__main__":
    Thread(target=simulator, daemon=True).start()
    log.info("Starting Modbus RTU server on port %s ...", PORT)

    StartSerialServer(
        context=context,
        framer=ModbusRtuFramer,
        port=PORT,           # Windows: "COMx" | Linux: "/dev/ttyUSB0" | macOS: "/dev/tty.usbserial-XXXX"
        baudrate=9600,
        bytesize=8,
        parity="N",
        stopbits=1,
        timeout=1,
    )
