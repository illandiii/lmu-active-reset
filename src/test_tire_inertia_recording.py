"""Replay the native tire partition observations against the local C core."""

import argparse, ctypes as C, hashlib, json
from pathlib import Path


class Partition(C.Structure):
    _fields_ = [("total_table", C.c_float * 3), ("sectors", (C.c_double * 3) * 8)]


class Prepared(C.Structure):
    _fields_ = [(n, C.c_double * 3) for n in ("sector_sum", "internal", "inverse_internal")]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("recording", type=Path)
    a = p.parse_args()
    dllpath = Path(__file__).resolve().parents[1] / "work/tire-inertia-fixture.dll"
    f = C.CDLL(str(dllpath)).fixture_tire_inertia
    f.argtypes = [C.POINTER(Partition), C.POINTER(Prepared)]
    f.restype = C.c_int
    rows = json.loads(a.recording.read_text())["rows"]
    assert rows and len(rows) % 4 == 0
    mismatch = 0
    max_inverse_error = 0.0
    native_states = 0
    for start in range(0, len(rows), 4):
        states = (Partition * 4)()
        out = (Prepared * 4)()
        for w, row in enumerate(rows[start : start + 4]):
            assert row["wheel"] == w and row["sample"] == rows[start]["sample"]
            states[w].total_table[:] = row["total_table_inertia"]
            for i, s in enumerate(row["sector_inertia"]):
                states[w].sectors[i][:] = s
        original = bytes(states)
        assert f(states, out) == 0 and bytes(states) == original
        for w, row in enumerate(rows[start : start + 4]):
            assert list(out[w].sector_sum) == row["cached_sector_sum"]
            assert list(out[w].internal) == row["internal_inertia"]
            error = max(
                abs(out[w].inverse_internal[j] - row["inverse_inertia"][j]) for j in range(3)
            )
            mismatch += error != 0
            max_inverse_error = max(max_inverse_error, error)
            native_states += 1
        # A wrong final sector must not publish the three earlier valid tires.
        prior = bytes(out)
        states[3].sectors[7][2] = float("nan")
        assert f(states, out) == 2 and bytes(out) == prior
    result = dict(
        source=a.recording.name,
        source_sha256=hashlib.sha256(a.recording.read_bytes()).hexdigest(),
        fixture_sha256=hashlib.sha256(dllpath.read_bytes()).hexdigest(),
        native_partition_matches=native_states,
        asynchronous_inverse_mismatch_rows=mismatch,
        max_asynchronous_inverse_error=max_inverse_error,
        last_tire_rejections=len(rows) // 4,
        physics_writes=0,
        limits="Native residuals and cached sums match exactly. Inverse reads can cross native update phases; no coherent checkpoint or live inertia write is validated.",
    )
    a.recording.with_suffix(".replay.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
