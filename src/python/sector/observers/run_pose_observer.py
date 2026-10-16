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
from sector.telemetry.capture_preparation import verify_telemetry
from sector.telemetry.context_gate import assess
from sector.research.probe_contacts_readonly import context
from sector.observers.run_physics_observer import k, api, modules, read_process, load_observer
from sector.observers.run_contact_experiment import IPC as ContactIPC


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


class Frame(ctypes.Structure):
    _fields_ = (
        [
            (n, ctypes.c_uint64)
            for n in ("tick_ms", "root_count", "total_count", "aggregate_mask", "force_calls")
        ]
        + [
            ("contacts", ctypes.c_uint64 * 4),
            ("fresh_mask_at_force", ctypes.c_uint64 * 6),
            ("phase_errors", ctypes.c_uint64),
            ("wheel_body_index", ctypes.c_int64 * 4),
        ]
        + [
            (n, ctypes.c_uint64)
            for n in ("prepare_before", "prepare_after", "prepare_saved_with_current_mass")
        ]
        + [
            (n, ctypes.c_double)
            for n in (
                "simulation_time",
                "original_us",
                "observer_total_us",
                "inertia_error",
                "omega_error",
            )
        ]
        + [("before", Body * 32), ("after", Body * 32)]
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


assert ctypes.sizeof(Body) == 304 and ctypes.sizeof(Frame) == 19680 and ctypes.sizeof(IPC) == 19776


def payload(value):
    if isinstance(value, ctypes.Array):
        return [payload(v) for v in value]
    if isinstance(value, ctypes.Structure):
        return {n: payload(getattr(value, n)) for n, _ in value._fields_}
    return value


def verify_resident_contact(pid, listed):
    resident = [m for m in listed if m["name"].lower() == "lmu-contact-native-14150.dll"]
    if not resident:
        return False
    expected = (PROJECT / "bin/lmu-contact-native-14150.dll").resolve()
    profile = json.loads((PROJECT / "compatibility/14150-contact-experiment.json").read_text())
    if (
        len(resident) != 1
        or Path(resident[0]["path"]) != expected
        or hashlib.sha256(expected.read_bytes()).hexdigest() != profile["dll_sha256"]
    ):
        raise ValueError("Unrecognized resident contact DLL")
    api("OpenFileMappingW", [W.DWORD, W.BOOL, W.LPCWSTR], W.HANDLE)
    mapping = k.OpenFileMappingW(4, False, f"Local\\LMU_Contact_v1_{pid}")
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


def run(pid, seconds, execute, offline):
    if execute and not offline:
        raise ValueError("Confirmed offline no-EAC launch required")
    binary, _, _ = verify_telemetry(pid)
    context()
    profile = json.loads((PROJECT / "compatibility/14150-pose-observer.json").read_text())
    dll = (PROJECT / "bin/lmu-pose-observer-14150.dll").resolve()
    if hashlib.sha256(dll.read_bytes()).hexdigest() != profile["dll_sha256"]:
        raise ValueError("Pose DLL hash changed")
    listed = modules(pid)
    if any(
        "easyanticheat" in m["name"].lower() or m["name"].lower().startswith("eac") for m in listed
    ):
        raise ValueError("EAC module present")
    if any(m["name"].lower().startswith(("lmu-pose-", "lmu-physics-")) for m in listed):
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
            + "_pose_observer"
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
                        "physics_writes": 0,
                    }
                )
            )
            return
        Path(str(prefix) + ".metadata.json").write_text(
            json.dumps(
                {
                    "pid": pid,
                    "build": 14150,
                    "seconds": seconds,
                    "physics_writes": 0,
                    "dll_sha256": profile["dll_sha256"],
                    "previous_contact_module_stopped": old_stopped,
                    "scope": "Typed numerical before/after Dynamics capture plus contact/force phase observation; no checkpoint restore.",
                },
                indent=2,
            )
            + "\n"
        )
        mapping = k.CreateFileMappingW(
            ctypes.c_void_p(-1), None, 4, 0, ctypes.sizeof(IPC), f"Local\\LMU_PoseObserver_v1_{pid}"
        )
        if not mapping or ctypes.get_last_error() == 183:
            raise ValueError("Pose mapping exists or unavailable")
        view = k.MapViewOfFile(mapping, 0xF001F, 0, 0, ctypes.sizeof(IPC))
        if not view:
            raise OSError("Pose mapping unavailable")
        ipc = IPC.from_address(view)
        ctypes.memset(view, 0, ctypes.sizeof(IPC))
        ipc.magic = 0x4C4D55504F534531
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
            json.dumps({"pose_observer": "active", "log_prefix": prefix.name, "physics_writes": 0}),
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
                decision = assess(before, after, game, last, now, started, observation_build=14150)
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
                        row["before"] = row["before"][: copy.frame.total_count]
                        row["after"] = row["after"][: copy.frame.total_count]
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
