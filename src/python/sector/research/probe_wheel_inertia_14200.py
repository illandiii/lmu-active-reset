"""Bounded read-only wheel axial inertia formula check, never a checkpoint."""

import argparse
import ctypes
import datetime
import json
import math
import struct
import time
from ctypes import wintypes as W
from sector.telemetry.capture_context import PROJECT, KERNEL
from sector.telemetry.capture_preparation_14200 import verify_telemetry
from sector.research.probe_contacts_14200 import context
from sector.observers.run_physics_observer import modules


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--pid", type=int, required=True)
    a = p.parse_args()
    verify_telemetry(a.pid)
    initial = context()
    listed = modules(a.pid)
    if any("easyanti" in m["name"].lower() for m in listed):
        raise ValueError("EAC present")
    base = next(m["base"] for m in listed if m["name"].lower() == "le mans ultimate.exe")
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

    def read(addr, n):
        nonlocal total
        if not 0x10000 <= addr < 0x800000000000 or not 0 < n <= 64 or total + n > 65536:
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
        v = struct.unpack("<d", read(addr, 8))[0]
        if not math.isfinite(v):
            raise ValueError("Nonfinite observation")
        return v

    try:
        if read(base + 0xBA31BF, 14) != bytes.fromhex("488b0522302701488bb8a0590100"):
            raise ValueError("Owner code changed")
        owner = ptr(base + 0x1E161E8)
        hdv = ptr(owner + 0x159A0)
        bodies = [ptr(hdv + 0x618 + w * 0x6B0 + 0x1D0) for w in range(4)]
        if len(set(bodies)) != 4:
            raise ValueError("Duplicate wheel bodies")
        for sample in range(24):
            for w, body in enumerate(bodies):
                wheel = hdv + 0x618 + w * 0x6B0
                values = {
                    n: number(wheel + o)
                    for n, o in [
                        ("base_130", 0x130),
                        ("term_4b8", 0x4B8),
                        ("term_5d8", 0x5D8),
                        ("factor_4f0", 0x4F0),
                    ]
                }
                expected = (
                    (values["term_4b8"] + values["term_4b8"]) + values["term_5d8"]
                ) * values["factor_4f0"] + values["base_130"]
                inertia = number(body + 0x140)
                inverse = number(body + 0xF8)
                cached = number(wheel + 0x138)
                rows.append(
                    dict(
                        sample=sample,
                        wheel=w,
                        inputs=values,
                        axial_inertia=inertia,
                        inverse_axial=inverse,
                        cached_inverse=cached,
                        formula_error=abs(expected - inertia),
                        inverse_error=abs(1 / expected - inverse) if expected > 0 else None,
                    )
                )
                if ptr(wheel + 0x1D0) != body:
                    raise ValueError("Wheel lifetime changed")
            if ptr(base + 0x1E161E8) != owner or ptr(owner + 0x159A0) != hdv:
                raise ValueError("Vehicle lifetime changed")
            time.sleep(0.01)
    finally:
        KERNEL.CloseHandle(h)
    if context() != initial:
        raise ValueError("Context changed")
    result = dict(
        build=14200,
        rows=rows,
        bytes_read=total,
        physics_writes=0,
        max_formula_error=max(r["formula_error"] for r in rows),
        max_inverse_error=max(r["inverse_error"] for r in rows),
        limits="Asynchronous read-only formula check; upstream meaning/history of the four inputs and coherent restore remain separate requirements.",
    )
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    (PROJECT / "logs" / f"{stamp}_wheel_inertia_14200.json").write_text(
        json.dumps(result, indent=2) + "\n"
    )
    print(json.dumps({k: v for k, v in result.items() if k != "rows"}, indent=2))


if __name__ == "__main__":
    main()
