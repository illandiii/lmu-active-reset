"""Replay the thermal bank from coherent native frames, never write to LMU."""

import argparse
import ctypes as C
import json
import hashlib
from pathlib import Path
from run_snapshot_observer_v14_25661166 import ThermalGrid


class Bank(C.Structure):
    _fields_ = [("sequence", C.c_uint64), ("grid", ThermalGrid)]


def grid(row):
    g = ThermalGrid()
    for w, t in enumerate(row["thermal_grid"]["tires"]):
        g.tires[w].angular_index[:] = t["angular_index"]
        for i, cells in enumerate(t["cells"]):
            for j, cell in enumerate(cells):
                g.tires[w].cells[i][j].layers[:] = cell["layers"]
                g.tires[w].cells[i][j].coefficients[:] = cell["coefficients"]
    return g


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    a = p.parse_args()
    rows = [json.loads(x) for x in a.frames.read_text().splitlines() if x]
    assert rows
    dllpath = Path(__file__).resolve().parents[1] / "work/checkpoint-bank-fixture.dll"
    dll = C.CDLL(str(dllpath))
    save = dll.fixture_thermal_save
    save.argtypes = [C.POINTER(Bank), C.c_uint64, C.POINTER(ThermalGrid)]
    save.restype = C.c_int
    prepare = dll.fixture_thermal_prepare
    prepare.argtypes = [C.POINTER(Bank), C.c_uint64, C.POINTER(ThermalGrid), C.POINTER(ThermalGrid)]
    prepare.restype = C.c_int
    bank = Bank()
    first = grid(rows[0])
    out = ThermalGrid()
    assert save(C.byref(bank), 42, C.byref(first)) == 1
    original = bytes(bank)
    negative = 0
    changed = 0
    for row in rows:
        current = grid(row)
        changed += bytes(current) != bytes(first)
        assert prepare(C.byref(bank), 42, C.byref(current), C.byref(out)) == 1 and bytes(
            out
        ) == bytes(first)
        expected = bytes(out)
        assert (
            not prepare(C.byref(bank), 43, C.byref(current), C.byref(out))
            and bytes(out) == expected
        )
        negative += 1
        value = current.tires[3].cells[71][8].layers[4]
        current.tires[3].cells[71][8].layers[4] = float("nan")
        assert not save(C.byref(bank), 43, C.byref(current)) and bytes(bank) == original
        assert (
            not prepare(C.byref(bank), 42, C.byref(current), C.byref(out))
            and bytes(out) == expected
        )
        negative += 2
        current.tires[3].cells[71][8].layers[4] = value
        indices = current.tires[2].angular_index
        prior = indices[71]
        indices[71] = indices[0]
        assert not save(C.byref(bank), 43, C.byref(current)) and bytes(bank) == original
        negative += 1
        indices[71] = prior
        indices[70], indices[71] = indices[71], indices[70]
        assert (
            not prepare(C.byref(bank), 42, C.byref(current), C.byref(out))
            and bytes(out) == expected
        )
        negative += 1
    result = {
        "frames": len(rows),
        "prepared_from_first": len(rows),
        "changed_grids": changed,
        "negative_checks": negative,
        "thermal_cells": 2592,
        "saved_bytes": C.sizeof(ThermalGrid),
        "physics_writes": 0,
        "full_restore_supported": False,
        "source_sha256": hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        "fixture_sha256": hashlib.sha256(dllpath.read_bytes()).hexdigest(),
        "limitations": "Only five thermal layers and four coefficients. No node wear, accumulated inputs, contact history or native thermal write plan; replay sequence is synthetic.",
    }
    a.frames.with_suffix(".thermal-replay.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
