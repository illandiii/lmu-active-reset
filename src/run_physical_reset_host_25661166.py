"""Bounded, explicitly commanded offline physical-reset experiment.

The observation gate supplies context evidence only. Native admission, lifecycle
epoch, stationary limits, typed plan preflight and contact reconstruction are
separate requirements inside the host. A restore additionally requires --allow-writes
and a successful preparation reported by this same running module.
"""

import argparse
import ctypes as C
import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import struct
import time
from urllib.error import URLError

from capture_context import PROJECT, KERNEL, get_json
from capture_preparation_steam25661166 import verify_telemetry
from context_gate import assess_player_observation
from probe_contacts_14200 import context
from run_physics_observer import k, modules, read_process, load_observer

MAGIC = 0x4C4D555253543031


class Report(C.Structure):
    _fields_ = (
        [
            (name, C.c_uint64)
            for name in (
                "action",
                "result",
                "fields",
                "forward_writes",
                "recovery_writes",
                "flow_result",
                "cause",
                "contact_mask",
                "contact_calls",
                "saves",
                "loads",
                "callbacks",
                "pins",
                "epoch",
                "readable",
                "verified_fields",
                "mismatched_fields",
            )
        ]
        + [
            (name, C.c_double * 3)
            for name in (
                "saved_position",
                "before_position",
                "after_position",
            )
        ]
        + [("speed", C.c_double)]
    )


class IPC(C.Structure):
    _fields_ = [
        ("magic", C.c_uint64),
        ("version", C.c_uint64),
        ("pid", C.c_uint64),
        ("stop", C.c_int32),
        ("status", C.c_int32),
        ("heartbeat_ms", C.c_int64),
        ("allow", C.c_int32),
        ("permit_writes", C.c_int32),
        ("owner_thread", C.c_uint64),
        ("request_id", C.c_int64),
        ("request_action", C.c_int64),
        ("request_tick", C.c_int64),
        ("report_sequence", C.c_int64),
        ("completed_id", C.c_int64),
        ("error", C.c_int32),
        ("reserved", C.c_int32),
        ("report", Report),
        ("physical_field_write_attempts", C.c_int64),
    ]

    @property
    def allow_observation(self):
        return self.allow

    @property
    def physics_writes(self):
        return self.physical_field_write_attempts


assert C.sizeof(Report) == 216 and C.sizeof(IPC) == 328


def as_dict(report):
    return {
        name: list(getattr(report, name))
        if isinstance(getattr(report, name), C.Array)
        else getattr(report, name)
        for name, _ in report._fields_
    }


def context_attempt(ipc, hotkeys, reasons, fetch=get_json, clock=KERNEL.GetTickCount64):
    """A failed transport read revokes admission without killing the runner."""
    try:
        before = fetch("/navigation/state")["state"]
        game = fetch("/rest/sessions/GetGameState")
        after = fetch("/navigation/state")["state"]
        return before, game, after
    except (TimeoutError, URLError, ConnectionError):
        ipc.allow = 0
        ipc.permit_writes = 0
        ipc.heartbeat_ms = clock()
        if hotkeys:
            hotkeys.pending = None
            hotkeys.armed = False
        reasons["context_transport_retry"] = reasons.get("context_transport_retry", 0) + 1
        return None


def final_acknowledgement(view, ipc, last_id):
    sequence = ipc.report_sequence
    copy = IPC.from_buffer_copy(C.string_at(view, C.sizeof(IPC)))
    if (
        sequence & 1
        or copy.report_sequence != ipc.report_sequence
        or copy.report_sequence != sequence
        or copy.completed_id <= last_id
    ):
        return None
    row = as_dict(copy.report)
    row.update(
        id=copy.completed_id, physical_field_write_attempts=copy.physical_field_write_attempts
    )
    return row


def run(args):
    from physical_reset_hotkeys import HotkeyCommands, WindowsKeys, function_key

    hotkeys = HotkeyCommands() if getattr(args, "hotkeys", False) else None
    keyboard = None
    if hotkeys:
        save_vk, restore_vk = function_key(args.save_key), function_key(args.restore_key)
        if save_vk == restore_vk or not args.allow_writes:
            raise ValueError("Distinct hotkeys and explicit write opt-in required")
        keyboard = WindowsKeys(args.pid, save_vk, restore_vk)
    if not args.offline_launch_confirmed:
        raise ValueError("Offline launch without EAC must be explicitly confirmed")
    binary, _, _ = verify_telemetry(args.pid)
    context()
    game = get_json("/rest/sessions/GetGameState")
    if game.get("inControlOfVehicle") is not True or game.get("inMonitor") is not False:
        raise ValueError("GUIDA is required")
    listed = modules(args.pid)
    if any(
        "easyanticheat" in m["name"].lower() or m["name"].lower().startswith("eac") for m in listed
    ):
        raise ValueError("EAC module present")
    from audit_snapshot_25661166 import audit

    previous = audit(args.pid)
    dll = (PROJECT / f"bin/lmu-physical-reset-v{args.revision}-25661166.dll").resolve()
    if any(m["name"].lower() == dll.name.lower() for m in listed):
        raise ValueError("Physical host already resident; do not replace a resident module")
    profile = json.loads(
        (PROJECT / f"compatibility/steam-25661166-physical-reset-v{args.revision}.json").read_text()
    )
    if hashlib.sha256(dll.read_bytes()).hexdigest() != profile["dll_sha256"]:
        raise ValueError("Host DLL hash mismatch")
    image = next(m for m in listed if m["name"].lower() == "le mans ultimate.exe")
    handle = k.OpenProcess(0x43A, False, args.pid)
    if not handle:
        raise OSError("Process access unavailable")
    prefix = (
        PROJECT
        / "logs"
        / (
            datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
            + f"_physical_reset_host_v{args.revision}"
        )
    )
    mapping = view = ipc = native = console = None
    originals = []
    failure = None
    reports = []
    reasons = {}
    try:
        # Asynchronous rejection only; the authoritative check runs again
        # inside the pinned native reader before any checkpoint operation.
        def pointer(address):
            return struct.unpack("<Q", read_process(handle, address, 8))[0]

        owner = pointer(image["base"] + 0x1E194C8)
        physics = pointer(owner + 0x159A8)
        vehicle = pointer(physics + 0x4BC0)
        terminal_failure = struct.unpack("<I", read_process(handle, vehicle + 0x1CF9C, 4))[0]
        if terminal_failure:
            raise ValueError(
                f"Vehicle terminal failure {terminal_failure}; a healthy vehicle is required"
            )
        for target in profile["targets"]:
            address = image["base"] + target["rva"]
            raw = read_process(handle, address, 32)
            if not raw.startswith(bytes.fromhex(target["prolog"])):
                raise ValueError("Changed target: " + target["name"])
            originals.append((address, raw, target["name"]))
        metadata = dict(
            pid=args.pid,
            owner_thread=args.owner_thread,
            previous=previous,
            dll_sha256=profile["dll_sha256"],
            allow_writes=args.allow_writes,
            request_file=str(args.request_file.resolve()),
            profile=profile,
        )
        Path(str(prefix) + ".metadata.json").write_text(json.dumps(metadata, indent=2))
        mapping = k.CreateFileMappingW(
            C.c_void_p(-1),
            None,
            4,
            0,
            C.sizeof(IPC),
            f"Local\\LMU_PhysicalReset25661166_v{args.revision}_{args.pid}",
        )
        if not mapping or C.get_last_error() == 183:
            raise ValueError("Host mapping already exists or is unavailable")
        view = k.MapViewOfFile(mapping, 0xF001F, 0, 0, C.sizeof(IPC))
        if not view:
            raise OSError("Host mapping unavailable")
        ipc = IPC.from_address(view)
        C.memset(view, 0, C.sizeof(IPC))
        ipc.magic, ipc.version, ipc.pid = MAGIC + args.revision - 1, 1, args.pid
        ipc.owner_thread = args.owner_thread
        ipc.heartbeat_ms = KERNEL.GetTickCount64()
        load_observer(handle, args.pid, dll, ipc)
        for _ in range(100):
            if ipc.status >= 2:
                break
            ipc.heartbeat_ms = KERNEL.GetTickCount64()
            time.sleep(0.05)
        if ipc.status != 2:
            raise RuntimeError(f"Host refused: {ipc.status}/{ipc.error}")
        native_path = Path(str(prefix) + ".native.jsonl")
        native_path.touch(exist_ok=False)
        console = Path(str(prefix) + ".native-result.txt").open("w")
        native = subprocess.Popen(
            [str(binary), str(args.pid), str(args.seconds + 2), str(native_path)],
            stdout=console,
            stderr=console,
        )
        print(
            json.dumps(dict(host="active", log_prefix=prefix.name, writes_require_command=True)),
            flush=True,
        )
        if hotkeys:
            print(
                f"{args.save_key}: SALVA | {args.restore_key}: RIPRISTINA | Ctrl+C: ARRESTA. "
                "Solo con LMU in primo piano. Durata massima: " + str(args.seconds) + " s.",
                flush=True,
            )
        deadline = time.monotonic() + args.seconds
        tail, telemetry = "", None
        last_completed = sent_id = 0
        prepared_at = 0.0
        with (
            native_path.open() as incoming,
            Path(str(prefix) + ".reports.jsonl").open("w") as output,
        ):
            while time.monotonic() < deadline and ipc.status == 2 and native.poll() is None:
                started = KERNEL.GetTickCount64()
                observation = context_attempt(ipc, hotkeys, reasons)
                if observation is None:
                    prepared_at = 0.0
                    time.sleep(0.05)
                    continue
                before, game, after = observation
                tail += incoming.read()
                lines = tail.split("\n")
                tail = lines.pop()
                for line in lines:
                    if line:
                        telemetry = json.loads(line)
                now = KERNEL.GetTickCount64()
                decision = assess_player_observation(
                    before, after, game, telemetry, now, started, observation_build=14200
                )
                ipc.allow = int(decision["context_decision"] == "allowed")
                ipc.heartbeat_ms = now
                reasons[decision["reason"]] = reasons.get(decision["reason"], 0) + 1
                if decision["context_decision"] == "forbidden":
                    raise ValueError("Offline Practice context ended")
                acknowledged = None
                completed = ipc.completed_id
                sequence = ipc.report_sequence
                if completed > last_completed and not sequence & 1:
                    copy = IPC.from_buffer_copy(C.string_at(view, C.sizeof(IPC)))
                    if (
                        copy.report_sequence == ipc.report_sequence == sequence
                        and copy.completed_id == completed
                    ):
                        row = as_dict(copy.report)
                        row.update(
                            id=completed,
                            physical_field_write_attempts=copy.physical_field_write_attempts,
                        )
                        reports.append(row)
                        acknowledged = row
                        output.write(json.dumps(row, allow_nan=False) + "\n")
                        output.flush()
                        print(json.dumps(row, allow_nan=False), flush=True)
                        if hotkeys and (row["action"] != 2 or row["result"] != 0):
                            import winsound

                            if row["result"] == 0:
                                message = (
                                    "Checkpoint salvato."
                                    if row["action"] == 1
                                    else "Ripristino completato."
                                )
                            else:
                                message = f"Comando rifiutato: risultato {row['result']}, causa {row['cause']}."
                            print(message, flush=True)
                            winsound.MessageBeep(
                                winsound.MB_OK if row["result"] == 0 else winsound.MB_ICONHAND
                            )
                        last_completed = completed
                        prepared_at = (
                            time.monotonic()
                            if row["action"] == 2 and row["result"] == 0 and row["fields"] == 10077
                            else 0.0
                        )
                if hotkeys:
                    focused, save, restore, modifiers = keyboard.read()
                    command = hotkeys.poll(
                        focused,
                        bool(ipc.allow),
                        save,
                        restore,
                        modifiers,
                        time.monotonic(),
                        acknowledged,
                    )
                    # Do not queue input while a previous native request is in flight.
                    if command and ipc.completed_id != sent_id:
                        hotkeys.pending = None
                        command = {}
                else:
                    command = (
                        json.loads(args.request_file.read_text())
                        if args.request_file.exists()
                        else {}
                    )
                if command.get("stop") is True:
                    break
                request_id, action = command.get("id", 0), command.get("action", 0)
                if type(request_id) is not int or type(action) is not int:
                    raise ValueError("Malformed command")
                if (
                    request_id > sent_id
                    and action in (1, 2, 3)
                    and ipc.allow
                    and ipc.completed_id == sent_id
                ):
                    if action == 3 and (
                        not args.allow_writes
                        or not prepared_at
                        or time.monotonic() - prepared_at > 60
                    ):
                        raise ValueError(
                            "Restore requires explicit write opt-in and recent native preparation"
                        )
                    ipc.permit_writes = int(action == 3 and args.allow_writes)
                    ipc.request_action = action
                    ipc.request_tick = now
                    ipc.request_id = request_id  # Publish last.
                    sent_id = request_id
                time.sleep(0.05)
    except KeyboardInterrupt:
        print("Arresto richiesto; ripristino degli agganci in corso.", flush=True)
    except Exception as error:
        failure = f"{type(error).__name__}: {error}"
    finally:
        if ipc is not None:
            ipc.allow = 0
            ipc.stop = 1
            for _ in range(350):
                if ipc.status in (3, 4, 5):
                    break
                time.sleep(0.02)
            # A native command can complete while HTTP is timing out or while
            # shutdown drains the callback. Preserve its final acknowledgement.
            row = final_acknowledgement(view, ipc, reports[-1]["id"] if reports else 0)
            if row is not None:
                reports.append(row)
                with Path(str(prefix) + ".reports.jsonl").open("a") as output:
                    output.write(json.dumps(row, allow_nan=False) + "\n")
                print(json.dumps(row, allow_nan=False), flush=True)
            restored = {}
            for address, raw, name in originals:
                try:
                    restored[name] = read_process(handle, address, 32) == raw
                except Exception:
                    restored[name] = False
            if ipc.status != 3 or not all(restored.values()):
                failure = failure or "Host quarantine or hook shutdown failure"
            summary = dict(
                status=ipc.status,
                error=ipc.error,
                failure=failure,
                restored=restored,
                reports=reports,
                context_reasons=reasons,
                physical_field_write_attempts=ipc.physical_field_write_attempts,
            )
            Path(str(prefix) + ".summary.json").write_text(json.dumps(summary, indent=2))
            print(json.dumps(summary, indent=2), flush=True)
        if native is not None:
            # This is our read-only telemetry child, not the game process.
            if native.poll() is None:
                native.terminate()
            native.wait(timeout=5)
        if console:
            console.close()
        if view:
            k.UnmapViewOfFile(view)
        if mapping:
            k.CloseHandle(mapping)
        k.CloseHandle(handle)
    if failure:
        raise RuntimeError(failure)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--revision", type=int, choices=range(1, 10), default=8)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--owner-thread", type=int, default=0)
    parser.add_argument("--seconds", type=int, default=180)
    parser.add_argument("--request-file", type=Path, required=True)
    parser.add_argument("--offline-launch-confirmed", action="store_true")
    parser.add_argument("--allow-writes", action="store_true")
    parser.add_argument("--hotkeys", action="store_true")
    parser.add_argument("--save-key", default="F6")
    parser.add_argument("--restore-key", default="F7")
    args = parser.parse_args()
    if not 5 <= args.seconds <= 240 or args.owner_thread < 0:
        parser.error("Duration must be 5..240 seconds; owner thread 0 selects native discovery")
    run(args)
