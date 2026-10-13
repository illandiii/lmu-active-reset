"""Replay numerical observations through the C core in THIS process only."""

import argparse, ctypes as C, hashlib, json, math
from pathlib import Path


class Mass(C.Structure):
    _fields_ = [("inverse_mass", C.c_double), ("inverse_inertia_local", C.c_double * 3)]


class Body(C.Structure):
    _fields_ = [
        ("mass", Mass),
        ("position_world", C.c_double * 3),
        ("velocity_world", C.c_double * 3),
        ("orientation", C.c_double * 9),
        ("momentum_world", C.c_double * 3),
    ]


class Spin(C.Structure):
    _fields_ = [
        ("angle", C.c_double),
        ("rate", C.c_double),
        ("attached", C.c_uint),
        ("deformed", C.c_uint),
    ]


class Prepared(C.Structure):
    _fields_ = [
        ("spin", Spin),
        ("orientation", C.c_double * 9),
        ("omega", C.c_double * 3),
        ("momentum", C.c_double * 3),
        ("inverse_inertia_world", C.c_double * 6),
    ]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("recording", type=Path)
    a = p.parse_args()
    root = Path(__file__).resolve().parents[1]
    dll = C.CDLL(str(root / "work/wheel-fixture.dll"))
    f = dll.fixture_wheel_prepare
    f.argtypes = [C.POINTER(Spin), C.POINTER(Body), C.POINTER(Mass), C.POINTER(Prepared)]
    f.restype = C.c_int
    validate = dll.fixture_wheel_validate
    validate.argtypes = [C.POINTER(Spin), C.POINTER(Body), C.POINTER(Body), C.POINTER(Prepared)]
    validate.restype = C.c_int
    rows = json.loads(a.recording.read_text())["rows"]
    matches = 0
    mismatches = []
    max_math_error = 0
    assert rows, "Empty recording"
    for n, row in enumerate(rows):
        spin = (Spin * 4)()
        hub = (Body * 4)()
        mass = (Mass * 4)()
        out = (Prepared * 4)()
        r = row["hub_orientation"]
        w = row["hub_omega"]
        angle = row["angle"]
        rate = row["rate"]
        for i in range(4):
            spin[i] = Spin(angle, rate, 1, 0)
            mass[i] = Mass(1, (C.c_double * 3)(1, 1, 1))
            hub[i].mass = mass[i]
            hub[i].orientation[:] = r
            hub[i].momentum_world[:] = w
        assert f(spin, hub, mass, out) == 0
        s = math.sin(angle)
        c = math.cos(angle)
        expected_r = [
            v
            for j in range(3)
            for v in (
                r[j * 3],
                c * r[j * 3 + 1] + s * r[j * 3 + 2],
                c * r[j * 3 + 2] - s * r[j * 3 + 1],
            )
        ]
        expected_w = [w[j] + rate * r[j * 3] for j in range(3)]
        error = max(
            [abs(x - y) for x, y in zip(out[0].orientation, expected_r)]
            + [abs(x - y) for x, y in zip(out[0].omega, expected_w)]
        )
        assert error < 1e-9
        max_math_error = max(max_math_error, error)
        rotation_error = max(
            abs(x - y) for x, y in zip(out[0].orientation, row["wheel_orientation"])
        )
        omega_error = max(abs(x - y) for x, y in zip(out[0].omega, row["wheel_omega"]))
        # Put the measured native binding LAST to test all-four publication.
        wheels = (Body * 4)()
        for i in range(4):
            wheels[i].mass = mass[i]
            wheels[i].orientation[:] = out[i].orientation
            wheels[i].momentum_world[:] = out[i].momentum
        wheels[3].orientation[:] = row["wheel_orientation"]
        wheels[3].momentum_world[:] = row["wheel_omega"]
        C.memset(C.addressof(out), 0xA5, C.sizeof(out))
        untouched = bytes(out)
        status = validate(spin, hub, wheels, out)
        if rotation_error < 1e-9 and omega_error < 1e-9:
            assert status == 0
            matches += 1
        else:
            assert status != 0 and bytes(out) == untouched
            mismatches.append(
                dict(
                    row=n,
                    wheel=row["wheel"],
                    rotation_error=rotation_error,
                    omega_error=omega_error,
                    rejected_without_publication=True,
                )
            )
    result = dict(
        source=a.recording.name,
        source_sha256=hashlib.sha256(a.recording.read_bytes()).hexdigest(),
        c_core_replays=len(rows),
        c_math_max_error=max_math_error,
        native_matches=matches,
        native_mismatches=mismatches,
        physics_writes=0,
        limits="Local replay; isotropic inertias used to supply observed omega. Consistency is necessary, not sufficient for a coherent checkpoint. No live restore tested.",
    )
    a.recording.with_suffix(".replay.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
