"""Read-only check of C6E170 fuel-dependent mass and EE2730 addends."""

import argparse, ctypes, json, math, struct, datetime
from ctypes import wintypes as W
from capture_context import PROJECT, KERNEL, get_json
from capture_preparation_14200 import verify_telemetry
from run_physics_observer import modules


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--pid", type=int, required=True)
    a = p.parse_args()
    verify_telemetry(a.pid)
    listed = modules(a.pid)
    if any("easyanti" in m["name"].lower() for m in listed):
        raise ValueError("EAC present")
    base = next(m["base"] for m in listed if m["name"].lower() == "le mans ultimate.exe")
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
        if not 0x10000 <= addr < 0x800000000000 or not 0 < n <= 256 or total + n > 8192:
            raise ValueError("Read budget")
        b = ctypes.create_string_buffer(n)
        got = ctypes.c_size_t()
        if not KERNEL.ReadProcessMemory(h, addr, b, n, ctypes.byref(got)) or got.value != n:
            raise ctypes.WinError(ctypes.get_last_error())
        total += n
        return b.raw

    def ptr(addr):
        return struct.unpack("<Q", read(addr, 8))[0]

    def val(addr):
        v = struct.unpack("<d", read(addr, 8))[0]
        if not math.isfinite(v):
            raise ValueError("Nonfinite value")
        return v

    try:
        if read(base + 0xBA31BF, 14) != bytes.fromhex("488b0522302701488bb8a0590100"):
            raise ValueError("Owner code changed")
        owner = ptr(base + 0x1E161E8)
        vehicle = ptr(owner + 0x159A0)
        fuel = vehicle + 0x448
        q = val(fuel)
        density = val(fuel + 0x60)
        base_mass = val(fuel + 0x80)
        body = ptr(fuel + 0x78)
        mass = val(body + 8)
        inv_mass = val(body + 0x58)
        rate = val(fuel + 8)
        branch = ptr(vehicle + 0x550)
        addends = []
        selectors = []
        if branch == 0:
            game_vehicle = ptr(vehicle + 0x4BC0)
            addends.append(struct.unpack("<f", read(game_vehicle + 0x440D0, 4))[0])
            selectors = list(read(vehicle + 0x138, 14))
            if any(i > 3 for i in selectors):
                raise ValueError("Unknown setup-table selector")
            # Preserve the native addition order (the final two terms group separately).
            for i, index in enumerate(selectors[:13]):
                addends.append(val(vehicle + 0x148 + i * 0x20 + index * 8))
            tail = val(vehicle + 0x2E8 + selectors[13] * 8) + val(vehicle + 0x558)
            addends.extend([val(vehicle + 0x2608), tail])
        extra = 0.0
        for x in addends:
            extra += x
        expected = q * density + base_mass + extra
        begin, end = struct.unpack("<QQ", read(vehicle + 0x23F8, 16))
        if not 0 < end - begin <= 32 * 8 or (end - begin) % 8:
            raise ValueError("Body vector bounds")
        objects = struct.unpack("<" + "Q" * ((end - begin) // 8), read(begin, end - begin))
        if objects.count(body) != 1:
            raise ValueError("Fuel body not uniquely in integration vector")
        if (
            q != val(fuel)
            or mass != val(body + 8)
            or inv_mass != val(body + 0x58)
            or ptr(fuel + 0x78) != body
            or ptr(base + 0x1E161E8) != owner
            or ptr(owner + 0x159A0) != vehicle
        ):
            raise ValueError("State/lifetime changed")
        motor = ptr(vehicle + 0x46F8)
        battery_alias = ptr(motor) == base + 0x1790490 and ptr(motor + 0x160) == vehicle + 0x4F8
    finally:
        KERNEL.CloseHandle(h)
    if get_json("/navigation/state")["state"] != before:
        raise ValueError("Context changed")
    result = dict(
        build=14200,
        quantity=q,
        quantity_rate=rate,
        density=density,
        base_mass=base_mass,
        additional_mass=extra,
        setup_addends=addends,
        setup_selectors=selectors,
        fuel_body_index=objects.index(body),
        root_count=len(objects),
        observed_mass=mass,
        observed_inverse_mass=inv_mass,
        expected_mass=expected,
        mass_error=abs(mass - expected),
        inverse_mass_error=abs(inv_mass - 1 / expected) if expected > 0 else None,
        hdv_4f8_is_electric_motor_battery=battery_alias,
        bytes_read=total,
        physics_writes=0,
        limits="Single bounded asynchronous sample. Native formula/ownership checked; full fuel system, inertia, thermal and energy restore not certified.",
    )
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    dest = PROJECT / "logs" / f"{stamp}_fuel_mass_14200.json"
    dest.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
