#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Modbus RTU Slave: Meter Simulator for RS-485
- Python 3.x
- pip install "pymodbus==3.*" pyserial

Run:
  python modbus_meter_slave.py --port COM5 --baud 9600 --unit 1
  python modbus_meter_slave.py --port /dev/ttyUSB0 --baud 9600 --unit 1
"""

import argparse
import logging
import threading
import time
import random

from pymodbus.datastore import (
    ModbusSlaveContext,
    ModbusServerContext,
    ModbusSequentialDataBlock,
)
from pymodbus.server import StartSerialServer
from pymodbus.transaction import ModbusRtuFramer


logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
)
log = logging.getLogger("meter-sim")


def u32_to_words(value: int):
    value &= 0xFFFFFFFF
    hi = (value >> 16) & 0xFFFF
    lo = value & 0xFFFF
    return hi, lo


def words_to_u32(hi: int, lo: int) -> int:
    return ((hi & 0xFFFF) << 16) | (lo & 0xFFFF)


class MeterModel:
    # Input Registers base addresses (0-based)
    IR_ENERGY_H = 0
    IR_ENERGY_L = 1
    IR_WATER_H = 2
    IR_WATER_L = 3
    IR_VOLT_X10 = 4
    IR_CURR_X100 = 5
    IR_POWER_W = 6
    IR_FLOW_X10 = 7

    # Holding Registers base addresses
    HR_SERIAL_H = 100
    HR_SERIAL_L = 101
    HR_FW_VER   = 102
    HR_FLAGS    = 103
    HR_RESET    = 104

    def __init__(self, context: ModbusServerContext, unit_id: int, auto_update: bool = True):
        self.context = context
        self.unit_id = unit_id
        self.auto_update = auto_update

        # State (internal units)
        self.energy_Wh = 12_345
        self.water_L = 67_890
        self.voltage_V = 230.5
        self.current_A = 1.23
        self.power_W = 284
        self.flow_Lpm = 8.4

        self.serial_u32 = 0x0123_4567
        self.fw_ver_x100 = 123  # v1.23
        self.flags = 0x0001     # bit0 = OK

        self._write_snapshot()
        self._stop = threading.Event()
        self._thread = None

    def _get_store(self):
        return self.context[self.unit_id]

    def _set_ir(self, addr: int, value: int):
        self._get_store().setValues(4, addr, [value & 0xFFFF])

    def _set_hr(self, addr: int, value: int):
        self._get_store().setValues(3, addr, [value & 0xFFFF])

    def _get_hr(self, addr: int) -> int:
        return self._get_store().getValues(3, addr, count=1)[0]

    def _write_snapshot(self):
        e_hi, e_lo = u32_to_words(self.energy_Wh)
        w_hi, w_lo = u32_to_words(self.water_L)
        self._set_ir(self.IR_ENERGY_H, e_hi)
        self._set_ir(self.IR_ENERGY_L, e_lo)
        self._set_ir(self.IR_WATER_H,  w_hi)
        self._set_ir(self.IR_WATER_L,  w_lo)

        self._set_ir(self.IR_VOLT_X10, int(round(self.voltage_V * 10)))
        self._set_ir(self.IR_CURR_X100, int(round(self.current_A * 100)))
        self._set_ir(self.IR_POWER_W, int(self.power_W) & 0xFFFF)
        self._set_ir(self.IR_FLOW_X10, int(round(self.flow_Lpm * 10)))

        s_hi, s_lo = u32_to_words(self.serial_u32)
        self._set_hr(self.HR_SERIAL_H, s_hi)
        self._set_hr(self.HR_SERIAL_L, s_lo)
        self._set_hr(self.HR_FW_VER,   self.fw_ver_x100)
        self._set_hr(self.HR_FLAGS,    self.flags)
        # HR_RESET handled in _handle_hr_writes

    def _handle_hr_writes(self):
        cmd = self._get_hr(self.HR_RESET)
        if cmd == 0xA5A5:
            self.energy_Wh = 0
            self.water_L = 0
            self._write_snapshot()
            self._set_hr(self.HR_RESET, 0x0000)

    def start(self):
        if not self.auto_update:
            log.info("Auto-update disabled.")
            return
        if self._thread and self._thread.is_alive():
            return
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()
        log.info("Auto-update thread started.")

    def stop(self):
        self._stop.set()
        if self._thread:
            self._thread.join(timeout=2.0)

    def _run(self):
        next_t = time.time()
        while not self._stop.is_set():
            now = time.time()
            if now >= next_t:
                self._step()
                next_t = now + 1.0
            time.sleep(0.05)

    def _step(self):
        self.voltage_V += random.uniform(-0.3, 0.3)
        self.voltage_V = max(200.0, min(250.0, self.voltage_V))

        self.current_A += random.uniform(-0.05, 0.05)
        self.current_A = max(0.0, min(15.0, self.current_A))

        self.power_W = int(self.voltage_V * self.current_A)

        self.flow_Lpm += random.uniform(-0.4, 0.4)
        self.flow_Lpm = max(0.0, min(80.0, self.flow_Lpm))

        self.energy_Wh += max(0, int(self.power_W / 3600))
        self.water_L += max(0, int(self.flow_Lpm / 60))

        self._handle_hr_writes()
        self._write_snapshot()


def build_context():
    di = ModbusSequentialDataBlock(0, [0] * 16)
    co = ModbusSequentialDataBlock(0, [0] * 16)
    ir = ModbusSequentialDataBlock(0, [0] * 128)
    hr = ModbusSequentialDataBlock(0, [0] * 256)

    slave = ModbusSlaveContext(di=di, co=co, ir=ir, hr=hr, zero_mode=True)
    ctx = ModbusServerContext(slaves=slave, single=True)
    return ctx


def main():
    import serial

    parser = argparse.ArgumentParser(description="Modbus RTU Slave - Meter Simulator")
    parser.add_argument("--port", required=True, help="Serial port (e.g., COM5 or /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=9600, help="Baudrate (default 9600)")
    parser.add_argument("--parity", choices=["N", "E", "O"], default="N", help="Parity (N/E/O)")
    parser.add_argument("--stopbits", type=int, choices=[1, 2], default=1, help="Stop bits (1/2)")
    parser.add_argument("--bytesize", type=int, choices=[7, 8], default=8, help="Data bits (7/8)")
    parser.add_argument("--unit", type=int, default=1, help="Unit ID / Slave ID (default 1)")
    parser.add_argument("--no-auto", action="store_true", help="Disable auto-update (values fixed)")
    args = parser.parse_args()

    parity_map = {
        "N": serial.PARITY_NONE,
        "E": serial.PARITY_EVEN,
        "O": serial.PARITY_ODD,
    }

    context = build_context()
    # In single=True mode, pymodbus ignores unit id and serves a single slave.
    model = MeterModel(context, unit_id=0x00 if context.single else args.unit, auto_update=(not args.no_auto))
    model.start()

    log.info("Starting Modbus RTU Slave on %s @ %d %s%d",
             args.port, args.baud, args.parity, args.stopbits)
    log.info("Unit ID: %d | IR/HR initialized. Press Ctrl+C to stop.", args.unit)

    try:
        StartSerialServer(
            context=context,
            framer=ModbusRtuFramer,
            identity=None,
            port=args.port,
            timeout=1,
            baudrate=args.baud,
            stopbits=args.stopbits,
            bytesize=args.bytesize,
            parity=parity_map[args.parity],
            handle_local_echo=False,
        )
    except KeyboardInterrupt:
        pass
    finally:
        model.stop()
        log.info("Server stopped.")


if __name__ == "__main__":
    main()
