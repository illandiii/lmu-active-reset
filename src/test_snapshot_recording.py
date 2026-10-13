"""Validate a typed partial snapshot from the updated executable, offline."""

import argparse, collections, ctypes as C, hashlib, json, math
from pathlib import Path
from run_snapshot_observer_25661166 import Saved
from constraint_wire_types import Recipe, Result
from test_binding_recording import assign


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    a = p.parse_args()
    rows = [json.loads(s) for s in a.frames.read_text().splitlines() if s]
    assert rows, "No snapshot captured"
    work = Path(__file__).resolve().parents[1] / "work"
    dll = C.CDLL(str(work / "constraint-group-fixture.dll"))
    f = dll.fixture_constraint_group
    f.argtypes = [C.POINTER(Saved), C.c_size_t, C.POINTER(Recipe), C.c_size_t, C.POINTER(Result)]
    f.restype = C.c_int
    maximum_inertia = maximum_omega = 0.0
    identities = None
    phase_counts = collections.Counter()
    impulse_values = []
    for frame in rows:
        nb = frame["total_count"]
        nc = frame["constraint_count"]
        bodies = (Saved * nb)()
        recipes = (Recipe * nc)()
        out = (Result * nc)()
        assert (
            frame["memory_valid"] == 1 and len(frame["memory"]) == nc and len(frame["bodies"]) == nb
        )
        for i in range(nb):
            assign(bodies[i], frame["bodies"][i])
        topology = []
        for i, (r, m) in enumerate(zip(frame["recipes"], frame["memory"])):
            recipe = recipes[i]
            recipe.identity = r["identity"]
            recipe.kind = r["kind"]
            recipe.body_indices[:] = r["body_indices"]
            assert (m["identity"], m["kind"], m["body_indices"]) == (
                r["identity"],
                r["kind"],
                r["body_indices"],
            )
            nr = {1: 1, 2: 3, 3: 2, 4: 3}[recipe.kind]
            assert m["row_count"] == nr
            g = getattr(
                recipe.geometry,
                "bar" if recipe.kind == 1 else "ball" if recipe.kind == 2 else "angular",
            )
            axes = "anchors_local" if recipe.kind < 3 else "axes_local"
            for j, v in enumerate(r["geometry"][axes]):
                getattr(g, axes)[j][:] = v
            if recipe.kind == 1:
                g.softness = r["geometry"]["softness"]
            else:
                g.softness[:] = r["geometry"]["softness"]
            for row in m["rows"][:nr]:
                assert (
                    all(math.isfinite(row[k]) for k in ("impulse", "lower", "upper"))
                    and row["lower"] <= row["upper"]
                )
                impulse_values.append(row["impulse"])
            topology.append((recipe.identity, recipe.kind, list(recipe.body_indices)))
        if identities is None:
            identities = topology
        assert identities == topology
        assert f(bodies, nb, recipes, nc, out) == frame["geometry_status"] == 0
        assert sum(r.row_count for r in out) == sum(m["row_count"] for m in frame["memory"])
        maximum_inertia = max(maximum_inertia, frame["inertia_error"])
        maximum_omega = max(maximum_omega, frame["omega_error"])
        assert frame["inertia_error"] < 1e-8 and frame["omega_error"] < 1e-8
        phase_counts[frame["context"]["reason"]] += 1
    report = dict(
        source=a.frames.name,
        sha256=hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        frames=len(rows),
        body_count=rows[0]["total_count"],
        constraint_count=rows[0]["constraint_count"],
        constraint_rows=sum(m["row_count"] for m in rows[0]["memory"]),
        impulse_values=len(impulse_values),
        nonzero_impulses=sum(v != 0 for v in impulse_values),
        max_inertia_error=maximum_inertia,
        max_omega_error=maximum_omega,
        context_reasons=dict(phase_counts),
        simulation_span_s=rows[-1]["simulation_time"] - rows[0]["simulation_time"],
        physics_writes=0,
        limitation="Coherent partial capture and local geometry preparation. Cached impulses are not applied to the recorded velocities. No full snapshot restore or subsystem completeness claim.",
    )
    a.frames.with_suffix(".snapshot-replay.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
