"""Exercise the RAM bank on recorded partial snapshots; no live game access.

Epoch values below are replay-only labels, not verified native identity tokens.
"""

import argparse
import collections
import ctypes as C
import hashlib
import json
from pathlib import Path
from run_snapshot_observer_25661166 import Saved, Mass
from constraint_wire_types import Recipe, Result, Memory
from test_binding_recording import assign
from test_mechanical_recording import Links, Dependencies


class Identity(C.Structure):
    _fields_ = [
        (n, C.c_uint64)
        for n in ("process_epoch", "session_epoch", "vehicle_epoch", "configuration_epoch")
    ] + [("executable_sha256", C.c_ubyte * 32)]


class Snapshot(C.Structure):
    _fields_ = [
        ("identity", Identity),
        ("sequence", C.c_uint64),
        ("simulation_time", C.c_double),
        ("body_count", C.c_size_t),
        ("constraint_count", C.c_size_t),
        ("bodies", Saved * 32),
        ("recipes", Recipe * 64),
        ("memory", Memory * 64),
    ]


class Bank(C.Structure):
    _fields_ = [
        ("occupied", C.c_uint64),
        ("saved", Snapshot),
        ("has_mechanical_dependencies", C.c_uint64),
        ("links", Links),
        ("dependencies", Dependencies),
    ]


class BodyPrepared(C.Structure):
    _fields_ = [("state", Saved), ("inverse_world", C.c_double * 6), ("omega", C.c_double * 3)]


class Preparation(C.Structure):
    _fields_ = [
        ("sequence", C.c_uint64),
        ("body_count", C.c_size_t),
        ("constraint_count", C.c_size_t),
        ("bodies", BodyPrepared * 32),
        ("constraints", Result * 64),
        ("memory", Memory * 64),
        ("full_restore_supported", C.c_uint64),
    ]


def snapshot(frame):
    s = Snapshot()
    s.identity = Identity(1, 1, 1, 1)
    s.identity.executable_sha256[:] = bytes.fromhex(
        "66942337813b301b87d12edfa330f33354a2a4b8cd9b048885cbf77eb6f940cf"
    )
    s.sequence = frame["sequence"]
    s.simulation_time = frame["simulation_time"]
    s.body_count = frame["total_count"]
    s.constraint_count = frame["constraint_count"]
    for i, b in enumerate(frame["bodies"]):
        assign(s.bodies[i], b)
    for i, (r, m) in enumerate(zip(frame["recipes"], frame["memory"])):
        recipe = s.recipes[i]
        recipe.identity = r["identity"]
        recipe.kind = r["kind"]
        recipe.body_indices[:] = r["body_indices"]
        g = getattr(
            recipe.geometry, "bar" if r["kind"] == 1 else "ball" if r["kind"] == 2 else "angular"
        )
        name = "anchors_local" if r["kind"] < 3 else "axes_local"
        for j, v in enumerate(r["geometry"][name]):
            getattr(g, name)[j][:] = v
        if r["kind"] == 1:
            g.softness = r["geometry"]["softness"]
        else:
            g.softness[:] = r["geometry"]["softness"]
        memory = s.memory[i]
        memory.identity = m["identity"]
        memory.kind = m["kind"]
        memory.row_count = m["row_count"]
        memory.body_indices[:] = m["body_indices"]
        for j, row in enumerate(m["rows"]):
            assign(memory.rows[j], row)
    return s


def mechanical_dependencies(frame):
    d = Dependencies()
    source = frame["dependencies"]
    assign(d.fuel, source["fuel"])
    for w in range(4):
        assign(d.wheels[w], source["wheels"][w])
        d.tires[w].total_table[:] = source["tires"][w]["total_table"]
        for sector in range(8):
            d.tires[w].sectors[sector][:] = source["tires"][w]["sectors"][sector]
    return d


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    a = p.parse_args()
    frames = [json.loads(s) for s in a.frames.read_text().splitlines() if s]
    assert frames
    dllpath = Path(__file__).resolve().parents[1] / "work/checkpoint-bank-fixture.dll"
    dll = C.CDLL(str(dllpath))
    size = dll.fixture_checkpoint_size
    size.argtypes = [C.c_uint]
    size.restype = C.c_size_t
    assert [size(i) for i in range(3)] == [C.sizeof(t) for t in (Snapshot, Bank, Preparation)]
    save = dll.fixture_checkpoint_save
    save.argtypes = [C.POINTER(Bank), C.POINTER(Snapshot)]
    save.restype = C.c_int
    prepare = dll.fixture_checkpoint_prepare
    prepare.argtypes = [
        C.POINTER(Bank),
        C.POINTER(Identity),
        C.POINTER(Mass),
        C.c_size_t,
        C.POINTER(Recipe),
        C.c_size_t,
        C.POINTER(Preparation),
    ]
    prepare.restype = C.c_int
    save_mechanical = dll.fixture_checkpoint_save_mechanical
    save_mechanical.argtypes = save.argtypes + [C.POINTER(Links), C.POINTER(Dependencies)]
    save_mechanical.restype = C.c_int
    prepare_mechanical = dll.fixture_checkpoint_prepare_mechanical
    prepare_mechanical.argtypes = prepare.argtypes[:-1] + [
        C.POINTER(Dependencies),
        C.POINTER(Preparation),
    ]
    prepare_mechanical.restype = C.c_int
    mechanical = "dependencies" in frames[0]
    bank = Bank()
    firstbank = Bank()
    far = collections.Counter()
    negative = 0
    accepted = 0
    rejected = collections.Counter()
    for index, frame in enumerate(frames):
        s = snapshot(frame)
        mass = (Mass * s.body_count)(*(s.bodies[i].mass for i in range(s.body_count)))
        out = Preparation()
        if mechanical:
            d = mechanical_dependencies(frame)
            links = Links()
            assign(links, frame["links"])

        def save_current():
            return (
                save_mechanical(C.byref(bank), C.byref(s), C.byref(links), C.byref(d))
                if mechanical
                else save(C.byref(bank), C.byref(s))
            )

        def prepare_current(source_bank):
            args = (
                C.byref(source_bank),
                C.byref(s.identity),
                mass,
                s.body_count,
                s.recipes,
                s.constraint_count,
            )
            return (
                prepare_mechanical(*args, C.byref(d), C.byref(out))
                if mechanical
                else prepare(*args, C.byref(out))
            )

        before = bytes(bank)
        status = save_current()
        if mechanical and frame["mechanical_status"]:
            assert status == 2 and bytes(bank) == before
            rejected[str(frame["mechanical_status"])] += 1
            negative += 1
            if firstbank.occupied:
                original = bytes(out)
                assert prepare_current(firstbank) == 6 and bytes(out) == original
                negative += 1
            continue
        assert status == 0
        accepted += 1
        if not firstbank.occupied:
            firstbank = Bank.from_buffer_copy(bank)
        assert prepare_current(bank) == 0
        assert not out.full_restore_supported
        for i in range(s.body_count):
            assert bytes(out.bodies[i].state) == bytes(s.bodies[i])
        for i in range(s.constraint_count):
            assert bytes(out.memory[i]) == bytes(s.memory[i])
        before = bytes(bank)
        s.memory[s.constraint_count - 1].rows[0].impulse = float("nan")
        assert save_current() == 2 and bytes(bank) == before
        negative += 1
        original = bytes(out)
        s.identity.vehicle_epoch += 1
        assert prepare_current(bank) == 4 and bytes(out) == original
        negative += 1
        s.identity.vehicle_epoch -= 1
        status = prepare_current(firstbank)
        far[str(status)] += 1
        # Uncaptured tire mass changes remain a legitimate refusal even when
        # each instantaneous inertia dependency check succeeds.
        assert status in (0, 6)
        if status:
            assert bytes(out) == original
    result = dict(
        source=a.frames.name,
        source_sha256=hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        fixture_sha256=hashlib.sha256(dllpath.read_bytes()).hexdigest(),
        frames=len(frames),
        ram_saves=accepted,
        same_snapshot_preparations=accepted,
        rejected_mechanical_statuses=dict(rejected),
        negative_checks=negative,
        first_to_current_statuses=dict(far),
        mechanical_dependencies=mechanical,
        simulation_span_s=frames[-1]["simulation_time"] - frames[0]["simulation_time"],
        physics_writes=0,
        full_restore_supported=False,
        limits="Replay epochs are synthetic. Fuel and inertia dependencies do not cover full tire, thermal or drivetrain state. No game checkpoint restore.",
    )
    a.frames.with_suffix(".bank-replay.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
