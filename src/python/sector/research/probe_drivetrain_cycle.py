"""Short offline Practice UI cycle and bounded engine/driveline reads; no reset."""

import argparse
import ctypes
import datetime
import json
import math
import pathlib
import struct
import subprocess
import time
import urllib.request
from ctypes import wintypes as W
from sector.telemetry.capture_context import verify, get_json, KERNEL, PROJECT, HTTP
from sector.telemetry.context_gate import assess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--base", type=lambda x: int(x, 0), required=True)
    parser.add_argument(
        "--manual-drive",
        action="store_true",
        help="Wait for user to enter driving and start the engine",
    )
    parser.add_argument("--seconds", type=int, default=10)
    parser.add_argument("--wait-for-drive-seconds", type=int, default=0)
    args = parser.parse_args()
    binary, contract, digest = verify(args.pid)
    if not 10 <= args.seconds <= 120:
        raise ValueError("Duration must be 10..120 seconds")
    if not 0 <= args.wait_for_drive_seconds <= 600 or (
        args.wait_for_drive_seconds and not args.manual_drive
    ):
        raise ValueError("Wait requires manual mode and must be 0..600 seconds")

    def context():
        s = get_json("/navigation/state")["state"]
        if (
            s.get("appBuild") != 14130
            or s.get("settingMode") != "SETTING_GRANDPRIX"
            or s.get("gameSession") not in ("PRACTICE1", "PRACTICE2", "PRACTICE3", "PRACTICE4")
            or s.get("gameState") != "GSTATE_DYN"
        ):
            raise ValueError("Practice context refused")
        return s

    context()
    if get_json("/rest/sessions/GetGameState").get("inMonitor") is not True:
        raise ValueError("Must start in monitor")

    def post(key):
        context()
        command = contract[key]
        if (
            command["path"] not in ("/rest/garage/drive", "/rest/sessions/returnToMonitor")
            or command["method"] != "POST"
            or command["body"] is not None
        ):
            raise ValueError("UI contract mismatch")
        request = urllib.request.Request(
            "http://127.0.0.1:6397" + command["path"],
            data=b"null",
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with HTTP.open(request, timeout=3) as r:
            if r.status != 200:
                raise ValueError("UI command failed")

    KERNEL.ReadProcessMemory.argtypes = (
        W.HANDLE,
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t),
    )
    KERNEL.ReadProcessMemory.restype = W.BOOL
    handle = KERNEL.OpenProcess(0x410, False, args.pid)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    total = 0
    child = None
    console = None
    entered = False
    samples = []
    error = None
    returned = None
    cleanup_errors = []
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    stem = PROJECT / "logs" / f"{stamp}_drivetrain_cycle"
    native_path = pathlib.Path(str(stem) + ".jsonl")
    try:

        def read(a, n):
            nonlocal total
            if not 0x10000 <= a < 0x800000000000 or not 0 < n <= 128 or total + n > 65536:
                raise ValueError("Read budget refused")
            buf = ctypes.create_string_buffer(n)
            got = ctypes.c_size_t()
            if not KERNEL.ReadProcessMemory(handle, a, buf, n, ctypes.byref(got)) or got.value != n:
                raise ctypes.WinError(ctypes.get_last_error())
            total += n
            return buf.raw

        def pointer(a):
            return struct.unpack("<Q", read(a, 8))[0]

        inv = json.loads((PROJECT / "inventory.json").read_text(encoding="utf-8-sig"))
        with (pathlib.Path(inv["install_path"]) / "Le Mans Ultimate.exe").open("rb") as f:
            header = f.read(64)
        if read(args.base, 64) != header:
            raise ValueError("Base mismatch")
        if args.wait_for_drive_seconds:
            print(
                "Waiting for user to press GUIDA; recording starts on the transition.", flush=True
            )
            wait_deadline = time.monotonic() + args.wait_for_drive_seconds
            while context().get("navigationState") != "NAV_REALTIME":
                if time.monotonic() >= wait_deadline:
                    raise TimeoutError("No manual driving transition observed")
                time.sleep(0.25)
        console = pathlib.Path(str(stem) + ".console.txt").open("w")
        child = subprocess.Popen(
            [str(binary), str(args.pid), str(args.seconds + 5), str(native_path)],
            stdout=console,
            stderr=console,
        )
        time.sleep(0.6)
        if child.poll() is not None:
            raise ValueError("Native reader did not start")
        if not args.manual_drive:
            entered = True
            post("drive_command")
        print("Recording ready; waiting for verified driving context.", flush=True)
        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            time.sleep(0.2)
            started = KERNEL.GetTickCount64()
            before = context()
            game = get_json("/rest/sessions/GetGameState")
            lines = native_path.read_bytes().splitlines()
            native = None
            for line in reversed(lines):
                try:
                    native = json.loads(line)
                    break
                except json.JSONDecodeError:
                    pass
            after = context()
            decision = assess(before, after, game, native, KERNEL.GetTickCount64(), started)
            if decision["context_decision"] != "allowed":
                continue
            entered = True
            owner = pointer(args.base + 0x1DD94D8)
            vehicle = pointer(owner + 0x15978)
            engine = pointer(vehicle + 0x46F0)
            if pointer(engine) != args.base + 0x176E588:
                raise ValueError("ICE type differs from observed Mercedes")
            shaft = pointer(vehicle + 0x4918)
            gearbox = pointer(vehicle + 0x4920)
            fields = {
                "engine_1f8": struct.unpack("<d", read(engine + 0x1F8, 8))[0],
                "engine_280": struct.unpack("<d", read(engine + 0x280, 8))[0],
                "engine_250": struct.unpack("<d", read(engine + 0x250, 8))[0],
                "driveline_shaft_0_8_10": struct.unpack("<3d", read(shaft, 24)),
                "gearbox_64": struct.unpack("<i", read(gearbox + 0x64, 4))[0],
                "vehicle_448": struct.unpack("<d", read(vehicle + 0x448, 8))[0],
                "vehicle_4f8": struct.unpack("<d", read(vehicle + 0x4F8, 8))[0],
            }
            if (
                pointer(args.base + 0x1DD94D8) != owner
                or pointer(owner + 0x15978) != vehicle
                or pointer(vehicle + 0x46F0) != engine
                or pointer(vehicle + 0x4918) != shaft
                or pointer(vehicle + 0x4920) != gearbox
            ):
                raise ValueError("Object lifetime changed")
            if context() != after:
                raise ValueError("Context changed during observation")
            now = KERNEL.GetTickCount64()
            samples.append(
                {
                    "tick_ms": now,
                    "native_age_ms": now - native["host_tick_ms"],
                    "fields": fields,
                    "native": {
                        k: native.get(k)
                        for k in (
                            "rpm",
                            "gear",
                            "fuel_l",
                            "clutch",
                            "throttle",
                            "best_lap_s",
                            "total_laps",
                            "lap_invalidated",
                            "count_lap_flag",
                            "velocity_local_mps",
                        )
                    },
                    "engine_as_rpm_error": fields["engine_1f8"] - native["rpm"],
                    "engine_rad_s_as_rpm_error": fields["engine_1f8"] * 30 / math.pi
                    - native["rpm"],
                    "context_decision": "allowed",
                }
            )
            if math.sqrt(sum(v * v for v in native["velocity_local_mps"])) > 2:
                break
    except Exception as exc:
        error = f"{type(exc).__name__}: {exc}"
    finally:
        try:
            if entered:
                post("monitor_command")
            time.sleep(0.4)
            returned = get_json("/rest/sessions/GetGameState").get("inMonitor")
        except Exception as exc:
            cleanup_errors.append(f"Monitor status unavailable: {type(exc).__name__}: {exc}")
        finally:
            KERNEL.CloseHandle(handle)
            if child:
                try:
                    child.wait(timeout=args.seconds + 8)
                except subprocess.TimeoutExpired:
                    cleanup_errors.append(
                        "Native reader has not exited; its original bounded duration remains in effect"
                    )
            if console:
                console.close()
        result = {
            "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "samples": samples,
            "error": error,
            "cleanup_errors": cleanup_errors,
            "native_log": native_path.name if child else None,
            "native_exit_code": child.returncode if child else None,
            "native_sha256": digest,
            "bytes_read": total,
            "returned_to_monitor": returned,
            "physics_writes": 0,
            "reset_operations": 0,
            "ui_commands": (
                ["sessions/returnToMonitor"]
                if args.manual_drive
                else ["garage/drive", "sessions/returnToMonitor"]
            )
            if entered
            else [],
            "limitations": "Asynchronous reads of code-derived candidate fields. No restore or full engine state coverage.",
        }
        encoded = json.dumps(result, indent=2, allow_nan=False) + "\n"
        (PROJECT / "drivetrain-comparison.json").write_text(encoded)
        pathlib.Path(str(stem) + ".comparison.json").write_text(encoded)
        print(json.dumps({k: v for k, v in result.items() if k != "samples"}, indent=2))
        print("samples", len(samples))
    if error or cleanup_errors:
        raise RuntimeError(error or "; ".join(cleanup_errors))


if __name__ == "__main__":
    main()
