"""Replay the partial vehicle bank, preserving all captured dependencies."""

import argparse, ctypes as C, hashlib, json, math
from pathlib import Path
from test_persistent_checkpoint_recording import (
    PersistentBank,
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
    TirePersistentState,
)
from run_snapshot_observer_v5_25661166 import PowertrainPartialState
from run_snapshot_observer_v6_25661166 import TransmissionPartialState
from run_snapshot_observer_v8_25661166 import SolverHistory
from run_snapshot_observer_v10_25661166 import EngineIntegration


class VehicleBank(C.Structure):
    _fields_ = [("mechanical", PersistentBank), ("powertrain", PowertrainPartialState)]


class VehiclePreparation(C.Structure):
    _fields_ = [
        ("mechanical", Preparation),
        ("dependencies", Dependencies),
        ("tires", TirePersistentState * 4),
        ("powertrain", PowertrainPartialState),
    ]


class TransmissionBank(C.Structure):
    _fields_ = VehicleBank._fields_ + [("transmission", TransmissionPartialState)]


class TransmissionPreparation(C.Structure):
    _fields_ = VehiclePreparation._fields_ + [("transmission", TransmissionPartialState)]


class CachedMassBank(C.Structure):
    _fields_ = TransmissionBank._fields_ + [("tire_mass_cache", C.c_double * 4)]


class CachedMassPreparation(C.Structure):
    _fields_ = TransmissionPreparation._fields_ + [("tire_mass_cache", C.c_double * 4)]


class HistoryBank(C.Structure):
    _fields_ = CachedMassBank._fields_ + [("history", SolverHistory)]


class HistoryPreparation(C.Structure):
    _fields_ = CachedMassPreparation._fields_ + [("history", SolverHistory)]


class EngineBank(C.Structure):
    _fields_ = HistoryBank._fields_ + [("integration", EngineIntegration)]


class EnginePreparation(C.Structure):
    _fields_ = HistoryPreparation._fields_ + [("integration", EngineIntegration)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    p.add_argument(
        "--simulate-mass-cache",
        action="store_true",
        help="Synthetic cache = reciprocal of recorded inverse mass. NOT a native cache observation.",
    )
    p.add_argument(
        "--verify-plan",
        action="store_true",
        help="Execute the mapped-field plan on owned fixture memory only.",
    )
    a = p.parse_args()
    rows = [json.loads(x) for x in a.frames.read_text().splitlines() if x]
    assert rows
    if any(r.get("terminal_failure", 0) for r in rows):
        raise ValueError(
            "Terminal failure capture: full-bank replay is unsupported; retain native rejection evidence instead"
        )
    if a.simulate_mass_cache:
        assert all("tire_mass_cache" not in r for r in rows)
        for row in rows:
            row["tire_mass_cache"] = [
                1 / row["bodies"][i]["mass"]["inverse_mass"] for i in row["links"]["tires"]
            ]
    path = Path(__file__).resolve().parents[1] / "work/checkpoint-bank-fixture.dll"
    dll = C.CDLL(str(path))
    transmission = "transmission" in rows[0]
    BankType = TransmissionBank if transmission else VehicleBank
    OutputType = TransmissionPreparation if transmission else VehiclePreparation
    prefix = "fixture_transmission_" if transmission else "fixture_vehicle_partial_"
    extra_types = [C.POINTER(TransmissionPartialState)] if transmission else []
    cached_mass = "tire_mass_cache" in rows[0]
    if cached_mass:
        assert transmission
        BankType = CachedMassBank
        OutputType = CachedMassPreparation
        prefix = "fixture_cached_mass_"
        extra_types += [C.POINTER(C.c_double)]
    history = "history" in rows[0]
    if history:
        assert cached_mass
        BankType = HistoryBank
        OutputType = HistoryPreparation
        prefix = "fixture_history_"
        extra_types += [C.POINTER(SolverHistory)]
    engine = "integration" in rows[0]
    if engine:
        assert history
        BankType = EngineBank
        OutputType = EnginePreparation
        prefix = "fixture_engine_"
        extra_types += [C.POINTER(EngineIntegration)]
    size = getattr(dll, prefix + "size")
    size.argtypes = [C.c_uint]
    size.restype = C.c_size_t
    assert [size(i) for i in range(2)] == [C.sizeof(BankType), C.sizeof(OutputType)]
    save = getattr(dll, prefix + "save")
    save.argtypes = [
        C.POINTER(BankType),
        C.POINTER(Snapshot),
        C.POINTER(Links),
        C.POINTER(Dependencies),
        C.POINTER(TirePersistentState),
        C.POINTER(PowertrainPartialState),
    ] + extra_types
    save.restype = C.c_int
    prepare = getattr(dll, prefix + "prepare")
    prepare.argtypes = (
        [
            C.POINTER(BankType),
            C.POINTER(Identity),
            C.POINTER(Mass),
            C.c_size_t,
            C.POINTER(Recipe),
            C.c_size_t,
            C.POINTER(Dependencies),
            C.POINTER(TirePersistentState),
            C.POINTER(PowertrainPartialState),
        ]
        + extra_types
        + [C.POINTER(OutputType)]
    )
    prepare.restype = C.c_int
    bank = BankType()
    first = None
    negative = 0
    accepted = 0
    rejected = 0
    plan_result = None
    first_cache = None
    for row in rows:
        s = snapshot(row)
        d = mechanical_dependencies(row)
        l = Links()
        assign(l, row["links"])
        pstate = PowertrainPartialState()
        assign(pstate, row["powertrain"])
        if "native_cache" in row:
            from run_snapshot_observer_v12_25661166 import NativeCache

            cache_state = NativeCache()
            cache_state.sequence = s.sequence
            for i, value in enumerate(row["native_cache"]["bodies"]):
                assign(cache_state.bodies[i], value)
            for i, value in enumerate(row["native_cache"]["sectors"]):
                for j, sector in enumerate(value):
                    assign(cache_state.sectors[i][j], sector)
            valid = dll.fixture_native_cache_valid
            valid.argtypes = [C.POINTER(NativeCache), C.POINTER(type(s.bodies[0])), C.c_size_t]
            valid.restype = C.c_int
            assert valid(C.byref(cache_state), s.bodies, s.body_count) == 1
            for i in range(s.body_count):
                original_mass = cache_state.bodies[i].mass
                cache_state.bodies[i].mass *= 1.001
                assert not valid(C.byref(cache_state), s.bodies, s.body_count)
                negative += 1
                cache_state.bodies[i].mass = original_mass
            last = cache_state.sectors[3][7].aggregate[1]
            cache_state.sectors[3][7].aggregate[1] = float("nan")
            assert not valid(C.byref(cache_state), s.bodies, s.body_count)
            negative += 1
            cache_state.sectors[3][7].aggregate[1] = last
            if first_cache is None:
                first_cache = cache_state
        t = (TirePersistentState * 4)()
        for i, value in enumerate(row["tire_state"]):
            assign(t[i], value)
        tr = TransmissionPartialState()
        if transmission:
            assign(tr, row["transmission"])
        extra = [C.byref(tr)] if transmission else []
        cache = (C.c_double * 4)(*row["tire_mass_cache"]) if cached_mass else None
        if cached_mass:
            extra.append(cache)
        hist = SolverHistory()
        if history:
            hist.driveline_timer_46c8 = row["history"]["driveline_timer_46c8"]
            for i, b in enumerate(row["history"]["bodies"]):
                assign(hist.bodies[i], b)
            for i, rr in enumerate(row["history"]["rows"]):
                for j, b in enumerate(rr):
                    assign(hist.rows[i][j], b)
            extra.append(C.byref(hist))
        eg = EngineIntegration()
        if engine:
            eg.outputs_208[:] = row["integration"]["outputs_208"]
            eg.fields_240[:] = row["integration"]["fields_240"]
            eg.channel_count = row["integration"]["channel_count"]
            for i, value in enumerate(row["integration"]["channel_state"]):
                eg.channel_state[i][:] = value
            extra.append(C.byref(eg))
        m = (Mass * s.body_count)(*(s.bodies[i].mass for i in range(s.body_count)))
        out = OutputType()

        def store():
            return save(
                C.byref(bank), C.byref(s), C.byref(l), C.byref(d), t, C.byref(pstate), *extra
            )

        def restore(source):
            return prepare(
                C.byref(source),
                C.byref(s.identity),
                m,
                s.body_count,
                s.recipes,
                s.constraint_count,
                C.byref(d),
                t,
                C.byref(pstate),
                *extra,
                C.byref(out),
            )

        prior_bank = bytes(bank)
        status = store()
        # Independent oracle for the known capture-phase mismatch; do not hide
        # arbitrary implementation failures by classifying every refusal as it.
        mass_mismatch = False
        for w, tire in enumerate(row["tire_state"]):
            residual = tire["total_mass"]
            for sector_mass in tire["sector_mass"]:
                residual -= sector_mass
            expected = 1 / residual
            observed = row["bodies"][row["links"]["tires"][w]]["mass"]["inverse_mass"]
            mass_mismatch |= abs(observed - expected) > 32 * 2**-52 * abs(expected)
        if not cached_mass:
            assert (status == 2) == mass_mismatch
        else:
            assert status == 0
        if status:
            assert status == 2 and bytes(bank) == prior_bank
            rejected += 1
            negative += 1
            if first is not None:
                prior_output = bytes(out)
                assert restore(first) == 2 and bytes(out) == prior_output
                negative += 1
            continue
        accepted += 1
        if first is None:
            first = BankType.from_buffer_copy(bank)
        assert restore(bank) == 0 and restore(first) == 0
        assert bytes(out.powertrain) == bytes(first.powertrain)
        assert bytes(out.tires) == bytes(first.mechanical.tires)
        assert bytes(out.dependencies) == bytes(first.mechanical.original_dependencies)
        if transmission:
            assert bytes(out.transmission) == bytes(first.transmission)
        if cached_mass:
            assert bytes(out.tire_mass_cache) == bytes(first.tire_mass_cache)
        if history:
            assert bytes(out.history) == bytes(first.history)
        if engine:
            assert bytes(out.integration) == bytes(first.integration)
        for i in range(s.body_count):
            assert bytes(out.mechanical.bodies[i].state) == bytes(
                first.mechanical.base.saved.bodies[i]
            )
        for i in range(s.constraint_count):
            assert bytes(out.mechanical.memory[i]) == bytes(first.mechanical.base.saved.memory[i])
        assert not out.mechanical.full_restore_supported
        if a.verify_plan and plan_result is None:
            assert engine
            from test_restore_plan import verify_plan

            phase_bank = None
            if "wheel_phase" in row:
                from test_wheel_phase_recording import make_bank

                phase_bank = make_bank(dll, s, row)
            thermal_bank = None
            if "thermal_plan_status" in row:
                from test_tread_history_recording import make_thermal_bank

                thermal_bank = make_thermal_bank(dll, s.sequence, row)
            brake_bank = None
            if "brake_state" in row:
                from test_brake_recording import make_brake_bank

                brake_bank = make_brake_bank(dll, s.sequence, row)
            driveline_bank = None
            if "driveline_cache" in row:
                from test_driveline_recording import make_driveline_bank

                driveline_bank = make_driveline_bank(dll, s.sequence, row)
            outputs_bank = None
            if "driveline_outputs" in row:
                from test_driveline_outputs_recording import make_outputs_bank

                outputs_bank = make_outputs_bank(dll, s.sequence, row)
            control_bank = None
            if "powertrain_control" in row:
                from test_powertrain_control_recording import make_control_bank

                control_bank = make_control_bank(dll, s.sequence, row)
            shift_bank = None
            if "shift_observed" in row:
                from test_shift_transient_recording import make_shift_bank

                shift_bank = make_shift_bank(dll, s.sequence, row)
            plan_result = verify_plan(
                dll,
                first,
                out,
                first_cache,
                phase_bank,
                thermal_bank,
                brake_bank,
                driveline_bank,
                outputs_bank,
                control_bank,
                shift_bank,
            )
        before = bytes(bank)
        original = bytes(out)
        value = pstate.hdv_48f8
        pstate.hdv_48f8 = float("nan")
        assert store() == 2 and bytes(bank) == before
        assert restore(first) == 2 and bytes(out) == original
        negative += 2
        pstate.hdv_48f8 = value
        pstate.engine_type_rva += 8
        assert store() == 2 and bytes(bank) == before
        negative += 1
        pstate.engine_type_rva -= 8
        s.identity.session_epoch += 1
        assert restore(first) == 4 and bytes(out) == original
        negative += 1
        s.identity.session_epoch -= 1
        if transmission:
            tr.input_38 = float("nan")
            assert store() == 2 and bytes(bank) == before
            assert restore(first) == 2 and bytes(out) == original
            negative += 2
            tr.input_38 = row["transmission"]["input_38"]
        if cached_mass:
            cache[3] *= 1.001
            assert store() == 2 and bytes(bank) == before
            assert restore(first) == 2 and bytes(out) == original
            negative += 2
            cache[3] = row["tire_mass_cache"][3]
        if history:
            hist.rows[s.constraint_count - 1][0].bias = float("nan")
            assert store() == 2 and bytes(bank) == before
            assert restore(first) == 2 and bytes(out) == original
            negative += 2
            hist.rows[s.constraint_count - 1][0].bias = row["history"]["rows"][
                s.constraint_count - 1
            ][0]["bias"]
        if engine:
            eg.outputs_208[6] = float("nan")
            assert store() == 2 and bytes(bank) == before
            assert restore(first) == 2 and bytes(out) == original
            negative += 2
    ranges = {
        name: [min(r["powertrain"][name] for r in rows), max(r["powertrain"][name] for r in rows)]
        for name, _ in PowertrainPartialState._fields_
        if name != "engine_type_rva"
    }
    report = dict(
        frames=len(rows),
        first_to_current_preparations=accepted,
        rejected_incomplete_tire_mass_dependency=rejected,
        negative_checks=negative,
        span_s=rows[-1]["simulation_time"] - rows[0]["simulation_time"],
        powertrain_ranges=ranges,
        source_sha256=hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        fixture_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        physics_writes=0,
        full_restore_supported=False,
        limitations="Incomplete full-vehicle state coverage and synthetic replay identity. Win32 backend tested only on fixture-owned memory, not connected to a live-game reset. No real scoring/record preservation or moving-reset validation.",
    )
    if transmission:
        report["transmission_ranges"] = {
            name: [
                min(r["transmission"][name] for r in rows),
                max(r["transmission"][name] for r in rows),
            ]
            for name, _ in TransmissionPartialState._fields_
        }
    report["native_mass_cache"] = cached_mass
    report["solver_history"] = history
    report["engine_integration"] = engine
    if plan_result is not None:
        report["mapped_field_plan"] = plan_result
    if a.simulate_mass_cache:
        report.update(native_mass_cache=False, synthetic_mass_cache=True)
    suffix = (
        ".simulated-cache-replay.json" if a.simulate_mass_cache else ".vehicle-partial-replay.json"
    )
    a.frames.with_suffix(suffix).write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
