"""Replay coherent vehicle geometry preparation without accessing LMU."""

import argparse, collections, ctypes as C, hashlib, json
from pathlib import Path
from test_mechanical_recording import (
    Body,
    Mass,
    Prepared,
    FuelPrepared,
    Links,
    Dependencies,
    deps,
    assign,
)
from constraint_wire_types import Recipe, Result


class Integrated(C.Structure):
    _fields_ = [
        ("body_count", C.c_size_t),
        ("constraint_count", C.c_size_t),
        ("row_count", C.c_size_t),
        ("bodies", Prepared * 32),
        ("fuel", FuelPrepared),
        ("constraints", Result * 64),
    ]


def recipes(frame, phase):
    data = frame["constraints_" + phase]
    r = (Recipe * len(data))()
    for i, v in enumerate(data):
        r[i].identity = v["identity"]
        r[i].body_indices[:] = v["body_indices"]
        r[i].kind = v["kind"]
        name = "bar" if v["kind"] == 1 else "ball" if v["kind"] == 2 else "angular"
        g = getattr(r[i].geometry, name)
        source = v["geometry"]
        axes = "axes_local" if v["kind"] >= 3 else "anchors_local"
        for j, a in enumerate(source[axes]):
            getattr(g, axes)[j][:] = a
        if v["kind"] == 1:
            g.softness = source["softness"]
        else:
            g.softness[:] = source["softness"]
    return r


def topology(frame):
    return [(r["identity"], r["kind"], r["body_indices"]) for r in frame["constraints_before"]]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    a = p.parse_args()
    path = Path(__file__).resolve().parents[1] / "work/mechanical-constraint-fixture.dll"
    dll = C.CDLL(str(path))
    f = dll.fixture_mechanical_constraint
    f.argtypes = [
        C.POINTER(Body),
        C.POINTER(Mass),
        C.c_size_t,
        C.POINTER(Links),
        C.POINTER(Dependencies),
        C.POINTER(Dependencies),
        C.POINTER(Recipe),
        C.c_size_t,
        C.POINTER(Integrated),
    ]
    f.restype = C.c_int
    frames = [json.loads(line) for line in a.frames.read_text().splitlines() if line]
    assert frames
    statuses = collections.Counter()
    far = collections.Counter()
    first = frames[0]
    negative = 0
    for frame in frames:
        assert frame["phase_errors"] == 0 and frame["prepare_mechanical"] == 0
        assert topology(first) == topology(frame)
        n = frame["total_count"]
        nr = frame["constraint_count"]
        links = Links()
        links.fuel = frame["energy_before"]["fuel_body_index"]
        links.wheels[:] = frame["wheel_body_index"]
        links.tires[:] = range(frame["root_count"], n)
        current = (Mass * n)()
        for i in range(n):
            assign(current[i], frame["after"][i]["state"]["mass"])
        now = deps(frame, "after")
        for source, counter in ((frame, statuses), (first, far)):
            saved = (Body * n)()
            for i in range(n):
                assign(saved[i], source["before"][i]["state"])
            previous = deps(source, "before")
            r = recipes(source, "before")
            out = Integrated()
            inputs = bytes(saved), bytes(current), bytes(previous), bytes(now), bytes(r)
            result = f(
                saved,
                current,
                n,
                C.byref(links),
                C.byref(previous),
                C.byref(now),
                r,
                nr,
                C.byref(out),
            )
            counter[str(result)] += 1
            assert result == 0
            if source is frame:
                assert result == frame["prepare_integrated"]
            assert inputs == (bytes(saved), bytes(current), bytes(previous), bytes(now), bytes(r))
            assert (
                out.body_count == n
                and out.constraint_count == nr
                and out.row_count == frame["integrated_row_count"] == 49
            )
            for i in range(n):
                assert bytes(out.bodies[i].state) == bytes(saved[i])
            for i in range(nr):
                assert (
                    out.constraints[i].identity == r[i].identity
                    and out.constraints[i].kind == r[i].kind
                )
                assert list(out.constraints[i].body_indices) == list(r[i].body_indices)
            # A malformed final recipe must not publish the already prepared bodies.
            prior = bytes(out)
            r[nr - 1].kind = 99
            assert (
                f(
                    saved,
                    current,
                    n,
                    C.byref(links),
                    C.byref(previous),
                    C.byref(now),
                    r,
                    nr,
                    C.byref(out),
                )
                == 3
                and bytes(out) == prior
            )
            negative += 1
    report = dict(
        source=a.frames.name,
        source_sha256=hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        fixture_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        frames=len(frames),
        body_count=first["total_count"],
        constraint_count=first["constraint_count"],
        row_count=first["integrated_row_count"],
        kinds=dict(collections.Counter(r["kind"] for r in first["constraints_before"])),
        adjacent_statuses=dict(statuses),
        first_to_current_statuses=dict(far),
        negative_atomicity_checks=negative,
        time_span_s=frames[-1]["simulation_time"] - first["simulation_time"],
        physics_writes=0,
        limits="Coherent numerical preparation only. Persistent constraint state, other vehicle subsystems, contacts after teleport and actual game restore are not covered.",
    )
    a.frames.with_suffix(".integrated-replay.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
