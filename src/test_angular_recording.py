"""Replay angular constraint rows from a coherent native observation."""

import argparse
import collections
import ctypes as C
import hashlib
import json
from pathlib import Path
from run_ori_observer_14200 import Saved, Geometry, Warm
from probe_bar_constraints_14200 import Prepared
from test_binding_recording import assign


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    p.add_argument("--count", type=int, choices=(2, 3), required=True)
    a = p.parse_args()
    rows = [json.loads(line) for line in a.frames.read_text().splitlines() if line]
    assert rows
    work = Path(__file__).resolve().parents[1] / "work"
    path = work / "angular-constraint-fixture.dll"
    dll = C.CDLL(str(path))
    f = dll.fixture_angular
    f.argtypes = [C.POINTER(Saved), C.POINTER(Geometry), C.c_size_t, C.POINTER(Prepared)]
    f.restype = C.c_int
    warmdll = C.CDLL(str(work / "bar-constraint-fixture.dll"))
    fw = warmdll.fixture_bar_warm
    fw.argtypes = [C.POINTER(Prepared), C.POINTER(Warm), C.c_double, C.c_double, C.POINTER(Warm)]
    fw.restype = C.c_int
    ids = collections.Counter()
    maxerr = [0.0, 0.0, 0.0, 0.0]
    mismatches = 0
    for row in rows:
        bodies = (Saved * 2)()
        g = Geometry()
        out = (Prepared * 3)()
        state = Warm()
        for i in range(2):
            assign(bodies[i], row["bodies"][i])
        for axis in range(3):
            g.axes_local[axis][:] = row["geometry"]["axes_local"][axis]
        g.softness[:] = row["geometry"]["softness"]
        assert f(bodies, C.byref(g), a.count, out) == row["prepare_status"] == 0
        for name in ("velocity", "omega"):
            for i in range(2):
                getattr(state, name)[i][:] = row["warm_before"][name][i]
        for axis, native in enumerate(row["rows"][: a.count]):
            groups = [
                ([out[axis].jacobian[i // 3][i % 3] for i in range(12)], native["native_jacobian"]),
                ([out[axis].weighted[i // 3][i % 3] for i in range(12)], native["native_weighted"]),
                ([out[axis].inverse_effective_mass], [native["native_coefficient"]]),
            ]
            for k, (ours, theirs) in enumerate(groups):
                for x, y in zip(ours, theirs):
                    maxerr[k] = max(maxerr[k], abs(x - y))
                    mismatches += abs(x - y) > 1e-10 * max(1, abs(y))
            state.impulse = native["impulse_before"]
            nxt = Warm()
            assert (
                fw(
                    C.byref(out[axis]),
                    C.byref(state),
                    native["lower"],
                    native["upper"],
                    C.byref(nxt),
                )
                == row["warm_status"]
                == 0
            )
            maxerr[3] = max(maxerr[3], abs(nxt.impulse - native["impulse_after"]))
            mismatches += abs(nxt.impulse - native["impulse_after"]) > 1e-10 * max(
                1, abs(native["impulse_after"])
            )
            state = nxt
        for name in ("velocity", "omega"):
            for i in range(2):
                for j in range(3):
                    x = getattr(state, name)[i][j]
                    y = row["warm_after"][name][i][j]
                    maxerr[3] = max(maxerr[3], abs(x - y))
                    mismatches += abs(x - y) > 1e-10 * max(1, abs(y))
        ids[str(row["constraint_index"])] += 1
        prior = bytes(out)
        g.softness[a.count - 1] = float("nan")
        assert f(bodies, C.byref(g), a.count, out) != 0 and bytes(out) == prior
    result = dict(
        source=a.frames.name,
        source_sha256=hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        fixture_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        samples=len(rows),
        constraint_indices=dict(ids),
        max_errors=dict(zip(("jacobian", "weighted", "coefficient", "warm_start"), maxerr)),
        mismatches=int(mismatches),
        negative_atomicity_checks=len(rows),
        physics_writes=0,
        limitations="Native bounds are inputs. Bias, limit reconstruction and full checkpoint restore are not tested.",
    )
    a.frames.with_suffix(".angular-replay.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    assert not mismatches


if __name__ == "__main__":
    main()
