"""Local C replay of the 14200 body/fuel snapshots. Never accesses the game."""

from sector.paths import PROJECT_ROOT

import argparse
import collections
import ctypes as C
import hashlib
import json
from pathlib import Path
from sector.replay.test_wheel_recording import Body, Mass
from sector.replay.test_binding_recording import assign


class Fuel(C.Structure):
    _fields_ = [
        (n, C.c_double)
        for n in ("quantity", "quantity_rate", "density", "base_mass", "additional_mass")
    ]


class FuelPrepared(C.Structure):
    _fields_ = [("saved", Fuel), ("body_mass", C.c_double), ("body_inverse_mass", C.c_double)]


class Prepared(C.Structure):
    _fields_ = [
        ("state", Body),
        ("inverse_inertia_world", C.c_double * 6),
        ("angular_velocity_world", C.c_double * 3),
    ]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    a = p.parse_args()
    path = PROJECT_ROOT / "work/fuel-fixture.dll"
    dll = C.CDLL(str(path))
    f = dll.fixture_prepare_fuel
    f.argtypes = [
        C.POINTER(Body),
        C.POINTER(Mass),
        C.c_size_t,
        C.c_size_t,
        C.POINTER(Fuel),
        C.POINTER(Fuel),
        C.POINTER(Prepared),
        C.POINTER(FuelPrepared),
    ]
    f.restype = C.c_int
    rows = [json.loads(line) for line in a.frames.read_text().splitlines() if line]
    assert rows
    statuses = collections.Counter()
    same_frame = 0
    changed = collections.Counter()
    changed_mass = collections.Counter()
    changed_inertia = collections.Counter()
    total = 0
    for row in rows:
        n = row["total_count"]
        b = (Body * n)()
        m = (Mass * n)()
        out = (Prepared * n)()
        fuel = FuelPrepared()
        bf = Fuel()
        af = Fuel()
        assign(bf, row["energy_before"]["fuel"])
        assign(af, row["energy_after"]["fuel"])
        idx = row["energy_before"]["fuel_body_index"]
        assert (
            row["energy_before"]["valid"]
            and row["energy_after"]["valid"]
            and idx == row["energy_after"]["fuel_body_index"]
        )
        for i in range(n):
            assign(b[i], row["before"][i]["state"])
            assign(m[i], row["after"][i]["state"]["mass"])
            if bytes(b[i].mass) != bytes(m[i]):
                changed[str(i)] += 1
            if b[i].mass.inverse_mass != m[i].inverse_mass:
                changed_mass[str(i)] += 1
            if list(b[i].mass.inverse_inertia_local) != list(m[i].inverse_inertia_local):
                changed_inertia[str(i)] += 1
        source = (bytes(b), bytes(m), bytes(bf), bytes(af))
        C.memset(C.addressof(out), 0xA5, C.sizeof(out))
        C.memset(C.addressof(fuel), 0xA5, C.sizeof(fuel))
        previous = (bytes(out), bytes(fuel))
        status = f(b, m, n, idx, C.byref(bf), C.byref(af), out, C.byref(fuel))
        assert status == row["prepare_with_fuel"]
        statuses[str(status)] += 1
        if status:
            assert previous == (bytes(out), bytes(fuel))
        assert source == (bytes(b), bytes(m), bytes(bf), bytes(af))
        # The exact same frame should prepare correctly, including fuel identity.
        for i in range(n):
            m[i] = b[i].mass
        assert f(b, m, n, idx, C.byref(bf), C.byref(bf), out, C.byref(fuel)) == 0
        same_frame += 1
        total += n
    result = dict(
        source=a.frames.name,
        sha256=hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        fixture_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        frames=len(rows),
        same_frame_preparations=same_frame,
        body_states=total,
        cross_step_statuses=dict(statuses),
        changed_mass_or_inertia_bodies=dict(changed),
        changed_mass_bodies=dict(changed_mass),
        changed_inertia_bodies=dict(changed_inertia),
        physics_writes=0,
        limits="Local preparation only. Failure across steps remains a real unsupported dependency; it is not overridden. No live checkpoint write.",
    )
    a.frames.with_suffix(".fuel-replay.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
