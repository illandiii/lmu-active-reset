"""Bounded read-only energy candidates, compared with fresh official telemetry.
No field is declared restorable by this experiment.
"""

import argparse
import ctypes
import datetime
import json
import math
import struct
import time
from ctypes import wintypes as W
from pathlib import Path
from capture_context import PROJECT, KERNEL, get_json
from capture_preparation_14200 import verify_telemetry
from context_gate import assess
from run_physics_observer import modules


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--pid", type=int, required=True)
    p.add_argument("--native-log", type=Path, required=True)
    p.add_argument("--seconds", type=int, default=45)
    a = p.parse_args()
    if not 1 <= a.seconds <= 60:
        p.error("Duration 1..60")
    verify_telemetry(a.pid)
    loaded = modules(a.pid)
    if any("easyanti" in m["name"].lower() for m in loaded):
        raise ValueError("EAC present")
    base = next(m["base"] for m in loaded if m["name"].lower() == "le mans ultimate.exe")
    KERNEL.ReadProcessMemory.argtypes = (
        W.HANDLE,
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t),
    )
    KERNEL.ReadProcessMemory.restype = W.BOOL
    h = KERNEL.OpenProcess(0x410, False, a.pid)
    if not h:
        raise ctypes.WinError(ctypes.get_last_error())
    total = 0
    rows = []
    rejected = 0

    def read(addr, n):
        nonlocal total
        if not 0x10000 <= addr < 0x800000000000 or not 0 < n <= 64 or total + n > 1024 * 1024:
            raise ValueError("Read budget")
        b = ctypes.create_string_buffer(n)
        got = ctypes.c_size_t()
        if not KERNEL.ReadProcessMemory(h, addr, b, n, ctypes.byref(got)) or got.value != n:
            raise ctypes.WinError(ctypes.get_last_error())
        total += n
        return b.raw

    def ptr(addr):
        return struct.unpack("<Q", read(addr, 8))[0]

    def number(addr):
        x = struct.unpack("<d", read(addr, 8))[0]
        if not math.isfinite(x):
            raise ValueError("Nonfinite candidate")
        return x

    try:
        if read(base + 0xBA31BF, 14) != bytes.fromhex("488b0522302701488bb8a0590100"):
            raise ValueError("Owner code changed")
        deadline = time.monotonic() + a.seconds
        while time.monotonic() < deadline:
            started = KERNEL.GetTickCount64()
            before = get_json("/navigation/state")["state"]
            game = get_json("/rest/sessions/GetGameState")
            with a.native_log.open("rb") as f:
                f.seek(0, 2)
                size = f.tell()
                f.seek(max(0, size - 8192))
                lines = f.read().splitlines()
            native = None
            for line in reversed(lines):
                try:
                    native = json.loads(line)
                    break
                except json.JSONDecodeError:
                    pass
            after = get_json("/navigation/state")["state"]
            gate = assess(
                before,
                after,
                game,
                native,
                KERNEL.GetTickCount64(),
                started,
                observation_build=14200,
            )
            if gate["context_decision"] != "allowed":
                rejected += 1
                time.sleep(0.1)
                continue
            owner = ptr(base + 0x1E161E8)
            vehicle = ptr(owner + 0x159A0)
            motor = ptr(vehicle + 0x46F8)
            if ptr(motor) != base + 0x1790490:
                raise ValueError("Not the inventoried ElectricMotor type")
            battery = ptr(motor + 0x160)
            charge = number(battery)
            capacity = number(battery + 0x10)
            factor = number(battery + 0x20)
            candidate_fuel = number(vehicle + 0x448)
            candidate_virtual = number(vehicle + 0x4F8)
            if capacity <= 0 or not 0 <= charge <= capacity:
                raise ValueError("Charge/capacity invariant failed")
            if (
                ptr(base + 0x1E161E8) != owner
                or ptr(owner + 0x159A0) != vehicle
                or ptr(vehicle + 0x46F8) != motor
                or ptr(motor + 0x160) != battery
            ):
                raise ValueError("Ownership changed")
            final = get_json("/navigation/state")["state"]
            now = KERNEL.GetTickCount64()
            if final != after or now - native["host_tick_ms"] > 350:
                rejected += 1
                continue
            rows.append(
                dict(
                    tick_ms=now,
                    native_age_ms=now - native["host_tick_ms"],
                    charge_candidate=charge,
                    capacity_candidate=capacity,
                    conversion_factor_candidate=factor,
                    hdv_448=candidate_fuel,
                    hdv_4f8=candidate_virtual,
                    charge_ratio=charge / capacity,
                    ratio_error=charge / capacity - native["battery_fraction"],
                    native={
                        k: native[k]
                        for k in (
                            "sample",
                            "fuel_l",
                            "battery_fraction",
                            "virtual_energy",
                            "rpm",
                            "gear",
                            "throttle",
                            "brake",
                        )
                    },
                )
            )
            time.sleep(0.1)
    finally:
        KERNEL.CloseHandle(h)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    dest = PROJECT / "logs" / f"{stamp}_energy_candidates_14200.json"
    result = dict(
        pid=a.pid,
        build=14200,
        bytes_read=total,
        physics_writes=0,
        samples=rows,
        rejected=rejected,
        limits="Asynchronous field observations. ElectricMotor+160 is followed by native B15700; charge/capacity arithmetic observed, units and complete energy state not certified.",
    )
    dest.write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            dict(
                output=str(dest),
                samples=len(rows),
                rejected=rejected,
                bytes_read=total,
                physics_writes=0,
            )
        )
    )


if __name__ == "__main__":
    main()
