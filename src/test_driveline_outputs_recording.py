"""Replay the four captured outputs on owned memory, without LMU writes."""

import argparse, ctypes as C, json, hashlib
from pathlib import Path
from run_snapshot_observer_v20_25661166 import DrivelineOutputs


class OutputsBank(C.Structure):
    _fields_ = [("sequence", C.c_uint64), ("state", DrivelineOutputs)]


def state(row):
    return DrivelineOutputs(**row["driveline_outputs"])


def make_outputs_bank(dll, sequence, row):
    bank = OutputsBank()
    value = state(row)
    f = dll.fixture_outputs_save
    f.argtypes = [C.POINTER(OutputsBank), C.c_uint64, C.POINTER(DrivelineOutputs)]
    f.restype = C.c_int
    assert f(C.byref(bank), sequence, C.byref(value))
    return bank


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    a = p.parse_args()
    rows = [json.loads(line) for line in a.frames.read_text().splitlines() if line]
    assert rows
    path = Path(__file__).resolve().parents[1] / "work/checkpoint-bank-fixture.dll"
    dll = C.CDLL(str(path))
    bank = make_outputs_bank(dll, 42, rows[0])
    original = bytes(bank)
    out = DrivelineOutputs()
    changes = 0
    prepare = dll.fixture_outputs_prepare
    prepare.argtypes = [
        C.POINTER(OutputsBank),
        C.c_uint64,
        C.POINTER(DrivelineOutputs),
        C.POINTER(DrivelineOutputs),
    ]
    prepare.restype = C.c_int
    for row in rows:
        current = state(row)
        changes += bytes(current) != bytes(bank.state)
        assert prepare(C.byref(bank), 42, C.byref(current), C.byref(out)) and bytes(out) == bytes(
            bank.state
        )
        assert not prepare(C.byref(bank), 43, C.byref(current), C.byref(out))
        for name, _ in DrivelineOutputs._fields_:
            invalid = state(row)
            setattr(invalid, name, float("nan"))
            assert (
                not dll.fixture_outputs_save(C.byref(bank), 43, C.byref(invalid))
                and bytes(bank) == original
            )
            assert not prepare(C.byref(bank), 42, C.byref(invalid), C.byref(out)) and bytes(
                out
            ) == bytes(bank.state)
    report = dict(
        frames=len(rows),
        changed_states=changes,
        negative_checks=9 * len(rows),
        ranges={
            name: [
                min(r["driveline_outputs"][name] for r in rows),
                max(r["driveline_outputs"][name] for r in rows),
            ]
            for name, _ in DrivelineOutputs._fields_
        },
        source_sha256=hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        physics_writes=0,
        full_restore_supported=False,
    )
    a.frames.with_suffix(".outputs-replay.json").write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
