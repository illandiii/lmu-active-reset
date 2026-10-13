"""Replay recorded mechanical frames against the production cache validator."""

import argparse
import collections
import ctypes as C
import json
from pathlib import Path

from test_checkpoint_bank_recording import (
    snapshot,
    mechanical_dependencies,
    Links,
    Dependencies,
    Saved,
    assign,
)
from run_snapshot_observer_v4_25661166 import TirePersistentState


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("frames", type=Path)
    parser.add_argument("--dll", type=Path, default=Path("work/physical-reset-host-fixture.dll"))
    args = parser.parse_args()
    library = C.CDLL(str(args.dll.resolve()))
    validate = library.checkpoint_validate_cached_mechanical
    validate.argtypes = [
        C.POINTER(Saved),
        C.c_size_t,
        C.POINTER(Links),
        C.POINTER(Dependencies),
        C.POINTER(TirePersistentState),
        C.POINTER(C.c_double),
    ]
    validate.restype = C.c_int
    counts = collections.Counter()
    negative_checks = 0
    for line in args.frames.read_text().splitlines():
        row = json.loads(line)
        state, deps = snapshot(row), mechanical_dependencies(row)
        links = Links()
        assign(links, row["links"])
        tires = (TirePersistentState * 4)()
        for i, tire in enumerate(row["tire_state"]):
            assign(tires[i], tire)
        cache = (C.c_double * 4)(*row["tire_mass_cache"])
        original = bytes(deps), bytes(tires), bytes(cache)

        def check():
            return validate(
                state.bodies, state.body_count, C.byref(links), C.byref(deps), tires, cache
            )

        assert check() == 0
        assert original == (bytes(deps), bytes(tires), bytes(cache))
        counts[row["mechanical_status"]] += 1
        for wheel in range(4):
            old = cache[wheel]
            cache[wheel] *= 1.01
            assert check() != 0
            cache[wheel] = old
            negative_checks += 1
            for axis in range(3):
                old = tires[wheel].cached_sector_inertia[axis]
                tires[wheel].cached_sector_inertia[axis] += 0.01
                assert check() != 0
                tires[wheel].cached_sector_inertia[axis] = old
                negative_checks += 1
        assert check() == 0
    assert counts
    print(
        json.dumps(
            {
                "frames": sum(counts.values()),
                "old_statuses": dict(counts),
                "corrected_status": 0,
                "corrupt_cache_rejections": negative_checks,
            }
        )
    )


if __name__ == "__main__":
    main()
