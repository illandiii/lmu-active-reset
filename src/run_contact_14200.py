"""Bounded 14200 offline contact experiment, not a vehicle checkpoint reset.

Default is a read-only preflight. --load installs the tested natural-call hook.
--rebuild-once adds one stationary, four-wheel contact retirement request after
observations pass; the engine owns removal, re-query and surface-cache updates.
"""

import argparse
import ctypes
import datetime
import hashlib
import json
import math
from pathlib import Path
import subprocess
import time
from capture_preparation_14200 import verify_telemetry
from capture_context import PROJECT, KERNEL, get_json
from context_gate import assess
from probe_contacts_14200 import context
from run_physics_observer import k, modules, read_process, load_observer


class Result(ctypes.Structure):
    _fields_ = [
        (n, ctypes.c_uint64)
        for n in (
            "wheel",
            "tick_ms",
            "samples",
            "pre_count",
            "post_count",
            "pre_bad",
            "post_bad",
            "removed",
            "added",
            "post_positive",
            "surface_count",
            "retired",
            "cold_flags_clear",
        )
    ] + [
        (n, ctypes.c_double)
        for n in (
            "dt",
            "horizon",
            "angular_speed",
            "duration_us",
            "pre_min_depth",
            "post_min_depth",
            "post_max_depth",
            "normal_min",
            "normal_max",
            "position_delta",
        )
    ]


class IPC(ctypes.Structure):
    _fields_ = [
        ("magic", ctypes.c_uint64),
        ("version", ctypes.c_uint64),
        ("pid", ctypes.c_uint64),
        ("stop", ctypes.c_int32),
        ("status", ctypes.c_int32),
        ("heartbeat_ms", ctypes.c_int64),
        ("allow_observation", ctypes.c_int32),
        ("error", ctypes.c_int32),
        *[
            (n, ctypes.c_int64)
            for n in (
                "callbacks",
                "observations",
                "skipped",
                "sequence",
                "writer_thread",
                "request_id",
                "request_deadline_ms",
            )
        ],
        *[
            (n, ctypes.c_int32)
            for n in ("allow_rebuild", "completed_mask", "failed_mask", "reserved")
        ],
        *[(n, ctypes.c_uint64) for n in ("qpc_frequency", "physics_writes", "request_seen")],
        ("latest", Result * 4),
        ("rebuilt", Result * 4),
    ]


assert ctypes.sizeof(Result) == 184 and ctypes.sizeof(IPC) == 1616 and IPC.latest.offset == 144


def row(result):
    return {name: getattr(result, name) for name, _ in Result._fields_}


def stationary(sample):
    try:
        speed2 = sum(x * x for x in sample["velocity_local_mps"])
        return (
            sample["gear"] == 0
            and sample["in_pits"] is True
            and sample["throttle"] <= 0.01
            and math.isfinite(speed2)
            and speed2 <= 0.0001
        )
    except (KeyError, TypeError):
        return False


def ready(samples, now):
    return all(
        s.samples >= 64
        and s.pre_bad == 0
        and s.post_bad == 0
        and s.post_count > 0
        and s.post_positive > 0
        and 0 <= now - s.tick_ms <= 500
        and abs(s.position_delta) < 1e-12
        for s in samples
    )


def run(pid, seconds, execute, rebuild, offline_launch_confirmed):
    if execute and not offline_launch_confirmed:
        raise ValueError("Offline no-EAC launch confirmation required")
    if rebuild and not execute:
        raise ValueError("Rebuild requires --load")
    binary, _, binary_hash = verify_telemetry(pid)
    context()
    profile = json.loads((PROJECT / "compatibility/14200-contact-experiment.json").read_text())
    dll = (PROJECT / "bin/lmu-contact-native-14200.dll").resolve()
    if hashlib.sha256(dll.read_bytes()).hexdigest() != profile["dll_sha256"]:
        raise ValueError("Contact DLL hash changed")
    listed = modules(pid)
    if any(
        "easyanticheat" in m["name"].lower() or m["name"].lower().startswith("eac") for m in listed
    ):
        raise ValueError("EAC module found; no changes")
    if any(
        m["name"].lower().startswith(("lmu-contact-", "lmu-physics-", "lmu-binding-", "lmu-pose-"))
        for m in listed
    ):
        raise ValueError("Research DLL already resident; no duplicate load")
    main = next(m for m in listed if m["name"].lower() == "le mans ultimate.exe")
    target = main["base"] + 0xEF5B80
    handle = k.OpenProcess(0x43A if execute else 0x410, False, pid)
    if not handle:
        raise OSError(ctypes.get_last_error(), "Ordinary process access denied; no bypass")
    mapping = view = ipc = native = console = None
    original = None
    record = {}
    failure = None
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    prefix = PROJECT / "logs" / f"{stamp}_contact_experiment"
    try:
        original = read_process(handle, target, 32)
        if not original.startswith(bytes.fromhex(profile["prolog_hex"])):
            raise ValueError("Exact contact routine prolog mismatch")
        context()
        if not execute:
            print(
                json.dumps(
                    {
                        "preflight": "passed",
                        "hook_loaded": False,
                        "physics_writes": 0,
                        "target_rva": "0xEF5B80",
                        "dll_sha256": profile["dll_sha256"],
                    }
                )
            )
            return
        metadata = {
            "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "pid": pid,
            "build": 14200,
            "dll_sha256": profile["dll_sha256"],
            "diagnostic_sha256": binary_hash,
            "seconds": seconds,
            "rebuild_requested": rebuild,
            "vehicle_checkpoint_resets": 0,
            "target_rva": "0xEF5B80",
            "scope": "One stationary geometric-contact experiment in verified offline Practice; no teleport or car checkpoint.",
            "launch": "Operator confirmed offline no-EAC launch; exact build/UI and module inventory checked.",
            "installed_files_changed": [],
        }
        Path(str(prefix) + ".metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
        mapping = k.CreateFileMappingW(
            ctypes.c_void_p(-1),
            None,
            0x04,
            0,
            ctypes.sizeof(IPC),
            f"Local\\LMU_Contact14200_v1_{pid}",
        )
        if not mapping or ctypes.get_last_error() == 183:
            raise ValueError("Contact IPC already exists or cannot be created")
        view = k.MapViewOfFile(mapping, 0xF001F, 0, 0, ctypes.sizeof(IPC))
        if not view:
            raise OSError("Contact IPC map failed")
        ipc = IPC.from_address(view)
        ctypes.memset(view, 0, ctypes.sizeof(IPC))
        ipc.magic = 0x4C4D55434F4E5431
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
            raise RuntimeError(f"Contact module refused: status={ipc.status}, error={ipc.error}")
        console = Path(str(prefix) + ".native-result.txt").open("w")
        native_path = Path(str(prefix) + ".native.jsonl")
        native_path.touch(exist_ok=False)
        native = subprocess.Popen(
            [str(binary), str(pid), str(seconds + 2), str(native_path)],
            stdout=console,
            stderr=console,
        )
        deadline = time.monotonic() + seconds
        last_native = None
        pending = ""
        last_sequence = -1
        stable_since = None
        armed = False
        completed_at = None
        decisions = {}
        before_command = None
        print(
            json.dumps(
                {
                    "contact_module": "observing",
                    "pid": pid,
                    "rebuild_requested": rebuild,
                    "log_prefix": prefix.name,
                }
            ),
            flush=True,
        )
        with (
            native_path.open() as incoming,
            Path(str(prefix) + ".observations.jsonl").open("w") as output,
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
                        last_native = json.loads(line)
                after = get_json("/navigation/state")["state"]
                now = KERNEL.GetTickCount64()
                decision = assess(
                    before, after, game, last_native, now, started, observation_build=14200
                )
                allowed = decision["context_decision"] == "allowed"
                decisions[decision["context_decision"]] = (
                    decisions.get(decision["context_decision"], 0) + 1
                )
                ipc.allow_observation = int(allowed)
                ipc.heartbeat_ms = now
                if armed and (not allowed or not stationary(last_native)):
                    ipc.allow_rebuild = 0
                    ipc.request_deadline_ms = 0
                if decision["context_decision"] == "forbidden" and decision["reason"] in (
                    "multiplayer_or_replay",
                    "non_practice_session",
                    "network_scoring_conflicts_with_singleplayer",
                ):
                    raise ValueError("Offline Practice ended")
                sequence = ipc.sequence
                copied = None
                if not sequence & 1 and sequence != last_sequence:
                    copied = IPC.from_buffer_copy(ctypes.string_at(view, ctypes.sizeof(IPC)))
                    if sequence != ipc.sequence or sequence != copied.sequence:
                        copied = None
                if copied is not None and copied.observations:
                    last_sequence = sequence
                    output.write(
                        json.dumps(
                            {
                                "sequence": sequence,
                                "host_tick_ms": now,
                                "decision": decision,
                                "completed_mask": copied.completed_mask,
                                "failed_mask": copied.failed_mask,
                                "physics_writes": copied.physics_writes,
                                "samples": [row(r) for r in copied.latest],
                            },
                            allow_nan=False,
                        )
                        + "\n"
                    )
                    output.flush()
                    if not armed and rebuild:
                        if allowed and stationary(last_native) and ready(copied.latest, now):
                            if stable_since is None:
                                stable_since = now
                            if now - stable_since >= 2000:
                                before_command = last_native.copy()
                                ipc.request_deadline_ms = now + 3000
                                ipc.allow_rebuild = 1
                                ipc.request_id = 1
                                armed = True
                                print(
                                    "Single stationary contact request armed after live pre/post checks.",
                                    flush=True,
                                )
                        else:
                            stable_since = None
                if ipc.completed_mask == 15 and completed_at is None:
                    completed_at = time.monotonic()
                    ipc.allow_rebuild = 0
                    print(
                        "Four contact reconstructions reported; collecting post-operation stability.",
                        flush=True,
                    )
                if completed_at is not None and time.monotonic() - completed_at >= 5:
                    break
                if ipc.failed_mask:
                    raise RuntimeError(
                        "A wheel failed post-rebuild checks; further writes disabled"
                    )
                time.sleep(0.10)
        record = {
            "request_armed": armed,
            "context_decisions": decisions,
            "before_command": before_command,
            "last_native_sample": last_native,
            "completed_post_observation_seconds": 0
            if completed_at is None
            else time.monotonic() - completed_at,
        }
    except Exception as exc:
        failure = f"{type(exc).__name__}: {exc}"
    finally:
        if ipc is not None:
            ipc.allow_rebuild = 0
            ipc.allow_observation = 0
            ipc.stop = 1
            for _ in range(150):
                if ipc.status in (3, 4, 5):
                    break
                time.sleep(0.02)
            try:
                restored = read_process(handle, target, 32) == original
            except Exception:
                restored = False
            if ipc.status != 3 or not restored:
                failure = failure or "Contact hook shutdown or restoration failed"
            summary = {
                "status": ipc.status,
                "error": ipc.error,
                "failure": failure,
                "original_instructions_restored": restored,
                "module_resident_until_game_exit": True,
                "physics_writes": ipc.physics_writes,
                "completed_mask": ipc.completed_mask,
                "failed_mask": ipc.failed_mask,
                "callbacks": ipc.callbacks,
                "observations": ipc.observations,
                "skipped": ipc.skipped,
                "writer_thread_id": ipc.writer_thread,
                "vehicle_checkpoint_resets": 0,
                "rebuild_results": [row(r) for r in ipc.rebuilt],
                **record,
            }
            Path(str(prefix) + ".summary.json").write_text(
                json.dumps(summary, indent=2, allow_nan=False) + "\n"
            )
            print(
                json.dumps(
                    {
                        key: value
                        for key, value in summary.items()
                        if key not in ("rebuild_results", "before_command", "last_native_sample")
                    },
                    indent=2,
                ),
                flush=True,
            )
        if native is not None:
            # Never terminate a diagnostic which may own the SDK lock.
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
    if rebuild and (summary["completed_mask"] != 15 or summary["failed_mask"]):
        raise RuntimeError(
            "Requested contact experiment did not complete all four wheels; inspect summary"
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--seconds", type=int, default=120)
    parser.add_argument("--load", action="store_true")
    parser.add_argument("--rebuild-once", action="store_true")
    parser.add_argument("--offline-launch-confirmed", action="store_true")
    args = parser.parse_args()
    if not 10 <= args.seconds <= 180:
        parser.error("Duration must be 10..180 seconds")
    run(args.pid, args.seconds, args.load, args.rebuild_once, args.offline_launch_confirmed)
