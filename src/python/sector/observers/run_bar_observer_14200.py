"""Coherent numerical capture and phase observation only; no pose writes."""

import argparse
import ctypes
import datetime
import hashlib
import json
import subprocess
import time
from ctypes import wintypes as W
from pathlib import Path
from sector.telemetry.capture_context import PROJECT, KERNEL, get_json
from sector.telemetry.capture_preparation_14200 import verify_telemetry
from sector.telemetry.context_gate import assess
from sector.research.probe_contacts_14200 import context
from sector.observers.run_physics_observer import k, api, modules, read_process, load_observer
from sector.observers.run_contact_14200 import IPC as ContactIPC


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


class Geometry(ctypes.Structure):
    _fields_ = [("anchors_local", (ctypes.c_double * 3) * 2), ("softness", ctypes.c_double)]


class Frame(ctypes.Structure):
    _fields_ = [
        ("bodies", Saved * 2),
        ("geometry", Geometry),
        ("native_jacobian", ctypes.c_double * 12),
        ("native_weighted", ctypes.c_double * 12),
        ("native_coefficient", ctypes.c_double),
        ("prepare_status", ctypes.c_uint64),
        ("jacobian_error", ctypes.c_double),
        ("weighted_error", ctypes.c_double),
        ("coefficient_error", ctypes.c_double),
        ("constraint_index", ctypes.c_uint64),
        ("tick_ms", ctypes.c_uint64),
    ]


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


assert ctypes.sizeof(Body) == 304 and ctypes.sizeof(Frame) == 656 and ctypes.sizeof(IPC) == 752


def payload(value):
    if isinstance(value, ctypes.Array):
        return [payload(v) for v in value]
    if isinstance(value, ctypes.Structure):
        return {n: payload(getattr(value, n)) for n, _ in value._fields_}
    return value


def verify_resident_contact(pid, listed):
    resident = [m for m in listed if m["name"].lower() == "lmu-contact-native-14200.dll"]
    if not resident:
        return False
    expected = (PROJECT / "bin/lmu-contact-native-14200.dll").resolve()
    profile = json.loads((PROJECT / "compatibility/14200-contact-experiment.json").read_text())
    if (
        len(resident) != 1
        or Path(resident[0]["path"]) != expected
        or hashlib.sha256(expected.read_bytes()).hexdigest() != profile["dll_sha256"]
    ):
        raise ValueError("Unrecognized resident contact DLL")
    api("OpenFileMappingW", [W.DWORD, W.BOOL, W.LPCWSTR], W.HANDLE)
    mapping = k.OpenFileMappingW(4, False, f"Local\\LMU_Contact14200_v1_{pid}")
    view = None
    try:
        if not mapping:
            raise ValueError("Resident contact IPC unavailable")
        view = k.MapViewOfFile(mapping, 4, 0, 0, ctypes.sizeof(ContactIPC))
        if not view:
            raise ValueError("Cannot inspect resident contact status")
        s = ContactIPC.from_buffer_copy(ctypes.string_at(view, ctypes.sizeof(ContactIPC)))
        if (
            s.magic != 0x4C4D55434F4E5431
            or s.pid != pid
            or s.version != 1
            or s.status != 3
            or s.stop != 1
            or s.allow_rebuild != 0
        ):
            raise ValueError("Resident contact module is not stopped")
        return True
    finally:
        if view:
            k.UnmapViewOfFile(view)
        if mapping:
            k.CloseHandle(mapping)


def verify_resident_v1(pid, listed):
    from sector.observers.run_vehicle_observer_14200 import IPC as OldIPC

    resident = [m for m in listed if m["name"].lower() == "lmu-vehicle-observer-14200.dll"]
    if not resident:
        return False
    expected = (PROJECT / "bin/lmu-vehicle-observer-14200.dll").resolve()
    profile = json.loads((PROJECT / "compatibility/14200-vehicle-observer.json").read_text())
    if (
        len(resident) != 1
        or Path(resident[0]["path"]) != expected
        or hashlib.sha256(expected.read_bytes()).hexdigest() != profile["dll_sha256"]
    ):
        raise ValueError("Unrecognized resident v1 observer")
    api("OpenFileMappingW", [W.DWORD, W.BOOL, W.LPCWSTR], W.HANDLE)
    mapping = k.OpenFileMappingW(4, False, f"Local\\LMU_VehicleObserver14200_v1_{pid}")
    view = None
    try:
        if not mapping:
            raise ValueError("Old observer IPC missing")
        view = k.MapViewOfFile(mapping, 4, 0, 0, ctypes.sizeof(OldIPC))
        if not view:
            raise ValueError("Old observer IPC unreadable")
        old = OldIPC.from_buffer_copy(ctypes.string_at(view, ctypes.sizeof(OldIPC)))
        if (
            old.magic != 0x4C4D555645483231
            or old.pid != pid
            or old.version != 1
            or old.status != 3
            or old.error
            or old.stop != 1
            or old.allow_observation
        ):
            raise ValueError("Old observer not stopped cleanly")
        return True
    finally:
        if view:
            k.UnmapViewOfFile(view)
        if mapping:
            k.CloseHandle(mapping)


def verify_resident_v2(pid, listed):
    from sector.observers.run_vehicle_observer_v2_14200 import IPC as OldIPC

    resident = [m for m in listed if m["name"].lower() == "lmu-vehicle-observer-14200-v2.dll"]
    if not resident:
        return False
    expected = (PROJECT / "bin/lmu-vehicle-observer-14200-v2.dll").resolve()
    profile = json.loads((PROJECT / "compatibility/14200-vehicle-observer-v2.json").read_text())
    if (
        len(resident) != 1
        or Path(resident[0]["path"]) != expected
        or hashlib.sha256(expected.read_bytes()).hexdigest() != profile["dll_sha256"]
    ):
        raise ValueError("Unrecognized resident v1 observer")
    api("OpenFileMappingW", [W.DWORD, W.BOOL, W.LPCWSTR], W.HANDLE)
    mapping = k.OpenFileMappingW(4, False, f"Local\\LMU_VehicleObserver14200_v2_{pid}")
    view = None
    try:
        if not mapping:
            raise ValueError("Old observer IPC missing")
        view = k.MapViewOfFile(mapping, 4, 0, 0, ctypes.sizeof(OldIPC))
        if not view:
            raise ValueError("Old observer IPC unreadable")
        old = OldIPC.from_buffer_copy(ctypes.string_at(view, ctypes.sizeof(OldIPC)))
        if (
            old.magic != 0x4C4D555645483232
            or old.pid != pid
            or old.version != 1
            or old.status != 3
            or old.error
            or old.stop != 1
            or old.allow_observation
        ):
            raise ValueError("Old observer not stopped cleanly")
        return True
    finally:
        if view:
            k.UnmapViewOfFile(view)
        if mapping:
            k.CloseHandle(mapping)


def verify_resident_v3(pid, listed):
    from sector.observers.run_vehicle_observer_v3_14200 import IPC as OldIPC

    resident = [m for m in listed if m["name"].lower() == "lmu-vehicle-observer-14200-v3.dll"]
    if not resident:
        return False
    expected = (PROJECT / "bin/lmu-vehicle-observer-14200-v3.dll").resolve()
    profile = json.loads((PROJECT / "compatibility/14200-vehicle-observer-v3.json").read_text())
    if (
        len(resident) != 1
        or Path(resident[0]["path"]) != expected
        or hashlib.sha256(expected.read_bytes()).hexdigest() != profile["dll_sha256"]
    ):
        raise ValueError("Unrecognized resident v1 observer")
    api("OpenFileMappingW", [W.DWORD, W.BOOL, W.LPCWSTR], W.HANDLE)
    mapping = k.OpenFileMappingW(4, False, f"Local\\LMU_VehicleObserver14200_v3_{pid}")
    view = None
    try:
        if not mapping:
            raise ValueError("Old observer IPC missing")
        view = k.MapViewOfFile(mapping, 4, 0, 0, ctypes.sizeof(OldIPC))
        if not view:
            raise ValueError("Old observer IPC unreadable")
        old = OldIPC.from_buffer_copy(ctypes.string_at(view, ctypes.sizeof(OldIPC)))
        if (
            old.magic != 0x4C4D555645483233
            or old.pid != pid
            or old.version != 1
            or old.status != 3
            or old.error
            or old.stop != 1
            or old.allow_observation
        ):
            raise ValueError("Old observer not stopped cleanly")
        return True
    finally:
        if view:
            k.UnmapViewOfFile(view)
        if mapping:
            k.CloseHandle(mapping)


def run(pid, seconds, execute, offline):
    if execute and not offline:
        raise ValueError("Confirmed offline no-EAC launch required")
    binary, _, _ = verify_telemetry(pid)
    context()
    profile = json.loads((PROJECT / "compatibility/14200-bar-observer.json").read_text())
    dll = (PROJECT / "bin/lmu-bar-observer-14200.dll").resolve()
    if hashlib.sha256(dll.read_bytes()).hexdigest() != profile["dll_sha256"]:
        raise ValueError("Pose DLL hash changed")
    listed = modules(pid)
    if any(
        "easyanticheat" in m["name"].lower() or m["name"].lower().startswith("eac") for m in listed
    ):
        raise ValueError("EAC module present")
    v1_stopped = verify_resident_v1(pid, listed)
    v2_stopped = verify_resident_v2(pid, listed)
    v3_stopped = verify_resident_v3(pid, listed)
    if any(
        m["name"]
        .lower()
        .startswith(("lmu-pose-", "lmu-physics-", "lmu-vehicle-", "lmu-binding-", "lmu-bar-"))
        and not (
            (v1_stopped and m["name"].lower() == "lmu-vehicle-observer-14200.dll")
            or (v2_stopped and m["name"].lower() == "lmu-vehicle-observer-14200-v2.dll")
            or (v3_stopped and m["name"].lower() == "lmu-vehicle-observer-14200-v3.dll")
        )
        for m in listed
    ):
        raise ValueError("Conflicting or duplicate observer")
    old_stopped = verify_resident_contact(pid, listed)
    main = next(m for m in listed if m["name"].lower() == "le mans ultimate.exe")
    handle = k.OpenProcess(0x43A if execute else 0x410, False, pid)
    if not handle:
        raise OSError("Ordinary process access denied")
    originals = []
    mapping = view = ipc = native = console = None
    failure = None
    decisions = {}
    persisted = 0
    prefix = (
        PROJECT
        / "logs"
        / (
            datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
            + "_bar_observer"
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
            print(
                json.dumps(
                    {
                        "preflight": "passed",
                        "previous_contact_module_stopped": old_stopped,
                        "previous_vehicle_v1_stopped": v1_stopped,
                        "previous_vehicle_v2_stopped": v2_stopped,
                        "previous_vehicle_v3_stopped": v3_stopped,
                        "physics_writes": 0,
                    }
                )
            )
            return
        Path(str(prefix) + ".metadata.json").write_text(
            json.dumps(
                {
                    "pid": pid,
                    "build": 14200,
                    "seconds": seconds,
                    "physics_writes": 0,
                    "dll_sha256": profile["dll_sha256"],
                    "previous_contact_module_stopped": old_stopped,
                    "previous_vehicle_v1_stopped": v1_stopped,
                    "previous_vehicle_v2_stopped": v2_stopped,
                    "previous_vehicle_v3_stopped": v3_stopped,
                    "scope": "Typed Bar geometry immediately after its native A0 call; no impulse application or checkpoint restore.",
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
            f"Local\\LMU_BarObserver14200_v1_{pid}",
        )
        if not mapping or ctypes.get_last_error() == 183:
            raise ValueError("Pose mapping exists or unavailable")
        view = k.MapViewOfFile(mapping, 0xF001F, 0, 0, ctypes.sizeof(IPC))
        if not view:
            raise OSError("Pose mapping unavailable")
        ipc = IPC.from_address(view)
        ctypes.memset(view, 0, ctypes.sizeof(IPC))
        ipc.magic = 0x4C4D554241523231
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
