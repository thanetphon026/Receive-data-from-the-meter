# pc_modbus_sim.py  -- Python 3.x
# pip install pymodbus==3.* pyserial

import time
from threading import Thread
from pymodbus.datastore import ModbusSequentialDataBlock, ModbusSlaveContext, ModbusServerContext
from pymodbus.server import StartSerialServer
from pymodbus.transaction import ModbusRtuFramer

# Holding Registers map (ตัวอย่าง):
# HR[0] = Energy (Wh)   | HR[1] = Water (Liters)  | HR[2] = Voltage*10 | HR[3] = Current*100
store = ModbusSlaveContext(
    di=ModbusSequentialDataBlock(0, [0]*16),
    co=ModbusSequentialDataBlock(0, [0]*16),
    hr=ModbusSequentialDataBlock(0, [0]*64),
    ir=ModbusSequentialDataBlock(0, [0]*64),
)
context = ModbusServerContext(slaves=store, single=True)

def simulator():
    slave_id = 0x01
    hr_addr  = 0
    energy_wh = 12000   # 12.000 kWh
    water_l   = 3456    # 3.456 m^3
    voltage_x10 = 2300  # 230.0 V
    current_x100 = 157  # 1.57 A

    while True:
        energy_wh += 5     # +5 Wh ทุก 1 วินาที
        water_l   += 1     # +1 L  ทุก 1 วินาที
        store.setValues(3, 0,  [energy_wh, water_l, voltage_x10, current_x100])  # 3=HR
        time.sleep(1)

Thread(target=simulator, daemon=True).start()

StartSerialServer(
    context,
    framer=ModbusRtuFramer,
    port="COM5",        # Windows: "COM5" | Linux: "/dev/ttyUSB0"
    baudrate=9600,
    bytesize=8,
    parity='N',
    stopbits=1,
    timeout=1
)
