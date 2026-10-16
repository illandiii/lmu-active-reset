"""Replay cached native impulses through the common ordered warm-start core."""

from sector.paths import PROJECT_ROOT

import ctypes as C
import json
import hashlib
from sector.telemetry.constraint_wire_types import Result, Prepared
from sector.observers.run_ball_observer_14200 import Saved, Geometry as BallGeometry
from sector.observers.run_ori_observer_14200 import Geometry as AngularGeometry
from sector.research.probe_bar_constraints_14200 import Geometry as BarGeometry
from sector.replay.test_binding_recording import assign


class Row(C.Structure):
    _fields_ = [(n, C.c_double) for n in ("impulse", "lower", "upper")]


class Memory(C.Structure):
    _fields_ = [
        ("identity", C.c_size_t),
        ("body_indices", C.c_size_t * 2),
        ("row_count", C.c_size_t),
        ("kind", C.c_uint),
        ("rows", Row * 3),
    ]


class Velocity(C.Structure):
    _fields_ = [("linear", C.c_double * 3), ("angular", C.c_double * 3)]


class Output(C.Structure):
    _fields_ = [
        ("body_count", C.c_size_t),
        ("constraint_count", C.c_size_t),
        ("bodies", Velocity * 32),
        ("memory", Memory * 64),
    ]


def main():
    project = PROJECT_ROOT
    work = project / "work"
    dll = C.CDLL(str(work / "constraint-warm-fixture.dll"))
    warm = dll.fixture_warm_group
    warm.argtypes = [
        C.POINTER(Velocity),
        C.c_size_t,
        C.POINTER(Result),
        C.POINTER(Memory),
        C.c_size_t,
        C.POINTER(Output),
    ]
    warm.restype = C.c_int
    configurations = [
        (1, "20261003T123446799616Z_bar_observer_v3", BarGeometry, "bar", 1),
        (2, "20261003T130104470730Z_ball_observer", BallGeometry, "ball", 3),
        (3, "20261003T130740837382Z_hinge_observer", AngularGeometry, "angular", 2),
        (4, "20261003T130619859165Z_ori_observer", AngularGeometry, "angular", 3),
    ]
    reports = []
    for kind, stem, G, libname, nr in configurations:
        geometry_dll = C.CDLL(str(work / f"{libname}-constraint-fixture.dll"))
        prepare = getattr(geometry_dll, "fixture_" + libname)
        prepare.argtypes = (
            [C.POINTER(Saved), C.POINTER(G)]
            + ([C.c_size_t] if kind >= 3 else [])
            + [C.POINTER(Prepared)]
        )
        prepare.restype = C.c_int
        path = project / "logs" / f"{stem}.frames.jsonl"
        frames = [json.loads(s) for s in path.read_text().splitlines() if s]
        maximum = 0.0
        for frame in frames:
            bodies = (Saved * 2)()
            geometry = G()
            g = Result()
            m = Memory()
            before = (Velocity * 2)()
            out = Output()
            for i in range(2):
                assign(bodies[i], frame["bodies"][i])
            field = "axes_local" if kind >= 3 else "anchors_local"
            for i, axis in enumerate(frame["geometry"][field]):
                getattr(geometry, field)[i][:] = axis
            if kind == 1:
                geometry.softness = frame["geometry"]["softness"]
            else:
                geometry.softness[:] = frame["geometry"]["softness"]
            args = [bodies, C.byref(geometry)] + ([nr] if kind >= 3 else []) + [g.rows]
            assert prepare(*args) == 0
            g.identity = m.identity = frame["constraint_index"]
            g.kind = m.kind = kind
            g.row_count = m.row_count = nr
            g.body_indices[:] = m.body_indices[:] = [0, 1]
            for i in range(2):
                before[i].linear[:] = frame["warm_before"]["velocity"][i]
                before[i].angular[:] = frame["warm_before"]["omega"][i]
            native = [frame] if kind == 1 else frame["rows"][:nr]
            for i, row in enumerate(native):
                m.rows[i] = Row(
                    frame["warm_before"]["impulse"] if kind == 1 else row["impulse_before"],
                    row["lower"],
                    row["upper"],
                )
            assert warm(before, 2, C.byref(g), C.byref(m), 1, C.byref(out)) == 0
            pairs = []
            for i in range(2):
                pairs.extend(zip(out.bodies[i].linear, frame["warm_after"]["velocity"][i]))
                pairs.extend(zip(out.bodies[i].angular, frame["warm_after"]["omega"][i]))
            pairs.extend(
                (
                    out.memory[0].rows[i].impulse,
                    frame["warm_after"]["impulse"] if kind == 1 else native[i]["impulse_after"],
                )
                for i in range(nr)
            )
            for x, y in pairs:
                maximum = max(maximum, abs(x - y))
                assert abs(x - y) <= 1e-10 * max(1, abs(y))
        reports.append(
            dict(
                kind=kind,
                samples=len(frames),
                max_error=maximum,
                source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
            )
        )
    report = dict(
        recordings=reports,
        fixture_sha256=hashlib.sha256(
            (work / "constraint-warm-fixture.dll").read_bytes()
        ).hexdigest(),
        physics_writes=0,
        limits="Previous-build recordings. Native-produced limits are inputs. No current-build live warm-start or full checkpoint restore.",
    )
    (project / "logs/20261010_warm_group_replay.json").write_text(
        json.dumps(report, indent=2) + "\n"
    )
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
