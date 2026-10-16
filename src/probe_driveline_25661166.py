"""Bounded read-only inventory of the current player's driveline object links."""

import argparse
import ctypes
import datetime
import json
import struct
from ctypes import wintypes as W
from capture_context import PROJECT, KERNEL, get_json
from capture_preparation_steam25661166 import verify_telemetry
from run_physics_observer import modules


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--pid", type=int, required=True)
    a = p.parse_args()
    verify_telemetry(a.pid)
    loaded = modules(a.pid)
    if any("easyanti" in m["name"].lower() for m in loaded):
        raise ValueError("EAC present")
    image = next(m for m in loaded if m["name"].lower() == "le mans ultimate.exe")
    base = image["base"]
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
        if not 0x10000 <= addr < 0x800000000000 or not 0 < n <= 128 or total + n > 4096:
            raise ValueError("Read budget")
        b = ctypes.create_string_buffer(n)
        got = ctypes.c_size_t()
        if not KERNEL.ReadProcessMemory(h, addr, b, n, ctypes.byref(got)) or got.value != n:
            raise ctypes.WinError(ctypes.get_last_error())
        total += n
        return b.raw

    def ptr(addr):
        return struct.unpack("<Q", read(addr, 8))[0]

    try:
        owner = ptr(base + 0x1E194C8)
        hdv = ptr(owner + 0x159A8)
        links = {off: ptr(hdv + 0x4900 + off) for off in (8, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38)}
        if links[8] != hdv + 0x618:
            raise ValueError("Driveline wheel ownership differs")
        shaft_bytes = read(links[0x18], 16)
        shaft = {
            "raw_hex": shaft_bytes.hex(),
            "double_values": list(struct.unpack("<2d", shaft_bytes)),
            "hdv_relative_offset": hex(links[0x18] - hdv),
            "gear": struct.unpack("<i", read(links[0x20] + 0x64, 4))[0],
        }
        shift_base = hdv + 0x4740
        shift = {"block_rva_relative_hdv": "0x4740", "tables": []}
        for offset in (0, 0x18, 0x30, 0x48):
            begin, end, capacity = struct.unpack("<3Q", read(shift_base + offset, 24))
            if not begin <= end <= capacity or (end - begin) % 8 or end - begin > 512:
                raise ValueError("Shift table layout unverified")
            shift["tables"].append({"offset": hex(offset), "count": (end - begin) // 8})
        shift["ramp_d0"] = struct.unpack("<d", read(shift_base + 0xD0, 8))[0]
        shift["indices_d8_dc"] = list(struct.unpack("<2I", read(shift_base + 0xD8, 8)))
        shift["flags_e0_e1"] = list(read(shift_base + 0xE0, 2))
        shift["timer_e8_factors_f0_f8"] = list(struct.unpack("<3d", read(shift_base + 0xE8, 24)))
        shift["engine_50_1b0"] = [
            struct.unpack("<d", read(links[0x28] + offset, 8))[0] for offset in (0x50, 0x1B0)
        ]
        shift["transition_ints_4ba0_4bb4"] = list(struct.unpack("<6i", read(hdv + 0x4BA0, 24)))
        shift["accumulators_4b30_4bb8"] = [
            struct.unpack("<f", read(hdv + offset, 4))[0] for offset in (0x4B30, 0x4BB8)
        ]
        shift["timer_4ac0"] = struct.unpack("<d", read(hdv + 0x4AC0, 8))[0]
        shift["control_4668"] = struct.unpack("<d", read(hdv + 0x4668, 8))[0]
        shift["notification_4ae8_4aec"] = list(struct.unpack("<2i", read(hdv + 0x4AE8, 8)))
        shift["request_48_4c_time_50"] = list(struct.unpack("<2id", read(hdv + 0x48, 16)))
        objects = {}
        for off in (0x28, 0x30, 0x38):
            obj = links[off]
            vt = ptr(obj)
            if not base <= vt < base + image["size"]:
                raise ValueError("Vtable outside pinned module")
            methods = struct.unpack("<8Q", read(vt, 64))
            if any(not base <= m < base + image["size"] for m in methods):
                raise ValueError("Method outside pinned module")
            objects[hex(off)] = {
                "vtable_rva": hex(vt - base),
                "method_rvas": [hex(m - base) for m in methods],
            }
        if (
            links != {off: ptr(hdv + 0x4900 + off) for off in links}
            or ptr(base + 0x1E194C8) != owner
            or ptr(owner + 0x159A8) != hdv
        ):
            raise ValueError("Owner changed")
    finally:
        KERNEL.CloseHandle(h)
    if get_json("/navigation/state")["state"] != before:
        raise ValueError("Context changed")
    result = dict(
        steam_build=25661166,
        objects=objects,
        shaft=shaft,
        shift=shift,
        bytes_read=total,
        physics_writes=0,
        limits="Asynchronous object/vtable inventory only. Does not establish state coverage or native ABI; shaft/shift values are not a coherent checkpoint.",
    )
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    dest = PROJECT / "logs" / f"{stamp}_driveline_25661166.json"
    dest.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
