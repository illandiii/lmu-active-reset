"""Read-only typed Bar constraint observation; cached geometry may be from an earlier solver phase."""

import argparse
import ctypes as C
import datetime
import json
import math
import struct
from ctypes import wintypes as W
from capture_context import PROJECT, KERNEL
from capture_preparation_14200 import verify_telemetry
from probe_contacts_14200 import context
from run_physics_observer import modules
from test_wheel_recording import Body
from run_vehicle_observer_14200 import payload


class Geometry(C.Structure):
    _fields_ = [("anchors_local", (C.c_double * 3) * 2), ("softness", C.c_double)]


class Prepared(C.Structure):
    _fields_ = [
        ("length", C.c_double),
        ("jacobian", (C.c_double * 3) * 4),
        ("weighted", (C.c_double * 3) * 4),
        ("inverse_effective_mass", C.c_double),
    ]


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
        C.c_void_p,
        C.c_void_p,
        C.c_size_t,
        C.POINTER(C.c_size_t),
    )
    KERNEL.ReadProcessMemory.restype = W.BOOL
    h = KERNEL.OpenProcess(0x410, False, a.pid)
    if not h:
        raise C.WinError(C.get_last_error())
    total = 0
    rows = []

    def read(addr, n):
        nonlocal total
        if not 0x10000 <= addr < 0x800000000000 or not 0 < n <= 512 or total + n > 65536:
            raise ValueError("Read budget")
        b = C.create_string_buffer(n)
        got = C.c_size_t()
        if not KERNEL.ReadProcessMemory(h, addr, b, n, C.byref(got)) or got.value != n:
            raise C.WinError(C.get_last_error())
        total += n
        return b.raw

    def ptr(addr):
        return struct.unpack("<Q", read(addr, 8))[0]

    def doubles(addr, n):
        v = struct.unpack("<" + "d" * n, read(addr, n * 8))
        if not all(math.isfinite(x) for x in v):
            raise ValueError("Nonfinite observation")
        return v

    def vector(addr):
        b, e, c = struct.unpack("<3Q", read(addr, 24))
        if not 0x10000 <= b <= e <= c or (e - b) % 8 or not 0 < e - b <= 512:
            raise ValueError("Topology bound")
        return list(struct.unpack("<" + "Q" * ((e - b) // 8), read(b, e - b)))

    dll = C.CDLL(str(PROJECT / "work/bar-constraint-fixture.dll"))
    f = dll.fixture_bar
    f.argtypes = [C.POINTER(Body), C.POINTER(Geometry), C.POINTER(Prepared)]
    f.restype = C.c_int
    try:
        if read(base + 0xBA31BF, 14) != bytes.fromhex("488b0522302701488bb8a0590100"):
            raise ValueError("Owner code changed")
        owner = ptr(base + 0x1E161E8)
        hdv = ptr(owner + 0x159A0)
        root = vector(hdv + 0x23F8)
        constraints = vector(hdv + 0x2410)
        for index, c in enumerate(constraints):
            if ptr(c) != base + 0x18042A8:
                continue
            links = [ptr(c + 0xA0), ptr(c + 0xA8)]
            if any(root.count(b) != 1 for b in links):
                raise ValueError("Bar body ownership unresolved")
            bodies = (Body * 2)()
            g = Geometry()
            out = Prepared()
            for i, b in enumerate(links):
                bodies[i].mass.inverse_mass = doubles(b + 0x58, 1)[0]
                bodies[i].mass.inverse_inertia_local[:] = doubles(b + 0xF8, 3)
                bodies[i].position_world[:] = doubles(b + 0x40, 3)
                bodies[i].velocity_world[:] = doubles(b + 0x28, 3)
                bodies[i].orientation[:] = doubles(b + 0x90, 9)
                bodies[i].momentum_world[:] = doubles(b + 0x110, 3)
                g.anchors_local[i][:] = doubles(c + 0x50 + i * 0x18, 3)
            g.softness = doubles(c + 0x118, 1)[0]
            jac = doubles(c + 0xB0, 12)
            weighted = doubles(c + 0x140, 12)
            coeff = doubles(c + 0x138, 1)[0]
            impulse, lower, upper = doubles(c + 0x120, 3)
            status = f(bodies, C.byref(g), C.byref(out))
            result = dict(
                index=index,
                body_indices=[root.index(b) for b in links],
                bodies=payload(bodies),
                geometry=payload(g),
                native_jacobian=jac,
                native_weighted=weighted,
                native_inverse_effective_mass=coeff,
                cached_impulse=impulse,
                lower_impulse=lower,
                upper_impulse=upper,
                status=status,
            )
            if not status:
                result.update(
                    prepared=payload(out),
                    jacobian_error=max(
                        abs(out.jacobian[i // 3][i % 3] - jac[i]) for i in range(12)
                    ),
                    weighted_error=max(
                        abs(out.weighted[i // 3][i % 3] - weighted[i]) for i in range(12)
                    ),
                    coefficient_error=abs(out.inverse_effective_mass - coeff),
                )
            rows.append(result)
            if ptr(c) != base + 0x18042A8 or [ptr(c + 0xA0), ptr(c + 0xA8)] != links:
                raise ValueError("Bar lifetime changed")
        if (
            root != vector(hdv + 0x23F8)
            or constraints != vector(hdv + 0x2410)
            or ptr(base + 0x1E161E8) != owner
            or ptr(owner + 0x159A0) != hdv
        ):
            raise ValueError("Topology changed")
    finally:
        KERNEL.CloseHandle(h)
    if context() != initial:
        raise ValueError("Context changed")
    valid = [r for r in rows if r["status"] == 0]
    result = dict(
        build=14200,
        bars=len(rows),
        valid_geometry=len(valid),
        rows=rows,
        bytes_read=total,
        physics_writes=0,
        max_jacobian_error=max((r["jacobian_error"] for r in valid), default=None),
        max_weighted_error=max((r["weighted_error"] for r in valid), default=None),
        max_coefficient_error=max((r["coefficient_error"] for r in valid), default=None),
        limits="Asynchronous geometry compared with phase-dependent native caches; not a coherent constraint checkpoint, impulse application or full solver reconstruction.",
    )
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    (PROJECT / "logs" / f"{stamp}_bar_constraints_14200.json").write_text(
        json.dumps(result, indent=2) + "\n"
    )
    print(json.dumps({k: v for k, v in result.items() if k != "rows"}, indent=2))


if __name__ == "__main__":
    main()
