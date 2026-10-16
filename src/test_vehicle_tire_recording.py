"""Replay coherent v2 tire partitions with the C core; no process access."""

import argparse
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
from test_tire_inertia_recording import Partition, Prepared


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    a = p.parse_args()
    rows = [json.loads(line) for line in a.frames.read_text().splitlines() if line]
    assert rows
    dllpath = Path(__file__).resolve().parents[1] / "work/tire-inertia-fixture.dll"
    f = C.CDLL(str(dllpath)).fixture_tire_inertia
    f.argtypes = [C.POINTER(Partition), C.POINTER(Prepared)]
    f.restype = C.c_int
    count = 0
    nonzero = 0
    error = 0.0
    for row in rows:
        assert (
            row["tires_before_status"] == row["tires_after_status"] == 0
            and row["tire_partition_error"] == 0
        )
        for phase in ("before", "after"):
            s = (Partition * 4)()
            out = (Prepared * 4)()
            for w, tire in enumerate(row["tires_" + phase]):
                s[w].total_table[:] = tire["total_table"]
                for i, sector in enumerate(tire["sectors"]):
                    s[w].sectors[i][:] = sector
                    nonzero += sum(x != 0 for x in sector)
            original = bytes(s)
            assert f(s, out) == 0 and bytes(s) == original
            for w in range(4):
                native = row[phase][row["root_count"] + w]["state"]["mass"]["inverse_inertia_local"]
                e = max(abs(native[j] - out[w].inverse_internal[j]) for j in range(3))
                assert e == 0
                error = max(error, e)
                count += 1
    prefix = str(a.frames).removesuffix(".frames.jsonl")
    native = [
        json.loads(line) for line in Path(prefix + ".native.jsonl").read_text().splitlines() if line
    ]
    result = dict(
        source=a.frames.name,
        source_sha256=hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        fixture_sha256=hashlib.sha256(dllpath.read_bytes()).hexdigest(),
        frames=len(rows),
        matched_tire_states=count,
        max_inverse_error=error,
        nonzero_sector_entries=nonzero,
        max_speed_kph=max(
            math.hypot(*r.get("velocity_local_mps", [0, 0, 0])) * 3.6 for r in native
        ),
        fuel_range_l=[
            min(r["fuel_l"] for r in native if "fuel_l" in r),
            max(r["fuel_l"] for r in native if "fuel_l" in r),
        ],
        physics_writes=0,
        limits="Coherent partition validation only. Zero sectors in this configuration do not validate nonzero native sectors, full tire history or live restore.",
    )
    a.frames.with_suffix(".tire-replay.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
