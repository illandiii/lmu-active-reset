"""Replay the brake numerical bank on owned memory only."""

import argparse, ctypes as C, json, hashlib
from pathlib import Path
from run_snapshot_observer_v18_25661166 import BrakeState


class BrakeBank(C.Structure):
    _fields_ = [("sequence", C.c_uint64), ("wheels", BrakeState * 4)]


def states(row):
    out = (BrakeState * 4)()
    for i, s in enumerate(row["brake_state"]):
        out[i].fields_5d8[:] = s["fields_5d8"]
        out[i].field_630 = s["field_630"]
        out[i].cache_138 = s["cache_138"]
    return out


def make_brake_bank(dll, sequence, row):
    bank = BrakeBank()
    s = states(row)
    save = dll.fixture_brake_save
    save.argtypes = [C.POINTER(BrakeBank), C.c_uint64, C.POINTER(BrakeState)]
    save.restype = C.c_int
    assert save(C.byref(bank), sequence, s)
    return bank


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    a = p.parse_args()
    rows = [json.loads(x) for x in a.frames.read_text().splitlines() if x]
    assert rows
    path = Path(__file__).resolve().parents[1] / "work/checkpoint-bank-fixture.dll"
    dll = C.CDLL(str(path))
    bank = make_brake_bank(dll, 42, rows[0])
    original = bytes(bank)
    out = (BrakeState * 4)()
    negative = 0
    changes = 0
    prepare = dll.fixture_brake_prepare
    prepare.argtypes = [
        C.POINTER(BrakeBank),
        C.c_uint64,
        C.POINTER(BrakeState),
        C.POINTER(BrakeState),
    ]
    prepare.restype = C.c_int
    for row in rows:
        s = states(row)
        changes += bytes(s) != bytes(bank.wheels)
        assert prepare(C.byref(bank), 42, s, out) and bytes(out) == bytes(bank.wheels)
        for w in range(4):
            assert s[w].fields_5d8[0] == row["dependencies"]["wheels"][w]["term_5d8"]
            actual = row["bodies"][row["links"]["wheels"][w]]["mass"]["inverse_inertia_local"][0]
            assert abs(s[w].cache_138 - actual) <= 32 * 2**-52 * abs(actual)
        s[3].fields_5d8[4] = float("nan")
        assert not dll.fixture_brake_save(C.byref(bank), 43, s) and bytes(bank) == original
        assert not prepare(C.byref(bank), 42, s, out) and bytes(out) == bytes(bank.wheels)
        negative += 2
        s = states(row)
        assert not prepare(C.byref(bank), 43, s, out) and bytes(out) == bytes(bank.wheels)
        negative += 1
    report = {
        "frames": len(rows),
        "prepared_from_first": len(rows),
        "changed_brake_states": changes,
        "negative_checks": negative,
        "physics_writes": 0,
        "source_sha256": hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        "fixture_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "limits": "Typed numeric C6ED00/ECD890 fields only. Damage indicator outside the wheel and full brake failure recovery are not covered.",
    }
    a.frames.with_suffix(".brake-replay.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
