"""Bounded read-only test of the tire inertia partition; not a coherent checkpoint."""

import argparse, ctypes, datetime, json, math, struct, time
from ctypes import wintypes as W
from capture_context import PROJECT, KERNEL
from capture_preparation_14200 import verify_telemetry
from probe_contacts_14200 import context
from run_physics_observer import modules


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
        if not 0x10000 <= addr < 0x800000000000 or not 0 < n <= 1024 or total + n > 256000:
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
        if read(base + 0xBA31BF, 14) != bytes.fromhex("488b0522302701488bb8a0590100"):
            raise ValueError("Owner code changed")
        owner = ptr(base + 0x1E161E8)
        hdv = ptr(owner + 0x159A0)
        tires = [ptr(hdv + 0x618 + i * 0x6B0 + 0x3E8) for i in range(4)]
        if len(set(tires)) != 4:
            raise ValueError("Duplicate tire objects")
        for sample in range(24):
            for wheel, tire in enumerate(tires):
                sectors_raw = read(tire + 0x48, 7 * 0x60 + 24)
                sectors = [struct.unpack_from("<3d", sectors_raw, i * 0x60) for i in range(8)]
                table = list(struct.unpack("<3f", read(tire + 0xCE4, 12)))
                cached = list(struct.unpack("<3d", read(tire + 0xC20, 24)))
                internal = list(struct.unpack("<3d", read(tire + 0xC00, 24)))
                inverse = list(struct.unpack("<3d", read(tire + 0xBB8, 24)))
                if not all(
                    math.isfinite(x)
                    for group in sectors + [table, cached, internal, inverse]
                    for x in group
                ):
                    raise ValueError("Nonfinite observation")
                summed = [sum(s[j] for s in sectors) for j in range(3)]
                expected = [table[j] - summed[j] for j in range(3)]
                rows.append(
                    dict(
                        sample=sample,
                        wheel=wheel,
                        total_table_inertia=table,
                        sector_inertia=sectors,
                        cached_sector_sum=cached,
                        internal_inertia=internal,
                        inverse_inertia=inverse,
                        sum_error=max(abs(summed[j] - cached[j]) for j in range(3)),
                        residual_error=max(abs(expected[j] - internal[j]) for j in range(3)),
                        inverse_error=max(abs(1 / internal[j] - inverse[j]) for j in range(3))
                        if min(internal) > 0
                        else None,
                    )
                )
            if (
                any(ptr(hdv + 0x618 + i * 0x6B0 + 0x3E8) != tires[i] for i in range(4))
                or ptr(base + 0x1E161E8) != owner
                or ptr(owner + 0x159A0) != hdv
            ):
                raise ValueError("Lifetime changed")
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
        max_sum_error=max(r["sum_error"] for r in rows),
        max_residual_error=max(r["residual_error"] for r in rows),
        max_inverse_error=max(r["inverse_error"] for r in rows),
        limits="Sequential asynchronous reads may straddle physics updates. Agreement supports the partition formula, not complete thermal/history coverage or restore permission.",
    )
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    dest = PROJECT / "logs" / f"{stamp}_tire_inertia_14200.json"
    dest.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({k: v for k, v in result.items() if k != "rows"}, indent=2))


if __name__ == "__main__":
    main()
