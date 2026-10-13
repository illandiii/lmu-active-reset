"""Verify exact saved thermal and tread history on owned RAM only."""

import argparse, ctypes as C, json, hashlib
from pathlib import Path
from test_tire_thermal_recording import ThermalGrid, Bank, grid
from run_snapshot_observer_v15_25661166 import TreadHistoryGrid


class HistoryBank(C.Structure):
    _fields_ = [("prior", Bank), ("history", TreadHistoryGrid)]


def history(row):
    result = TreadHistoryGrid()
    for w, angles in enumerate(row["tread_history"]["nodes"]):
        for i, nodes in enumerate(angles):
            for j, values in enumerate(nodes):
                result.nodes[w][i][j].fields_28[:] = values["fields_28"]
                result.nodes[w][i][j].field_100 = values["field_100"]
    return result


def make_thermal_bank(dll, sequence, row):
    bank = HistoryBank()
    g = grid(row)
    h = history(row)
    save = dll.fixture_thermal_history_save
    save.argtypes = [
        C.POINTER(HistoryBank),
        C.c_uint64,
        C.POINTER(ThermalGrid),
        C.POINTER(TreadHistoryGrid),
    ]
    save.restype = C.c_int
    assert save(C.byref(bank), sequence, C.byref(g), C.byref(h))
    return bank


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    a = p.parse_args()
    rows = [json.loads(x) for x in a.frames.read_text().splitlines() if x]
    assert rows
    path = Path(__file__).resolve().parents[1] / "work/checkpoint-bank-fixture.dll"
    dll = C.CDLL(str(path))
    save = dll.fixture_thermal_history_save
    save.argtypes = [
        C.POINTER(HistoryBank),
        C.c_uint64,
        C.POINTER(ThermalGrid),
        C.POINTER(TreadHistoryGrid),
    ]
    save.restype = C.c_int
    prepare = dll.fixture_thermal_history_prepare
    prepare.argtypes = [
        C.POINTER(HistoryBank),
        C.c_uint64,
        C.POINTER(ThermalGrid),
        C.POINTER(TreadHistoryGrid),
        C.POINTER(ThermalGrid),
        C.POINTER(TreadHistoryGrid),
    ]
    prepare.restype = C.c_int
    bank = HistoryBank()
    first_grid = grid(rows[0])
    first_history = history(rows[0])
    out_grid = ThermalGrid()
    out_history = TreadHistoryGrid()
    assert save(C.byref(bank), 42, C.byref(first_grid), C.byref(first_history))
    original = bytes(bank)
    negative = 0
    changes = 0
    for row in rows:
        g = grid(row)
        h = history(row)
        changes += bytes(h) != bytes(first_history)

        def restore(sequence=42):
            return prepare(
                C.byref(bank),
                sequence,
                C.byref(g),
                C.byref(h),
                C.byref(out_grid),
                C.byref(out_history),
            )

        assert (
            restore()
            and bytes(out_grid) == bytes(first_grid)
            and bytes(out_history) == bytes(first_history)
        )
        assert not restore(43)
        negative += 1
        h.nodes[3][71][6].field_100 = float("nan")
        assert not save(C.byref(bank), 43, C.byref(g), C.byref(h)) and bytes(bank) == original
        assert (
            not restore()
            and bytes(out_grid) == bytes(first_grid)
            and bytes(out_history) == bytes(first_history)
        )
        negative += 2
        h = history(row)
        g.tires[3].cells[71][8].coefficients[3] = -1
        assert not save(C.byref(bank), 43, C.byref(g), C.byref(h)) and bytes(bank) == original
        assert (
            not restore()
            and bytes(out_grid) == bytes(first_grid)
            and bytes(out_history) == bytes(first_history)
        )
        negative += 2
    report = {
        "frames": len(rows),
        "prepared_from_first": len(rows),
        "changed_history_grids": changes,
        "negative_checks": negative,
        "tread_nodes": 2016,
        "bank_bytes": C.sizeof(HistoryBank),
        "physics_writes": 0,
        "full_restore_supported": False,
        "source_sha256": hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        "fixture_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "limits": "Numeric fields 28,30,100 only; semantics and complete tire history remain unverified. Native write plan not implemented.",
    }
    a.frames.with_suffix(".tread-history-replay.json").write_text(
        json.dumps(report, indent=2) + "\n"
    )
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
