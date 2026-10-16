"""Replay coherent wheel phase against independently reconstructed hub rotation."""

import argparse
import ctypes as C
import json
import hashlib
from pathlib import Path
from test_persistent_checkpoint_recording import snapshot, Snapshot
from run_snapshot_observer_v16_25661166 import WheelPhase


class PhaseBank(C.Structure):
    _fields_ = [("sequence", C.c_uint64), ("phase", WheelPhase * 4)]


def phases(row):
    result = (WheelPhase * 4)()
    for i, data in enumerate(row["wheel_phase"]):
        result[i].hub_index = data["hub_index"]
        result[i].wheel_index = data["wheel_index"]
        for name, value in data["spin"].items():
            setattr(result[i].spin, name, value)
    return result


def make_bank(dll, s, row):
    bank = PhaseBank()
    p = phases(row)
    save = dll.fixture_wheel_phase_save
    save.argtypes = [C.POINTER(PhaseBank), C.POINTER(Snapshot), C.POINTER(WheelPhase)]
    save.restype = C.c_int
    assert save(C.byref(bank), C.byref(s), p)
    return bank


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("frames", type=Path)
    a = parser.parse_args()
    rows = [json.loads(x) for x in a.frames.read_text().splitlines() if x]
    assert rows
    dllpath = Path(__file__).resolve().parents[1] / "work/checkpoint-bank-fixture.dll"
    dll = C.CDLL(str(dllpath))
    saved = snapshot(rows[0])
    bank = make_bank(dll, saved, rows[0])
    original = bytes(bank)
    prepare = dll.fixture_wheel_phase_prepare
    prepare.argtypes = [
        C.POINTER(PhaseBank),
        C.POINTER(Snapshot),
        C.POINTER(WheelPhase),
        C.POINTER(type(saved.bodies[0])),
        C.c_size_t,
        C.POINTER(WheelPhase),
    ]
    prepare.restype = C.c_int
    out = (WheelPhase * 4)()
    negative = 0
    changes = 0
    for row in rows:
        current = snapshot(row)
        p = phases(row)
        changes += bytes(p) != bytes(bank.phase)

        def restore():
            return prepare(
                C.byref(bank), C.byref(saved), p, current.bodies, current.body_count, out
            )

        assert restore() and bytes(out) == bytes(bank.phase)
        prior = bytes(out)
        p[3].spin.angle += 0.1
        assert not restore() and bytes(out) == prior
        negative += 1
        p = phases(row)
        p[3].spin.attached = 0
        assert (
            not dll.fixture_wheel_phase_save(C.byref(bank), C.byref(current), p)
            and bytes(bank) == original
        )
        assert not restore() and bytes(out) == prior
        negative += 2
        p = phases(row)
        p[3].hub_index = current.body_count
        assert not restore() and bytes(out) == prior
        negative += 1
        p = phases(row)
        saved.sequence += 1
        assert not restore() and bytes(out) == prior
        negative += 1
        saved.sequence -= 1
    report = {
        "frames": len(rows),
        "prepared_from_first": len(rows),
        "changed_wheel_phases": changes,
        "negative_checks": negative,
        "physics_writes": 0,
        "source_sha256": hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        "fixture_sha256": hashlib.sha256(dllpath.read_bytes()).hexdigest(),
        "limits": "Attached undeformed wheels only; no game writes or moving restore test.",
    }
    a.frames.with_suffix(".wheel-phase-replay.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
