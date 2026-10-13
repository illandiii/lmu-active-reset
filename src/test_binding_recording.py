"""Replay same-thread recorded bodies with their actual masses and momenta."""

import argparse, ctypes as C, hashlib, json
from pathlib import Path
from test_wheel_recording import Body, Spin, Prepared


def assign(target, data):
    for key, kind in target._fields_:
        value = data[key]
        field = getattr(target, key)
        if isinstance(field, C.Structure):
            assign(field, value)
        elif isinstance(field, C.Array):
            field[:] = value
        else:
            setattr(target, key, value)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    a = p.parse_args()
    dllpath = Path(__file__).resolve().parents[1] / "work/wheel-fixture.dll"
    dll = C.CDLL(str(dllpath))
    f = dll.fixture_wheel_validate
    f.argtypes = [C.POINTER(Spin), C.POINTER(Body), C.POINTER(Body), C.POINTER(Prepared)]
    f.restype = C.c_int
    rows = [json.loads(line) for line in a.frames.read_text().splitlines() if line]
    assert rows, "Empty recording"
    groups = 0
    negative = 0
    for row in rows:
        for phase in ("before", "after", "last_transform"):
            sample = row[phase]
            spins = (Spin * 4)()
            hubs = (Body * 4)()
            wheels = (Body * 4)()
            out = (Prepared * 4)()
            for i in range(4):
                assign(spins[i], sample["spins"][i])
                assign(hubs[i], sample["hubs"][i])
                assign(wheels[i], sample["wheels"][i])
            status = f(spins, hubs, wheels, out)
            assert status == sample["validation"], (phase, status, sample["validation"])
            if status:
                continue
            groups += 1
            # A changed final wheel rate must reject the entire recorded group.
            spins[3].rate += 1
            C.memset(C.addressof(out), 0xA5, C.sizeof(out))
            original = bytes(out)
            assert f(spins, hubs, wheels, out) != 0 and bytes(out) == original
            negative += 1
    result = dict(
        source=a.frames.name,
        source_sha256=hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        fixture_sha256=hashlib.sha256(dllpath.read_bytes()).hexdigest(),
        frames=len(rows),
        validated_four_wheel_groups=groups,
        wheel_states=groups * 4,
        inconsistent_last_wheel_rejections=negative,
        physics_writes=0,
        limits="Local replay of recorded actual masses, inertias and momenta. No native restore, contact regeneration or lap preservation test.",
    )
    a.frames.with_suffix(".replay.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
