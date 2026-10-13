"""Replay offset-named partial control state. No game writes."""

import argparse, ctypes as C, json, hashlib
from pathlib import Path
from run_snapshot_observer_v23_25661166 import PowertrainControl


class ControlBank(C.Structure):
    _fields_ = [("sequence", C.c_uint64), ("state", PowertrainControl)]


def state(row):
    value = PowertrainControl()
    for name, item in row["powertrain_control"].items():
        if isinstance(item, list):
            getattr(value, name)[:] = item
        else:
            setattr(value, name, item)
    return value


def make_control_bank(dll, sequence, row):
    bank = ControlBank()
    value = state(row)
    f = dll.fixture_control_save
    f.argtypes = [C.POINTER(ControlBank), C.c_uint64, C.POINTER(PowertrainControl)]
    f.restype = C.c_int
    assert f(C.byref(bank), sequence, C.byref(value))
    return bank


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    a = p.parse_args()
    rows = [json.loads(line) for line in a.frames.read_text().splitlines() if line]
    assert rows
    dll = C.CDLL(str(Path(__file__).resolve().parents[1] / "work/checkpoint-bank-fixture.dll"))
    bank = make_control_bank(dll, 42, rows[0])
    out = PowertrainControl()
    negative = 0
    prep = dll.fixture_control_prepare
    prep.argtypes = [
        C.POINTER(ControlBank),
        C.c_uint64,
        C.POINTER(PowertrainControl),
        C.POINTER(PowertrainControl),
    ]
    prep.restype = C.c_int
    for row in rows:
        value = state(row)
        assert prep(C.byref(bank), 42, C.byref(value), C.byref(out)) and bytes(out) == bytes(
            bank.state
        )
        assert not prep(C.byref(bank), 43, C.byref(value), C.byref(out))
        negative += 1
        for name in (
            "ramp_d0",
            "timer_e8",
            "factor_f0",
            "factor_f8",
            "engine_50",
            "engine_1b0",
            "table_token",
            "index_d8",
            "index_dc",
            "active_e0",
            "latched_e1",
        ):
            bad = state(row)
            setattr(
                bad,
                name,
                float("nan")
                if name
                in ("ramp_d0", "timer_e8", "factor_f0", "factor_f8", "engine_50", "engine_1b0")
                else (bad.table_token + 1 if name == "table_token" else 99),
            )
            assert not prep(C.byref(bank), 42, C.byref(bad), C.byref(out)) and bytes(out) == bytes(
                bank.state
            )
            negative += 1
    report = dict(
        frames=len(rows),
        negative_checks=negative,
        changed_states=sum(bytes(state(r)) != bytes(bank.state) for r in rows),
        native_preparations=rows[-1]["bank_preparations"],
        native_rejections=rows[-1]["bank_rejections"],
        plan_fields=rows[-1]["plan_fields"],
        plan_bytes=rows[-1]["plan_bytes"],
        source_sha256=hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        physics_writes=0,
        full_restore_supported=False,
    )
    a.frames.with_suffix(".control-replay.json").write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
