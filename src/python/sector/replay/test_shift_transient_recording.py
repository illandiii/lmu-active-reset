"""Replay shift state preparation, with request-age preservation; no live writes."""

from sector.paths import PROJECT_ROOT

import ctypes as C
import json
import argparse
import hashlib
from pathlib import Path
from sector.observers.run_snapshot_observer_v24_25661166 import ShiftObserved


class Bank(C.Structure):
    _fields_ = [("sequence", C.c_uint64), ("state", ShiftObserved)]


def state(row):
    value = ShiftObserved()
    for name, item in row["shift_observed"].items():
        if isinstance(item, list):
            getattr(value, name)[:] = item
        else:
            setattr(value, name, item)
    return value


def make_shift_bank(dll, sequence, row):
    bank = Bank()
    value = state(row)
    save = dll.fixture_shift_save
    save.argtypes = [C.POINTER(Bank), C.c_uint64, C.POINTER(ShiftObserved)]
    save.restype = C.c_int
    assert save(C.byref(bank), sequence, C.byref(value))
    return bank


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    a = p.parse_args()
    rows = [json.loads(x) for x in a.frames.read_text().splitlines() if x]
    assert rows
    dll = C.CDLL(str(PROJECT_ROOT / "work/checkpoint-bank-fixture.dll"))
    save = dll.fixture_shift_save
    save.argtypes = [C.POINTER(Bank), C.c_uint64, C.POINTER(ShiftObserved)]
    save.restype = C.c_int
    prep = dll.fixture_shift_prepare
    prep.argtypes = [
        C.POINTER(Bank),
        C.c_uint64,
        C.POINTER(ShiftObserved),
        C.POINTER(ShiftObserved),
    ]
    prep.restype = C.c_int
    bank = Bank()
    value = state(rows[0])
    assert save(C.byref(bank), 42, C.byref(value))
    age = value.clock_a8 - value.event_50
    out = ShiftObserved()
    clock_errors = []
    for row in rows:
        value = state(row)
        assert prep(C.byref(bank), 42, C.byref(value), C.byref(out))
        assert abs((out.clock_a8 - out.event_50) - age) < 1e-8
        assert list(out.gears) == list(bank.state.gears)
        clock_errors.append(abs(row["simulation_time"] - value.clock_a8 - value.dt_b0))
        assert not prep(C.byref(bank), 43, C.byref(value), C.byref(out))
    report = dict(
        frames=len(rows),
        prepared=len(rows),
        request_age_preserved=True,
        max_simulation_clock_delta_error=max(clock_errors),
        selected_gears=sorted({r["shift_observed"]["gears"][1] for r in rows}),
        event_time_changes=sum(
            rows[i]["shift_observed"]["event_50"] != rows[i - 1]["shift_observed"]["event_50"]
            for i in range(1, len(rows))
        ),
        native_preparations=rows[-1]["bank_preparations"],
        native_rejections=rows[-1]["bank_rejections"],
        source_sha256=hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        physics_writes=0,
        shift_live_writer=False,
        full_restore_supported=False,
    )
    a.frames.with_suffix(".shift-replay.json").write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
