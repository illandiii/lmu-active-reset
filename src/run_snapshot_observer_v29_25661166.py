"""Coherent numerical capture and phase observation only; no pose writes."""

import argparse
import ctypes
import datetime
import hashlib
import json
import subprocess
import time
from pathlib import Path
from capture_context import PROJECT, KERNEL, get_json
from capture_preparation_steam25661166 import verify_telemetry
from context_gate import assess_player_observation as assess
from probe_contacts_14200 import context
from run_physics_observer import k, modules, read_process, load_observer

from constraint_wire_types import Recipe, Memory


class Mass(ctypes.Structure):
    _fields_ = [("inverse_mass", ctypes.c_double), ("inverse_inertia_local", ctypes.c_double * 3)]


class Saved(ctypes.Structure):
    _fields_ = [
        ("mass", Mass),
        ("position_world", ctypes.c_double * 3),
        ("velocity_world", ctypes.c_double * 3),
        ("orientation", ctypes.c_double * 9),
        ("momentum_world", ctypes.c_double * 3),
    ]


class Body(ctypes.Structure):
    _fields_ = [
        ("state", Saved),
        ("inverse_world", ctypes.c_double * 6),
        ("omega", ctypes.c_double * 3),
        ("force", ctypes.c_double * 3),
        ("torque", ctypes.c_double * 3),
        ("renormalization_counter", ctypes.c_int64),
    ]


class Fuel(ctypes.Structure):
    _fields_ = [
        (n, ctypes.c_double)
        for n in ("quantity", "quantity_rate", "density", "base_mass", "additional_mass")
    ]


class Energy(ctypes.Structure):
    _fields_ = [
        ("fuel", Fuel),
        ("battery", ctypes.c_double * 4),
        ("fuel_body_mass", ctypes.c_double),
        ("fuel_body_index", ctypes.c_int64),
        ("valid", ctypes.c_uint64),
    ]


class Partition(ctypes.Structure):
    _fields_ = [("total_table", ctypes.c_float * 3), ("sectors", (ctypes.c_double * 3) * 8)]


class WheelInputs(ctypes.Structure):
    _fields_ = [(n, ctypes.c_double) for n in ("base_130", "term_4b8", "term_5d8", "factor_4f0")]


class Dependencies(ctypes.Structure):
    _fields_ = [("fuel", Fuel), ("wheels", WheelInputs * 4), ("tires", Partition * 4)]


class Links(ctypes.Structure):
    _fields_ = [
        ("fuel", ctypes.c_size_t),
        ("wheels", ctypes.c_size_t * 4),
        ("tires", ctypes.c_size_t * 4),
    ]


class TirePersistentState(ctypes.Structure):
    _fields_ = [
        ("total_mass", ctypes.c_double),
        ("sector_mass", ctypes.c_double * 8),
        ("cached_sector_inertia", ctypes.c_double * 3),
        ("update_phase", ctypes.c_uint32),
        ("reserved", ctypes.c_uint32),
    ] + [(n, ctypes.c_double) for n in ("thermal_fb0", "thermal_fb8", "accumulated_fc0")]


class PowertrainPartialState(ctypes.Structure):
    _fields_ = [("engine_type_rva", ctypes.c_uint64)] + [
        (n, ctypes.c_double)
        for n in (
            "engine_1f8",
            "engine_250",
            "engine_260",
            "engine_280",
            "hdv_48a0",
            "hdv_48a8",
            "hdv_48b0",
            "hdv_48b8",
            "hdv_48e8",
            "hdv_48f0",
            "hdv_48f8",
        )
    ]


class TransmissionPartialState(ctypes.Structure):
    _fields_ = [
        (n, ctypes.c_double)
        for n in ("shaft_0", "shaft_8", "input_18", "input_20", "input_30", "input_38")
    ] + [("gear_64", ctypes.c_int32), ("reserved", ctypes.c_uint32)]


class BodyHistory(ctypes.Structure):
    _fields_ = [
        ("force", ctypes.c_double * 3),
        ("torque", ctypes.c_double * 3),
        ("renormalization", ctypes.c_uint32),
        ("reserved", ctypes.c_uint32),
    ]


class SolverRowHistory(ctypes.Structure):
    _fields_ = [
        ("bias", ctypes.c_double),
        ("coefficient", ctypes.c_double),
        ("jacobian", ctypes.c_double * 12),
        ("weighted", ctypes.c_double * 12),
    ]


class SolverHistory(ctypes.Structure):
    _fields_ = [
        ("bodies", BodyHistory * 32),
        ("rows", (SolverRowHistory * 3) * 64),
        ("driveline_timer_46c8", ctypes.c_double),
    ]


class EngineIntegration(ctypes.Structure):
    _fields_ = [
        ("outputs_208", ctypes.c_double * 7),
        ("channel_state", (ctypes.c_double * 2) * 2),
        ("fields_240", ctypes.c_float * 3),
        ("channel_count", ctypes.c_uint32),
    ]


class NativeBodyCache(ctypes.Structure):
    _fields_ = [
        ("mass", ctypes.c_double),
        ("inertia_local", ctypes.c_double * 3),
        ("inverse_world", ctypes.c_double * 6),
        ("omega", ctypes.c_double * 3),
    ]


class NativeSectorCache(ctypes.Structure):
    _fields_ = [("first_moment", ctypes.c_double * 3), ("aggregate", ctypes.c_double * 2)]


class NativeCache(ctypes.Structure):
    _fields_ = [
        ("sequence", ctypes.c_uint64),
        ("bodies", NativeBodyCache * 32),
        ("sectors", (NativeSectorCache * 8) * 4),
    ]


class ThermalCell(ctypes.Structure):
    _fields_ = [("layers", ctypes.c_double * 5), ("coefficients", ctypes.c_double * 4)]


class TireThermalGrid(ctypes.Structure):
    _fields_ = [("angular_index", ctypes.c_uint32 * 72), ("cells", (ThermalCell * 9) * 72)]


class ThermalGrid(ctypes.Structure):
    _fields_ = [("tires", TireThermalGrid * 4)]


class TreadHistory(ctypes.Structure):
    _fields_ = [("fields_28", ctypes.c_double * 2), ("field_100", ctypes.c_double)]


class TreadHistoryGrid(ctypes.Structure):
    _fields_ = [("nodes", ((TreadHistory * 7) * 72) * 4)]


class WheelSavedSpin(ctypes.Structure):
    _fields_ = [
        ("angle", ctypes.c_double),
        ("angular_rate", ctypes.c_double),
        ("attached", ctypes.c_uint32),
        ("deformed", ctypes.c_uint32),
    ]


class WheelPhase(ctypes.Structure):
    _fields_ = [
        ("spin", WheelSavedSpin),
        ("hub_index", ctypes.c_uint32),
        ("wheel_index", ctypes.c_uint32),
    ]


class BrakeState(ctypes.Structure):
    _fields_ = [
        ("fields_5d8", ctypes.c_double * 5),
        ("field_630", ctypes.c_double),
        ("cache_138", ctypes.c_double),
    ]


class DrivelineCache(ctypes.Structure):
    _fields_ = [
        ("torque", ctypes.c_double * 3),
        ("drive_type", ctypes.c_uint32),
        ("reserved", ctypes.c_uint32),
    ]


class DrivelineOutputs(ctypes.Structure):
    _fields_ = [
        (name, ctypes.c_double) for name in ("hdv_46d0", "hdv_46e0", "hdv_46e8", "hdv_4b38")
    ]


class PowertrainControl(ctypes.Structure):
    _fields_ = [
        (n, ctypes.c_double)
        for n in ("ramp_d0", "timer_e8", "factor_f0", "factor_f8", "engine_50", "engine_1b0")
    ] + [
        ("index_d8", ctypes.c_uint32),
        ("index_dc", ctypes.c_uint32),
        ("active_e0", ctypes.c_uint8),
        ("latched_e1", ctypes.c_uint8),
        ("reserved", ctypes.c_uint8 * 6),
        ("table_counts", ctypes.c_uint32 * 4),
        ("table_token", ctypes.c_uint64),
    ]


class ShiftObserved(ctypes.Structure):
    _fields_ = [
        (n, ctypes.c_double)
        for n in ("timer_4ac0", "control_4668", "clock_a8", "event_50", "dt_b0")
    ] + [
        ("accumulators", ctypes.c_float * 2),
        ("gears", ctypes.c_int32 * 6),
        ("notification", ctypes.c_int32 * 2),
        ("request", ctypes.c_int32 * 2),
    ]


class Frame(ctypes.Structure):
    _fields_ = (
        [
            (n, ctypes.c_uint64)
            for n in (
                "tick_ms",
                "root_count",
                "total_count",
                "constraint_count",
                "geometry_status",
                "memory_valid",
            )
        ]
        + [
            (n, ctypes.c_double)
            for n in ("simulation_time", "observer_us", "inertia_error", "omega_error")
        ]
        + [
            ("bodies", Saved * 32),
            ("recipes", Recipe * 64),
            ("memory", Memory * 64),
            ("dependencies", Dependencies),
            ("links", Links),
            ("mechanical_status", ctypes.c_uint64),
            ("tire_state", TirePersistentState * 4),
            ("powertrain", PowertrainPartialState),
            ("transmission", TransmissionPartialState),
            ("tire_mass_cache", ctypes.c_double * 4),
            ("history", SolverHistory),
        ]
        + [
            (n, ctypes.c_uint64)
            for n in (
                "topology_token",
                "configuration_token",
                "bank_generation",
                "bank_saved_sequence",
                "bank_status",
                "bank_saves",
                "bank_preparations",
                "bank_rejections",
            )
        ]
        + [("integration", EngineIntegration)]
        + [(n, ctypes.c_uint64) for n in ("plan_status", "plan_fields", "plan_bytes")]
        + [
            ("native_cache", NativeCache),
            ("binding_status", ctypes.c_uint64),
            ("thermal_grid", ThermalGrid),
            ("tread_history", TreadHistoryGrid),
            ("wheel_phase", WheelPhase * 4),
            ("thermal_plan_status", ctypes.c_uint64),
            ("brake_state", BrakeState * 4),
            ("driveline_cache", DrivelineCache),
            ("driveline_outputs", DrivelineOutputs),
            ("terminal_failure", ctypes.c_uint32),
            ("reserved_terminal", ctypes.c_uint32),
            ("powertrain_control", PowertrainControl),
            ("shift_observed", ShiftObserved),
        ]
        + [
            (n, ctypes.c_uint64)
            for n in ("joint_status", "joint_fields", "scoring_pointer", "joint_preflight")
        ]
    )


class IPC(ctypes.Structure):
    _fields_ = (
        [
            ("magic", ctypes.c_uint64),
            ("version", ctypes.c_uint64),
            ("pid", ctypes.c_uint64),
            ("stop", ctypes.c_int32),
            ("status", ctypes.c_int32),
            ("heartbeat_ms", ctypes.c_int64),
            ("allow_observation", ctypes.c_int32),
            ("error", ctypes.c_int32),
        ]
        + [
            (n, ctypes.c_int64)
            for n in ("callbacks", "observations", "skipped", "sequence", "writer_thread")
        ]
        + [("qpc_frequency", ctypes.c_uint64), ("frame", Frame)]
    )


assert (
    ctypes.sizeof(Body) == 304 and ctypes.sizeof(Frame) == 306200 and ctypes.sizeof(IPC) == 306296
)


def payload(value):
    if isinstance(value, Recipe):
        n = "bar" if value.kind == 1 else "ball" if value.kind == 2 else "angular"
        return dict(
            identity=value.identity,
            body_indices=list(value.body_indices),
            kind=value.kind,
            geometry=payload(getattr(value.geometry, n)),
        )
    if isinstance(value, ctypes.Array):
        return [payload(v) for v in value]
    if isinstance(value, ctypes.Structure):
        return {n: payload(getattr(value, n)) for n, _ in value._fields_}
    return value


def run(pid, seconds, execute, offline):
    if execute and not offline:
        raise ValueError("Confirmed offline no-EAC launch required")
    binary, _, _ = verify_telemetry(pid)
    context()
    game = get_json("/rest/sessions/GetGameState")
    if execute and (
        game.get("inMonitor") is not False
        or game.get("inControlOfVehicle") is not True
        or game.get("MultiStintState") != "DRIVING"
    ):
        raise ValueError("Enter GUIDA before loading the snapshot observer")
    profile = json.loads(
        (PROJECT / "compatibility/steam-25661166-snapshot-observer-v29.json").read_text()
    )
    dll = (PROJECT / "bin/lmu-snapshot-observer-25661166-v29.dll").resolve()
    if hashlib.sha256(dll.read_bytes()).hexdigest() != profile["dll_sha256"]:
        raise ValueError("Pose DLL hash changed")
    listed = modules(pid)
    if any(
        "easyanticheat" in m["name"].lower() or m["name"].lower().startswith("eac") for m in listed
    ):
        raise ValueError("EAC module present")
    from audit_snapshot_25661166 import audit

    previous = audit(pid)
    if any(m["name"].lower() == "lmu-snapshot-observer-25661166-v29.dll" for m in listed):
        raise ValueError("Snapshot v3 already resident")
    main = next(m for m in listed if m["name"].lower() == "le mans ultimate.exe")
    handle = k.OpenProcess(0x43A if execute else 0x410, False, pid)
    if not handle:
        raise OSError("Ordinary process access denied")
    originals = []
    mapping = view = ipc = native = console = None
    failure = None
    decisions = {}
    reasons = {}
    persisted = 0
    prefix = (
        PROJECT
        / "logs"
        / (
            datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
            + "_snapshot_observer_v29"
        )
    )
    try:
        for target in profile["targets"]:
            address = main["base"] + target["rva"]
            raw = read_process(handle, address, 32)
            if not raw.startswith(bytes.fromhex(target["prolog"])):
                raise ValueError("Prolog mismatch: " + target["name"])
            originals.append((address, raw, target["name"]))
        context()
        if not execute:
            print(json.dumps({"preflight": "passed", "previous": previous, "physics_writes": 0}))
            return
        Path(str(prefix) + ".metadata.json").write_text(
            json.dumps(
                {
                    "pid": pid,
                    "build": 14200,
                    "seconds": seconds,
                    "physics_writes": 0,
                    "dll_sha256": profile["dll_sha256"],
                    "previous": previous,
                    "scope": "Partial snapshot at Dynamics entry: bodies, geometry recipes and cached impulses. No warm-start application, full subsystem snapshot or checkpoint restore.",
                },
                indent=2,
            )
            + "\n"
        )
        mapping = k.CreateFileMappingW(
            ctypes.c_void_p(-1),
            None,
            4,
            0,
            ctypes.sizeof(IPC),
            f"Local\\LMU_Snapshot25661166_v29_{pid}",
        )
        if not mapping or ctypes.get_last_error() == 183:
            raise ValueError("Pose mapping exists or unavailable")
        view = k.MapViewOfFile(mapping, 0xF001F, 0, 0, ctypes.sizeof(IPC))
        if not view:
            raise OSError("Pose mapping unavailable")
        ipc = IPC.from_address(view)
        ctypes.memset(view, 0, ctypes.sizeof(IPC))
        ipc.magic = 0x4C4D55534E503053
        ipc.version = 1
        ipc.pid = pid
        ipc.heartbeat_ms = KERNEL.GetTickCount64()
        load_observer(handle, pid, dll, ipc)
        for _ in range(100):
            if ipc.status >= 2:
                break
            ipc.heartbeat_ms = KERNEL.GetTickCount64()
            time.sleep(0.05)
        if ipc.status != 2:
            raise RuntimeError(f"Observer refused {ipc.status}/{ipc.error}")
        print(
            json.dumps(
                {"vehicle_observer": "active", "log_prefix": prefix.name, "physics_writes": 0}
            ),
            flush=True,
        )
        native_path = Path(str(prefix) + ".native.jsonl")
        native_path.touch(exist_ok=False)
        console = Path(str(prefix) + ".native-result.txt").open("w")
        native = subprocess.Popen(
            [str(binary), str(pid), str(seconds + 2), str(native_path)],
            stdout=console,
            stderr=console,
        )
        pending = ""
        last = None
        last_sequence = -1
        deadline = time.monotonic() + seconds
        with (
            native_path.open() as incoming,
            Path(str(prefix) + ".frames.jsonl").open("w") as output,
        ):
            while time.monotonic() < deadline and ipc.status == 2 and native.poll() is None:
                started = KERNEL.GetTickCount64()
                before = get_json("/navigation/state")["state"]
                game = get_json("/rest/sessions/GetGameState")
                pending += incoming.read()
                lines = pending.split("\n")
                pending = lines.pop()
                for line in lines:
                    if line:
                        last = json.loads(line)
                after = get_json("/navigation/state")["state"]
                now = KERNEL.GetTickCount64()
                decision = assess(before, after, game, last, now, started, observation_build=14200)
                ipc.allow_observation = int(decision["context_decision"] == "allowed")
                ipc.heartbeat_ms = now
                reasons[decision["reason"]] = reasons.get(decision["reason"], 0) + 1
                decisions[decision["context_decision"]] = (
                    decisions.get(decision["context_decision"], 0) + 1
                )
                if decision["reason"] in (
                    "multiplayer_or_replay",
                    "non_practice_session",
                    "network_scoring_conflicts_with_singleplayer",
                ):
                    raise ValueError("Offline Practice ended")
                seq = ipc.sequence
                if not seq & 1 and seq != last_sequence:
                    copy = IPC.from_buffer_copy(ctypes.string_at(view, ctypes.sizeof(IPC)))
                    if seq == ipc.sequence and seq == copy.sequence and copy.observations:
                        row = payload(copy.frame)
                        row["bodies"] = row["bodies"][: row["total_count"]]
                        for n in ("recipes", "memory"):
                            row[n] = row[n][: row["constraint_count"]]
                        row.update(
                            sequence=seq,
                            physics_thread_id=copy.writer_thread,
                            context=decision,
                            physics_writes=0,
                        )
                        output.write(json.dumps(row, allow_nan=False) + "\n")
                        output.flush()
                        last_sequence = seq
                        persisted += 1
                time.sleep(0.1)
    except Exception as exc:
        failure = f"{type(exc).__name__}: {exc}"
    finally:
        if ipc is not None:
            ipc.allow_observation = 0
            ipc.stop = 1
            for _ in range(150):
                if ipc.status in (3, 4, 5):
                    break
                time.sleep(0.02)
            restored = {}
            for address, raw, name in originals:
                try:
                    restored[name] = read_process(handle, address, 32) == raw
                except Exception:
                    restored[name] = False
            if ipc.status != 3 or not all(restored.values()):
                failure = failure or "Hook shutdown or restoration failed"
            report = dict(
                status=ipc.status,
                error=ipc.error,
                failure=failure,
                callbacks=ipc.callbacks,
                observations=ipc.observations,
                skipped=ipc.skipped,
                persisted_frames=persisted,
                writer_thread=ipc.writer_thread,
                restored=restored,
                context_decisions=decisions,
                context_reasons=reasons,
                physics_writes=0,
                module_resident_until_game_exit=True,
            )
            Path(str(prefix) + ".summary.json").write_text(json.dumps(report, indent=2) + "\n")
            print(json.dumps(report, indent=2), flush=True)
        if native is not None:
            native.wait(timeout=seconds + 5)
        if console is not None:
            console.close()
        if view:
            k.UnmapViewOfFile(view)
        if mapping:
            k.CloseHandle(mapping)
        k.CloseHandle(handle)
    if failure:
        raise RuntimeError(failure)
    if execute and not persisted:
        raise RuntimeError("No pose frames captured")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--pid", type=int, required=True)
    p.add_argument("--seconds", type=int, default=12)
    p.add_argument("--load", action="store_true")
    p.add_argument("--offline-launch-confirmed", action="store_true")
    a = p.parse_args()
    if not 2 <= a.seconds <= 60:
        p.error("Duration must be 2..60 seconds")
    run(a.pid, a.seconds, a.load, a.offline_launch_confirmed)
