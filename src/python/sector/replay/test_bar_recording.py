"""Compare coherent native Bar caches to the independent local geometry core."""

from sector.paths import PROJECT_ROOT

import argparse
import collections
import ctypes as C
import hashlib
import json
from pathlib import Path
from sector.research.probe_bar_constraints_14200 import Body, Geometry, Prepared
from sector.replay.test_binding_recording import assign


class Warm(C.Structure):
    _fields_ = [
        ("velocity", (C.c_double * 3) * 2),
        ("omega", (C.c_double * 3) * 2),
        ("impulse", C.c_double),
    ]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    a = p.parse_args()
    rows = [json.loads(line) for line in a.frames.read_text().splitlines() if line]
    assert rows
    path = PROJECT_ROOT / "work/bar-constraint-fixture.dll"
    dll = C.CDLL(str(path))
    f = dll.fixture_bar
    f.argtypes = [C.POINTER(Body), C.POINTER(Geometry), C.POINTER(Prepared)]
    f.restype = C.c_int
    fw = dll.fixture_bar_warm
    fw.argtypes = [C.POINTER(Prepared), C.POINTER(Warm), C.c_double, C.c_double, C.POINTER(Warm)]
    fw.restype = C.c_int
    ids = collections.Counter()
    maxerr = [0.0, 0.0, 0.0]
    relative = 0.0
    negative = 0
    warm_checks = 0
    warm_mismatches = 0
    max_warm_error = 0.0
    for row in rows:
        b = (Body * 2)()
        g = Geometry()
        out = Prepared()
        for i in range(2):
            assign(b[i], row["bodies"][i])
            g.anchors_local[i][:] = row["geometry"]["anchors_local"][i]
        g.softness = row["geometry"]["softness"]
        assert f(b, C.byref(g), C.byref(out)) == row["prepare_status"] == 0
        groups = [
            ([out.jacobian[i // 3][i % 3] for i in range(12)], row["native_jacobian"]),
            ([out.weighted[i // 3][i % 3] for i in range(12)], row["native_weighted"]),
            ([out.inverse_effective_mass], [row["native_coefficient"]]),
        ]
        for k, (ours, native) in enumerate(groups):
            for x, y in zip(ours, native):
                err = abs(x - y)
                maxerr[k] = max(maxerr[k], err)
                relative = max(relative, err / max(1, abs(y)))
                assert err <= 1e-10 * max(1, abs(y)), (row["constraint_index"], k, x, y)
        if "warm_before" in row:
            wb = Warm()
            wo = Warm()
            wb.impulse = row["warm_before"]["impulse"]
            for n in ("velocity", "omega"):
                for i in range(2):
                    getattr(wb, n)[i][:] = row["warm_before"][n][i]
            assert (
                fw(C.byref(out), C.byref(wb), row["lower"], row["upper"], C.byref(wo))
                == row["warm_status"]
                == 0
            )
            pairs = [(wo.impulse, row["warm_after"]["impulse"])]
            for n in ("velocity", "omega"):
                pairs.extend(
                    (getattr(wo, n)[i][j], row["warm_after"][n][i][j])
                    for i in range(2)
                    for j in range(3)
                )
            errors = [abs(x - y) for x, y in pairs]
            max_warm_error = max(max_warm_error, *errors)
            warm_mismatches += any(abs(x - y) > 1e-10 * max(1, abs(y)) for x, y in pairs)
            warm_checks += 1
        ids[str(row["constraint_index"])] += 1
        prior = bytes(out)
        g.anchors_local[1][2] = float("nan")
        assert f(b, C.byref(g), C.byref(out)) != 0 and bytes(out) == prior
        negative += 1
    result = dict(
        source=a.frames.name,
        sha256=hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        fixture_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        samples=len(rows),
        constraint_indices=dict(ids),
        max_jacobian_error=maxerr[0],
        max_weighted_error=maxerr[1],
        max_coefficient_error=maxerr[2],
        max_scaled_error=relative,
        negative_atomicity_checks=negative,
        warm_start_checks=warm_checks,
        warm_start_mismatches=warm_mismatches,
        max_warm_error=max_warm_error,
        physics_writes=0,
        limits="Coherent geometry and optional clamp/apply comparison only. Native-produced bounds are inputs; bias/limit reconstruction, saved impulse coverage and other constraint types remain separate.",
    )
    a.frames.with_suffix(".bar-replay.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    assert warm_mismatches == 0, "Native warm-start mismatch; inspect saved report"


if __name__ == "__main__":
    main()
