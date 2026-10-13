"""Replay the coherent mechanical dependency preparation; never accesses LMU."""

import argparse, collections, ctypes as C, hashlib, json
from pathlib import Path
from test_vehicle_recording import Fuel, FuelPrepared, Prepared, Body, Mass
from test_tire_inertia_recording import Partition
from test_binding_recording import assign


class WheelInputs(C.Structure):
    _fields_ = [(n, C.c_double) for n in ("base_130", "term_4b8", "term_5d8", "factor_4f0")]


class Dependencies(C.Structure):
    _fields_ = [("fuel", Fuel), ("wheels", WheelInputs * 4), ("tires", Partition * 4)]


class Links(C.Structure):
    _fields_ = [("fuel", C.c_size_t), ("wheels", C.c_size_t * 4), ("tires", C.c_size_t * 4)]


def deps(row, phase):
    d = Dependencies()
    assign(d.fuel, row["energy_" + phase]["fuel"])
    for w in range(4):
        assign(d.wheels[w], row["wheels_" + phase][w])
        tire = row["tires_" + phase][w]
        d.tires[w].total_table[:] = tire["total_table"]
        for i in range(8):
            d.tires[w].sectors[i][:] = tire["sectors"][i]
    return d


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    a = p.parse_args()
    path = Path(__file__).resolve().parents[1] / "work/mechanical-prepare-fixture.dll"
    dll = C.CDLL(str(path))
    f = dll.fixture_mechanical
    f.argtypes = [
        C.POINTER(Body),
        C.POINTER(Mass),
        C.c_size_t,
        C.POINTER(Links),
        C.POINTER(Dependencies),
        C.POINTER(Dependencies),
        C.POINTER(Prepared),
        C.POINTER(FuelPrepared),
    ]
    f.restype = C.c_int
    rows = [json.loads(s) for s in a.frames.read_text().splitlines() if s]
    assert rows
    statuses = collections.Counter()
    far = collections.Counter()
    negatives = 0
    first = rows[0]
    for row in rows:
        n = row["total_count"]
        saved = (Body * n)()
        current = (Mass * n)()
        out = (Prepared * n)()
        fuel = FuelPrepared()
        links = Links()
        links.fuel = row["energy_before"]["fuel_body_index"]
        links.wheels[:] = row["wheel_body_index"]
        links.tires[:] = range(row["root_count"], n)
        sa = deps(row, "before")
        sb = deps(row, "after")
        for i in range(n):
            assign(saved[i], row["before"][i]["state"])
            assign(current[i], row["after"][i]["state"]["mass"])
        original = (bytes(saved), bytes(current), bytes(sa), bytes(sb))
        status = f(saved, current, n, C.byref(links), C.byref(sa), C.byref(sb), out, C.byref(fuel))
        assert status == row["prepare_mechanical"]
        statuses[str(status)] += 1
        assert original == (bytes(saved), bytes(current), bytes(sa), bytes(sb))
        if status == 0:
            for i in range(n):
                assert bytes(out[i].state) == bytes(saved[i])
        prior = (bytes(out), bytes(fuel))
        current[links.wheels[3]].inverse_inertia_local[2] *= 1.1
        assert f(
            saved, current, n, C.byref(links), C.byref(sa), C.byref(sb), out, C.byref(fuel)
        ) != 0 and prior == (bytes(out), bytes(fuel))
        negatives += 1
        # A longer gap tests data dependencies, not merely adjacent substeps.
        assert (
            first["root_count"] == row["root_count"]
            and first["wheel_body_index"] == row["wheel_body_index"]
        )
        for i in range(n):
            assign(saved[i], first["before"][i]["state"])
            assign(current[i], row["after"][i]["state"]["mass"])
        initial = deps(first, "before")
        prior = (bytes(out), bytes(fuel))
        status = f(
            saved, current, n, C.byref(links), C.byref(initial), C.byref(sb), out, C.byref(fuel)
        )
        far[str(status)] += 1
        if status:
            assert prior == (bytes(out), bytes(fuel))
    result = dict(
        source=a.frames.name,
        sha256=hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        fixture_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        frames=len(rows),
        adjacent_statuses=dict(statuses),
        first_to_current_statuses=dict(far),
        negative_atomicity_checks=negatives,
        time_span_s=rows[-1]["simulation_time"] - rows[0]["simulation_time"],
        physics_writes=0,
        limits="Body and declared mass/inertia dependencies prepared locally only. Not a full tire/brake/history/constraint/drivetrain checkpoint, scoring test or live restore.",
    )
    a.frames.with_suffix(".mechanical-replay.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
