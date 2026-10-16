"""Read-only scoring ownership and flags; never writes or invokes Score methods."""

import argparse
import ctypes
import datetime
import json
import struct
from ctypes import wintypes as W
from sector.telemetry.capture_context import PROJECT, KERNEL, get_json
from sector.telemetry.capture_preparation_14200 import verify_telemetry
from sector.observers.run_physics_observer import modules


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--pid", type=int, required=True)
    a = p.parse_args()
    verify_telemetry(a.pid)
    loaded = modules(a.pid)
    if any("easyanti" in m["name"].lower() for m in loaded):
        raise ValueError("EAC present")
    base = next(m["base"] for m in loaded if m["name"].lower() == "le mans ultimate.exe")
    before = get_json("/navigation/state")["state"]
    h = KERNEL.OpenProcess(0x410, False, a.pid)
    if not h:
        raise ctypes.WinError(ctypes.get_last_error())
    KERNEL.ReadProcessMemory.argtypes = (
        W.HANDLE,
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t),
    )
    KERNEL.ReadProcessMemory.restype = W.BOOL
    total = 0

    def read(addr, n):
        nonlocal total
        if not 0x10000 <= addr < 0x800000000000 or not 0 < n <= 32 or total + n > 1024:
            raise ValueError("Read budget")
        b = ctypes.create_string_buffer(n)
        got = ctypes.c_size_t()
        if not KERNEL.ReadProcessMemory(h, addr, b, n, ctypes.byref(got)) or got.value != n:
            raise ctypes.WinError(ctypes.get_last_error())
        total += n
        return b.raw

    def ptr(addr):
        return struct.unpack("<Q", read(addr, 8))[0]

    def i32(addr):
        return struct.unpack("<i", read(addr, 4))[0]

    try:
        owner = ptr(base + 0x1E161E8)
        vehicle = ptr(owner + 0x159A0)
        game_vehicle = ptr(vehicle + 0x4BC0)
        scoring = game_vehicle + 0x1CEC0
        if ptr(scoring) != owner or ptr(owner) != game_vehicle:
            raise ValueError("Scoring owner relation differs")

        def flags():
            return dict(
                current=i32(scoring + 0x160),
                remembered=i32(scoring + 0x27290),
                last_computed=i32(scoring + 0x2735C),
                next_timed=read(scoring + 0x164, 1)[0],
                off_track=read(scoring + 0x272B8, 1)[0],
            )

        values = flags()
        lap = i32(scoring + 0x10)
        if any(
            values[n] not in (0, 1, 2) for n in ("current", "remembered", "last_computed")
        ) or any(values[n] not in (0, 1) for n in ("next_timed", "off_track")):
            raise ValueError("Invalid flag value")
        if (
            values != flags()
            or ptr(scoring) != owner
            or ptr(owner) != game_vehicle
            or ptr(base + 0x1E161E8) != owner
            or ptr(owner + 0x159A0) != vehicle
        ):
            raise ValueError("State or ownership changed")
    finally:
        KERNEL.CloseHandle(h)
    if get_json("/navigation/state")["state"] != before:
        raise ValueError("Context changed")
    result = dict(
        build=14200,
        ownership_verified=True,
        flags=values,
        lap=lap,
        bytes_read=total,
        physics_writes=0,
        limits="Asynchronous bounded read. No scoring-thread hook, flag writes, lap transition or best-time preservation tested.",
    )
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    dest = PROJECT / "logs" / f"{stamp}_scoring_14200.json"
    dest.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
