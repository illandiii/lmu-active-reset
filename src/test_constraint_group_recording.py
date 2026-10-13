"""Replay each coherent observation through the common constraint dispatcher.

Recordings were made at different times; they are deliberately NOT combined
into a purported simultaneous vehicle checkpoint.
"""

import argparse, collections, ctypes as C, hashlib, json
from pathlib import Path
from run_ball_observer_14200 import Saved, Geometry as BallGeometry
from run_ori_observer_14200 import Geometry as AngularGeometry
from probe_bar_constraints_14200 import Geometry as BarGeometry, Prepared
from test_binding_recording import assign


class Geometry(C.Union):
    _fields_ = [("bar", BarGeometry), ("ball", BallGeometry), ("angular", AngularGeometry)]


class Recipe(C.Structure):
    _fields_ = [
        ("identity", C.c_size_t),
        ("body_indices", C.c_size_t * 2),
        ("kind", C.c_uint),
        ("geometry", Geometry),
    ]


class Result(C.Structure):
    _fields_ = [
        ("identity", C.c_size_t),
        ("body_indices", C.c_size_t * 2),
        ("row_count", C.c_size_t),
        ("kind", C.c_uint),
        ("rows", Prepared * 3),
    ]


def replay(path, kind, f):
    samples = [json.loads(line) for line in path.read_text().splitlines() if line]
    assert samples
    ids = collections.Counter()
    maximum = 0.0
    nr = {1: 1, 2: 3, 3: 2, 4: 3}[kind]
    for frame in samples:
        b = (Saved * 2)()
        r = (Recipe * 2)()
        out = (Result * 2)()
        for i in range(2):
            assign(b[i], frame["bodies"][i])
        r[0].identity = frame["constraint_index"]
        r[0].body_indices[:] = [0, 1]
        r[0].kind = kind
        g = r[0].geometry
        if kind < 3:
            typed = g.bar if kind == 1 else g.ball
            for i in range(2):
                typed.anchors_local[i][:] = frame["geometry"]["anchors_local"][i]
            if kind == 1:
                typed.softness = frame["geometry"]["softness"]
            else:
                typed.softness[:] = frame["geometry"]["softness"]
        else:
            for i in range(3):
                g.angular.axes_local[i][:] = frame["geometry"]["axes_local"][i]
            g.angular.softness[:] = frame["geometry"]["softness"]
        assert f(b, 2, r, 1, out) == 0
        assert out[0].row_count == nr and out[0].kind == kind and out[0].identity == r[0].identity
        native = [frame] if kind == 1 else frame["rows"][:nr]
        for axis, row in enumerate(native):
            ours = out[0].rows[axis]
            pairs = [(ours.jacobian[i // 3][i % 3], row["native_jacobian"][i]) for i in range(12)]
            pairs += [(ours.weighted[i // 3][i % 3], row["native_weighted"][i]) for i in range(12)]
            pairs += [(ours.inverse_effective_mass, row["native_coefficient"])]
            for x, y in pairs:
                maximum = max(maximum, abs(x - y))
                assert abs(x - y) <= 1e-10 * max(1, abs(y)), (kind, axis, x, y)
        # A valid first item must not publish if the final item is invalid.
        r[1] = r[0]
        r[1].identity += 1000
        r[1].kind = 99
        C.memset(C.byref(out), 0xA5, C.sizeof(out))
        prior = bytes(out)
        assert f(b, 2, r, 2, out) == 3 and bytes(out) == prior
        r[1] = r[0]
        assert f(b, 2, r, 2, out) == 2 and bytes(out) == prior
        r[1].identity += 1000
        r[1].body_indices[1] = 2
        assert f(b, 2, r, 2, out) == 2 and bytes(out) == prior
        ids[str(frame["constraint_index"])] += 1
    return dict(
        source=path.name,
        sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        kind=kind,
        samples=len(samples),
        constraint_indices=dict(ids),
        max_error=maximum,
        group_failure_checks=len(samples) * 3,
    )


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for n in ("bar", "ball", "hinge", "ori"):
        p.add_argument("--" + n, type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    path = Path(__file__).resolve().parents[1] / "work/constraint-group-fixture.dll"
    dll = C.CDLL(str(path))
    f = dll.fixture_constraint_group
    f.argtypes = [C.POINTER(Saved), C.c_size_t, C.POINTER(Recipe), C.c_size_t, C.POINTER(Result)]
    f.restype = C.c_int
    reports = [
        replay(getattr(a, n), k, f) for n, k in [("bar", 1), ("ball", 2), ("hinge", 3), ("ori", 4)]
    ]
    report = dict(
        recordings=reports,
        fixture_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        samples=sum(r["samples"] for r in reports),
        group_failure_checks=sum(r["group_failure_checks"] for r in reports),
        physics_writes=0,
        limitations="Independent per-constraint captures, not one coherent vehicle snapshot. Geometry only; no bias/history/contact or live restore test.",
    )
    a.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
