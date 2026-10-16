"""Replay numerical driveline caches on fixture memory; never restores LMU."""

from sector.paths import PROJECT_ROOT

import argparse
import ctypes as C
import json
import hashlib
from pathlib import Path
from sector.observers.run_snapshot_observer_v19_25661166 import DrivelineCache


class DrivelineBank(C.Structure):
    _fields_ = [("sequence", C.c_uint64), ("state", DrivelineCache)]


def state(row):
    s = DrivelineCache()
    s.torque[:] = row["driveline_cache"]["torque"]
    s.drive_type = row["driveline_cache"]["drive_type"]
    return s


def make_driveline_bank(dll, sequence, row):
    b = DrivelineBank()
    s = state(row)
    f = dll.fixture_driveline_save
    f.argtypes = [C.POINTER(DrivelineBank), C.c_uint64, C.POINTER(DrivelineCache)]
    f.restype = C.c_int
    assert f(C.byref(b), sequence, C.byref(s))
    return b


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    a = p.parse_args()
    rows = [json.loads(x) for x in a.frames.read_text().splitlines() if x]
    assert rows
    path = PROJECT_ROOT / "work/checkpoint-bank-fixture.dll"
    dll = C.CDLL(str(path))
    b = make_driveline_bank(dll, 42, rows[0])
    original = bytes(b)
    out = DrivelineCache()
    changes = 0
    negative = 0
    prepare = dll.fixture_driveline_prepare
    prepare.argtypes = [
        C.POINTER(DrivelineBank),
        C.c_uint64,
        C.POINTER(DrivelineCache),
        C.POINTER(DrivelineCache),
    ]
    prepare.restype = C.c_int
    for row in rows:
        s = state(row)
        changes += bytes(s) != bytes(b.state)
        assert prepare(C.byref(b), 42, C.byref(s), C.byref(out)) and bytes(out) == bytes(b.state)
        s.torque[2] = float("nan")
        assert not dll.fixture_driveline_save(C.byref(b), 43, C.byref(s)) and bytes(b) == original
        assert not prepare(C.byref(b), 42, C.byref(s), C.byref(out)) and bytes(out) == bytes(
            b.state
        )
        s = state(row)
        s.drive_type = (s.drive_type + 1) % 4
        assert not prepare(C.byref(b), 42, C.byref(s), C.byref(out)) and bytes(out) == bytes(
            b.state
        )
        s = state(row)
        assert not prepare(C.byref(b), 43, C.byref(s), C.byref(out))
        negative += 4
    report = dict(
        frames=len(rows),
        prepared_from_first=len(rows),
        changed_states=changes,
        negative_checks=negative,
        physics_writes=0,
        source_sha256=hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        fixture_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        limits="Three numerical torque caches only. Not complete clutch/gearbox/differential state coverage.",
    )
    a.frames.with_suffix(".driveline-replay.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
