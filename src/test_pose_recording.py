"""Exercise actual C preparation on recorded 14150 numerical payloads only."""

import argparse, ctypes, hashlib, json, math
from pathlib import Path
from run_pose_observer import Mass, Saved


class Prepared(ctypes.Structure):
    _fields_ = [
        ("state", Saved),
        ("inverse_world", ctypes.c_double * 6),
        ("omega", ctypes.c_double * 3),
    ]


def make_saved(value):
    s = Saved()
    s.mass.inverse_mass = value["mass"]["inverse_mass"]
    s.mass.inverse_inertia_local[:] = value["mass"]["inverse_inertia_local"]
    for key in ("position_world", "velocity_world", "orientation", "momentum_world"):
        getattr(s, key)[:] = value[key]
    return s


def test_recording(path):
    project = Path(__file__).resolve().parents[1]
    binary = project / "work/kinematic-bin/kinematic-fixture.dll"
    dll = ctypes.CDLL(str(binary))
    fn = dll.fixture_prepare
    fn.argtypes = [
        ctypes.POINTER(Saved),
        ctypes.POINTER(Mass),
        ctypes.c_size_t,
        ctypes.POINTER(Prepared),
        ctypes.c_int,
    ]
    fn.restype = ctypes.c_int
    rows = [json.loads(line) for line in path.read_text().splitlines() if line]
    if not rows:
        raise ValueError("No frames")
    max_omega_error = 0
    changed_masses = {}
    changed_inertias = {}
    bodies = 0
    strict_rejections = 0
    for row in rows:
        n = row["total_count"]
        assert 1 <= n <= 32
        saved = (Saved * n)(*(make_saved(b["state"]) for b in row["before"]))
        current = (Mass * n)(*(make_saved(b["state"]).mass for b in row["after"]))
        out = (Prepared * n)()
        ctypes.memset(ctypes.addressof(out), 0xA5, ctypes.sizeof(out))
        untouched = bytes(out)
        saved_bytes = bytes(saved)
        current_bytes = bytes(current)
        strict = fn(saved, current, n, out, 0)
        differs = any(
            row["before"][i]["state"]["mass"] != row["after"][i]["state"]["mass"] for i in range(n)
        )
        if differs:
            assert strict == 4 and bytes(out) == untouched
            strict_rejections += 1
        else:
            assert strict == 0
        assert fn(saved, current, n, out, 1) == 0
        assert bytes(saved) == saved_bytes and bytes(current) == current_bytes
        for i in range(n):
            before = row["before"][i]
            after = row["after"][i]
            assert bytes(out[i].state.mass) == bytes(current[i])
            assert list(out[i].state.position_world) == before["state"]["position_world"]
            assert list(out[i].state.velocity_world) == before["state"]["velocity_world"]
            assert list(out[i].state.orientation) == before["state"]["orientation"]
            for a, b in zip(out[i].omega, before["omega"]):
                assert math.isfinite(a) and abs(a - b) < 1e-9
                max_omega_error = max(max_omega_error, abs(a - b))
            if before["state"]["mass"]["inverse_mass"] != after["state"]["mass"]["inverse_mass"]:
                changed_masses[i] = changed_masses.get(i, 0) + 1
            if (
                before["state"]["mass"]["inverse_inertia_local"]
                != after["state"]["mass"]["inverse_inertia_local"]
            ):
                changed_inertias[i] = changed_inertias.get(i, 0) + 1
        bodies += n
    report = {
        "source": path.name,
        "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "fixture_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "frames": len(rows),
        "body_preparations": bodies,
        "strict_mass_change_rejections": strict_rejections,
        "partial_kinematic_preparation_passed": True,
        "changed_inverse_mass_by_index": changed_masses,
        "changed_local_inertia_by_index": changed_inertias,
        "max_angular_velocity_error": max_omega_error,
        "physics_writes": 0,
        "live_restore_operations": 0,
        "limitations": "Recorded values only. Current mass/inertia retained intentionally; no saved fuel, thermal, tire-history or complete checkpoint restoration.",
    }
    (project / "logs/20260921-pose-preparation-test.json").write_text(
        json.dumps(report, indent=2) + "\n"
    )
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("frames", type=Path)
    test_recording(parser.parse_args().frames)
