"""Replay persistent tire checkpoints. No live writes; identities are synthetic."""

from sector.paths import PROJECT_ROOT

import argparse
import collections
import ctypes as C
import hashlib
import json
from pathlib import Path
from sector.replay.test_checkpoint_bank_recording import (
    Bank,
    Snapshot,
    Identity,
    Preparation,
    Mass,
    Recipe,
    Links,
    Dependencies,
    snapshot,
    mechanical_dependencies,
    assign,
)
from sector.observers.run_snapshot_observer_v4_25661166 import TirePersistentState


class PersistentBank(C.Structure):
    _fields_ = [
        ("base", Bank),
        ("original_dependencies", Dependencies),
        ("tires", TirePersistentState * 4),
    ]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    a = p.parse_args()
    path = PROJECT_ROOT / "work/checkpoint-bank-fixture.dll"
    dll = C.CDLL(str(path))
    size = dll.fixture_persistent_size
    size.restype = C.c_size_t
    assert size() == C.sizeof(PersistentBank)
    save = dll.fixture_persistent_save
    save.argtypes = [
        C.POINTER(PersistentBank),
        C.POINTER(Snapshot),
        C.POINTER(Links),
        C.POINTER(Dependencies),
        C.POINTER(TirePersistentState),
    ]
    save.restype = C.c_int
    prepare = dll.fixture_persistent_prepare
    prepare.argtypes = [
        C.POINTER(PersistentBank),
        C.POINTER(Identity),
        C.POINTER(Mass),
        C.c_size_t,
        C.POINTER(Recipe),
        C.c_size_t,
        C.POINTER(Dependencies),
        C.POINTER(TirePersistentState),
        C.POINTER(Preparation),
    ]
    prepare.restype = C.c_int
    rows = [json.loads(line) for line in a.frames.read_text().splitlines() if line]
    assert rows
    bank = PersistentBank()
    first = None
    statuses = collections.Counter()
    negative = 0
    for row in rows:
        s = snapshot(row)
        d = mechanical_dependencies(row)
        links = Links()
        assign(links, row["links"])
        tires = (TirePersistentState * 4)()
        for i, t in enumerate(row["tire_state"]):
            assign(tires[i], t)
        mass = (Mass * s.body_count)(*(s.bodies[i].mass for i in range(s.body_count)))
        out = Preparation()
        assert save(C.byref(bank), C.byref(s), C.byref(links), C.byref(d), tires) == 0
        assert bytes(bank.original_dependencies) == bytes(d) and bytes(bank.tires) == bytes(tires)
        if first is None:
            first = PersistentBank.from_buffer_copy(bank)

        def restore(source):
            return prepare(
                C.byref(source),
                C.byref(s.identity),
                mass,
                s.body_count,
                s.recipes,
                s.constraint_count,
                C.byref(d),
                tires,
                C.byref(out),
            )

        assert restore(bank) == 0
        assert restore(first) == 0
        statuses["0"] += 1
        for i in range(s.body_count):
            assert bytes(out.bodies[i].state) == bytes(first.base.saved.bodies[i])
        assert not out.full_restore_supported
        prior = bytes(out)
        before = bytes(bank)
        original = tires[3].sector_mass[7]
        tires[3].sector_mass[7] = float("nan")
        assert restore(first) != 0 and bytes(out) == prior
        assert (
            save(C.byref(bank), C.byref(s), C.byref(links), C.byref(d), tires) != 0
            and bytes(bank) == before
        )
        negative += 2
        tires[3].sector_mass[7] = original
        s.identity.session_epoch += 1
        assert restore(first) == 4 and bytes(out) == prior
        negative += 1
        s.identity.session_epoch -= 1
        mass[0].inverse_mass *= 1.01
        assert restore(first) == 6 and bytes(out) == prior
        negative += 1
    report = dict(
        frames=len(rows),
        first_to_current_statuses=dict(statuses),
        negative_checks=negative,
        span_s=rows[-1]["simulation_time"] - rows[0]["simulation_time"],
        source_sha256=hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        fixture_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        physics_writes=0,
        full_restore_supported=False,
        limits="Numerical RAM replay only. Synthetic identities. Full thermal, drivetrain, damage state and live write adapter remain incomplete.",
    )
    a.frames.with_suffix(".persistent-bank-replay.json").write_text(
        json.dumps(report, indent=2) + "\n"
    )
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
