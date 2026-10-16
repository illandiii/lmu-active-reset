"""Load the bounded observational DLL into the user-prepared offline LMU instance.

This writes the DLL path into a small allocation and calls standard LoadLibraryW.
The DLL installs one verified observational hook; there is NO physics reset.
Any denied process access stops the operation; no privilege escalation or bypass.
"""

from sector.paths import PROJECT_ROOT

import argparse
import ctypes
import datetime
import hashlib
import json
import pathlib
import subprocess
import time
from ctypes import wintypes as W
from sector.telemetry.capture_context import verify, get_json, KERNEL
from sector.telemetry.context_gate import assess


ROOT = PROJECT_ROOT
PROJECT = ROOT


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
            for n in ("callbacks", "observations", "skipped", "sequence", "writer_thread")
        ],
        *[(n, ctypes.c_uint64) for n in ("tick_ms", "begin_qpc", "duration_qpc")],
        ("dt", ctypes.c_double),
        *[
            (n, ctypes.c_double * 3)
            for n in ("pre_position", "post_position", "pre_velocity", "post_velocity")
        ],
        ("body_count", ctypes.c_uint64),
        ("qpc_frequency", ctypes.c_uint64),
    ]


assert ctypes.sizeof(IPC) == 232 and IPC.dt.offset == 112


class MODULEENTRY(ctypes.Structure):
    _fields_ = [
        ("dwSize", W.DWORD),
        ("th32ModuleID", W.DWORD),
        ("th32ProcessID", W.DWORD),
        ("GlblcntUsage", W.DWORD),
        ("ProccntUsage", W.DWORD),
        ("modBaseAddr", ctypes.c_void_p),
        ("modBaseSize", W.DWORD),
        ("hModule", W.HMODULE),
        ("szModule", W.WCHAR * 256),
        ("szExePath", W.WCHAR * 260),
    ]


k = ctypes.WinDLL("kernel32", use_last_error=True)


def api(name, args, restype):
    f = getattr(k, name)
    f.argtypes = args
    f.restype = restype
    return f


api("OpenProcess", [W.DWORD, W.BOOL, W.DWORD], W.HANDLE)
api("CloseHandle", [W.HANDLE], W.BOOL)
api("CreateToolhelp32Snapshot", [W.DWORD, W.DWORD], W.HANDLE)
api("Module32FirstW", [W.HANDLE, ctypes.POINTER(MODULEENTRY)], W.BOOL)
api("Module32NextW", [W.HANDLE, ctypes.POINTER(MODULEENTRY)], W.BOOL)
api(
    "ReadProcessMemory",
    [W.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)],
    W.BOOL,
)
api(
    "WriteProcessMemory",
    [W.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)],
    W.BOOL,
)
api(
    "VirtualAllocEx",
    [W.HANDLE, ctypes.c_void_p, ctypes.c_size_t, W.DWORD, W.DWORD],
    ctypes.c_void_p,
)
api("VirtualFreeEx", [W.HANDLE, ctypes.c_void_p, ctypes.c_size_t, W.DWORD], W.BOOL)
api("GetModuleHandleW", [W.LPCWSTR], W.HMODULE)
api("GetProcAddress", [W.HMODULE, ctypes.c_char_p], ctypes.c_void_p)
api("GetModuleHandleExW", [W.DWORD, ctypes.c_void_p, ctypes.POINTER(W.HMODULE)], W.BOOL)
api("GetModuleFileNameW", [W.HMODULE, W.LPWSTR, W.DWORD], W.DWORD)
api(
    "CreateRemoteThread",
    [
        W.HANDLE,
        ctypes.c_void_p,
        ctypes.c_size_t,
        ctypes.c_void_p,
        ctypes.c_void_p,
        W.DWORD,
        ctypes.POINTER(W.DWORD),
    ],
    W.HANDLE,
)
api("WaitForSingleObject", [W.HANDLE, W.DWORD], W.DWORD)
api(
    "CreateFileMappingW",
    [W.HANDLE, ctypes.c_void_p, W.DWORD, W.DWORD, W.DWORD, W.LPCWSTR],
    W.HANDLE,
)
api("MapViewOfFile", [W.HANDLE, W.DWORD, W.DWORD, W.DWORD, ctypes.c_size_t], ctypes.c_void_p)
api("UnmapViewOfFile", [ctypes.c_void_p], W.BOOL)


def modules(pid):
    snapshot = k.CreateToolhelp32Snapshot(0x18, pid)
    if snapshot == ctypes.c_void_p(-1).value:
        raise OSError(ctypes.get_last_error(), "Module enumeration denied")
    try:
        entry = MODULEENTRY()
        entry.dwSize = ctypes.sizeof(entry)
        rows = []
        ok = k.Module32FirstW(snapshot, ctypes.byref(entry))
        while ok:
            rows.append(
                {
                    "name": entry.szModule,
                    "base": entry.modBaseAddr,
                    "size": entry.modBaseSize,
                    "path": entry.szExePath,
                }
            )
            ok = k.Module32NextW(snapshot, ctypes.byref(entry))
        return rows
    finally:
        k.CloseHandle(snapshot)


def read_process(handle, address, size):
    b = ctypes.create_string_buffer(size)
    n = ctypes.c_size_t()
    if not k.ReadProcessMemory(handle, address, b, size, ctypes.byref(n)) or n.value != size:
        raise OSError(ctypes.get_last_error(), "Code read denied")
    return b.raw


def context_valid(state):
    return (
        state.get("appBuild") == 14130
        and state.get("settingMode") == "SETTING_GRANDPRIX"
        and state.get("gameSession") in ("PRACTICE1", "PRACTICE2", "PRACTICE3", "PRACTICE4")
        and state.get("gameState") == "GSTATE_DYN"
    )


def load_observer(handle, pid, dll, ipc):
    local_function = k.GetProcAddress(k.GetModuleHandleW("kernel32.dll"), b"LoadLibraryW")
    owner = W.HMODULE()
    if not local_function or not k.GetModuleHandleExW(6, local_function, ctypes.byref(owner)):
        raise OSError("Loader resolution failed")
    path = ctypes.create_unicode_buffer(32768)
    if not k.GetModuleFileNameW(owner, path, len(path)):
        raise OSError("Loader module resolution failed")
    name = pathlib.Path(path.value).name.lower()
    matching = [m for m in modules(pid) if m["name"].lower() == name]
    if len(matching) != 1:
        raise ValueError("Ambiguous remote loader module")
    remote_function = matching[0]["base"] + local_function - owner.value
    payload = (str(dll) + "\0").encode("utf-16-le")
    remote = k.VirtualAllocEx(handle, None, len(payload), 0x3000, 0x04)
    if not remote:
        raise OSError(ctypes.get_last_error(), "Allocation denied; no fallback")
    thread = None
    finished = False
    try:
        n = ctypes.c_size_t()
        buffer = ctypes.create_string_buffer(payload)
        if not k.WriteProcessMemory(
            handle, remote, buffer, len(payload), ctypes.byref(n)
        ) or n.value != len(payload):
            raise OSError(ctypes.get_last_error(), "DLL path write denied; no fallback")
        tid = W.DWORD()
        thread = k.CreateRemoteThread(
            handle, None, 0, remote_function, remote, 0, ctypes.byref(tid)
        )
        if not thread:
            raise OSError(ctypes.get_last_error(), "LoadLibrary thread denied; no fallback")
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            ipc.heartbeat_ms = KERNEL.GetTickCount64()
            result = k.WaitForSingleObject(thread, 50)
            if result == 0:
                finished = True
                break
            if result != 258:
                raise OSError("Loader wait failed")
        if not finished:
            raise TimeoutError("Loader outcome pending; DLL path allocation retained")
    finally:
        if thread:
            k.CloseHandle(thread)
        if finished or not thread:
            k.VirtualFreeEx(handle, remote, 0, 0x8000)
    if not any(pathlib.Path(m["path"]) == dll for m in modules(pid)):
        raise RuntimeError("Observer module was not loaded")


def run(pid, seconds, execute, offline_launch_confirmed=False):
    if execute and not offline_launch_confirmed:
        raise ValueError("Direct EXE offline launch must be confirmed before loading the observer")
    binary, contract, binary_hash = verify(pid)
    dll = (PROJECT / "bin/lmu-physics-observer.dll").resolve()
    state = get_json("/navigation/state")["state"]
    game = get_json("/rest/sessions/GetGameState")
    if not context_valid(state) or game.get("inMonitor") is not True:
        raise ValueError("Must start in verified singleplayer Practice monitor")
    listed = modules(pid)
    if any(
        "easyanticheat" in m["name"].lower() or m["name"].lower().startswith("eac") for m in listed
    ):
        raise ValueError("EAC module present; stop without changes")
    if any("lmu-physics-observer" in m["name"].lower() for m in listed):
        raise ValueError("Observer already resident; no duplicate load")
    main = next(m for m in listed if m["name"].lower() == "le mans ultimate.exe")
    handle = k.OpenProcess(0x43A if execute else 0x410, False, pid)
    if not handle:
        raise OSError(
            ctypes.get_last_error(),
            "Requested ordinary process access denied; no escalation or bypass",
        )
    mapping = None
    view = None
    ipc = None
    native = None
    record = None
    target = main["base"] + 0xEAF350
    original = read_process(handle, target, 32)
    if not original.startswith(bytes.fromhex("488bc44889580848897010574881ecc0000000")):
        k.CloseHandle(handle)
        raise ValueError("Target instructions mismatch")
    if not context_valid(get_json("/navigation/state")["state"]):
        k.CloseHandle(handle)
        raise ValueError("Context changed")
    if not execute:
        k.CloseHandle(handle)
        print(
            json.dumps(
                {
                    "preflight": "passed",
                    "pid": pid,
                    "dll_sha256": hashlib.sha256(dll.read_bytes()).hexdigest(),
                    "context": {k: state[k] for k in ("appBuild", "settingMode", "gameSession")},
                    "target": "RVA 0xEAF350, exact prolog matched",
                    "loaded": False,
                }
            )
        )
        return
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    basename = PROJECT / "logs" / f"{stamp}_physics_observer"
    native_path = pathlib.Path(str(basename) + ".native.jsonl")
    path = pathlib.Path(str(basename) + ".observer.jsonl")
    metadata = {
        "pid": pid,
        "requested_seconds": seconds,
        "dll_sha256": hashlib.sha256(dll.read_bytes()).hexdigest(),
        "main_exe_sha256": hashlib.sha256(pathlib.Path(main["path"]).read_bytes()).hexdigest(),
        "native_diagnostic_sha256": binary_hash,
        "launch_context": "Operator confirmed direct EXE offline launch. Practice singleplayer checked through the pinned UI contract; no EAC module observed. Module absence alone is not launch proof.",
        "mechanism": "Standard LoadLibraryW; one MinHook observational detour; no vehicle writes; DLL remains resident until game closes.",
        "created_files_in_installation": [],
        "reset_operations": 0,
    }
    pathlib.Path(str(basename) + ".metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    try:
        mapping = k.CreateFileMappingW(
            ctypes.c_void_p(-1), None, 0x04, 0, ctypes.sizeof(IPC), f"Local\\LMU_Observer_v1_{pid}"
        )
        if not mapping or ctypes.get_last_error() == 183:
            raise RuntimeError("IPC mapping already exists or cannot be created")
        view = k.MapViewOfFile(mapping, 0xF001F, 0, 0, ctypes.sizeof(IPC))
        if not view:
            raise OSError(ctypes.get_last_error(), "IPC map failed")
        ipc = IPC.from_address(view)
        ctypes.memset(view, 0, ctypes.sizeof(IPC))
        ipc.magic = 0x4C4D554F425331
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
            raise RuntimeError(f"Observer refused: status {ipc.status}, error {ipc.error}")
        print(
            json.dumps({"observer": "armed", "pid": pid, "log": path.name, "seconds": seconds}),
            flush=True,
        )
        console = pathlib.Path(str(basename) + ".native-result.txt").open("w")
        native_path.touch(exist_ok=False)
        native = subprocess.Popen(
            [str(binary), str(pid), str(seconds + 2), str(native_path)],
            stdout=console,
            stderr=console,
        )
        deadline = time.monotonic() + seconds
        last_sequence = -1
        last_native = None
        pending = ""
        decisions = {}
        with native_path.open() as incoming, path.open("w") as output:
            while time.monotonic() < deadline and ipc.status == 2 and native.poll() is None:
                started = KERNEL.GetTickCount64()
                before = get_json("/navigation/state")["state"]
                game = get_json("/rest/sessions/GetGameState")
                if not context_valid(before):
                    raise ValueError("Practice singleplayer context ended")
                pending += incoming.read()
                lines = pending.split("\n")
                pending = lines.pop()
                for line in lines:
                    if line:
                        last_native = json.loads(line)
                after = get_json("/navigation/state")["state"]
                now = KERNEL.GetTickCount64()
                if not context_valid(after):
                    raise ValueError("Practice singleplayer context ended")
                decision = assess(before, after, game, last_native, now, started)
                ipc.allow_observation = int(decision["context_decision"] == "allowed")
                ipc.heartbeat_ms = now
                decisions[decision["context_decision"]] = (
                    decisions.get(decision["context_decision"], 0) + 1
                )
                sequence = ipc.sequence
                if not sequence & 1 and sequence != last_sequence:
                    copy = IPC.from_buffer_copy(ctypes.string_at(view, ctypes.sizeof(IPC)))
                    if sequence == ipc.sequence and sequence == copy.sequence and copy.observations:
                        row = {
                            "sequence": sequence,
                            "host_tick_ms": copy.tick_ms,
                            "physics_thread_id": copy.writer_thread,
                            "dt_s": copy.dt,
                            "integrate_duration_us": copy.duration_qpc / copy.qpc_frequency * 1e6,
                            "pre_position": list(copy.pre_position),
                            "post_position": list(copy.post_position),
                            "pre_velocity_world": list(copy.pre_velocity),
                            "post_velocity_world": list(copy.post_velocity),
                            "body_count": copy.body_count,
                            "callbacks": copy.callbacks,
                            "observations": copy.observations,
                            "context_decision": decision["context_decision"],
                            "reset_operations": 0,
                        }
                        output.write(json.dumps(row, allow_nan=False) + "\n")
                        output.flush()
                        last_sequence = sequence
                time.sleep(0.10)
        record = {
            "context_decisions": decisions,
            "callbacks": ipc.callbacks,
            "observations": ipc.observations,
            "skipped": ipc.skipped,
            "physics_thread_id": ipc.writer_thread,
        }
    finally:
        if ipc:
            ipc.allow_observation = 0
            ipc.stop = 1
            for _ in range(150):
                if ipc.status in (3, 4, 5):
                    break
                time.sleep(0.02)
            summary = {
                "status": ipc.status,
                "error": ipc.error,
                "original_instructions_restored": read_process(handle, target, 32) == original,
                "module_resident_until_game_exit": True,
                "reset_operations": 0,
                **(record or {}),
            }
            pathlib.Path(str(basename) + ".summary.json").write_text(
                json.dumps(summary, indent=2) + "\n"
            )
            print(json.dumps(summary, indent=2), flush=True)
        if native:
            native.wait(timeout=seconds + 5)
            console.close()
        if view:
            k.UnmapViewOfFile(view)
        if mapping:
            k.CloseHandle(mapping)
        k.CloseHandle(handle)


if __name__ == "__main__":
    a = argparse.ArgumentParser()
    a.add_argument("--pid", type=int, required=True)
    a.add_argument("--seconds", type=int, default=150)
    a.add_argument("--load", action="store_true")
    a.add_argument(
        "--offline-launch-confirmed",
        action="store_true",
        help="Attest that this instance was launched directly via the game EXE for offline use; does not override context or build checks",
    )
    args = a.parse_args()
    if not 10 <= args.seconds <= 180:
        a.error("duration must be 10..180 seconds")
    if args.load and not args.offline_launch_confirmed:
        a.error("--load requires --offline-launch-confirmed for this game instance")
    run(args.pid, args.seconds, args.load, args.offline_launch_confirmed)
