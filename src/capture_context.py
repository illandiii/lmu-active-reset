"""Observe the native LMU payload together with the local UI's mode discriminator.

No REST POST, process memory access, injection or physics mutation.
"""

import argparse
import ctypes
from ctypes import wintypes
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time
import urllib.request
import zipfile

from context_gate import assess

PROJECT = Path(__file__).resolve().parents[1]
KERNEL = ctypes.WinDLL("kernel32", use_last_error=True)
KERNEL.GetTickCount64.restype = ctypes.c_uint64
KERNEL.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
KERNEL.OpenProcess.restype = wintypes.HANDLE
KERNEL.QueryFullProcessImageNameW.argtypes = (
    wintypes.HANDLE,
    wintypes.DWORD,
    wintypes.LPWSTR,
    ctypes.POINTER(wintypes.DWORD),
)
KERNEL.QueryFullProcessImageNameW.restype = wintypes.BOOL
KERNEL.CloseHandle.argtypes = (wintypes.HANDLE,)


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        raise ValueError("Local endpoint redirected; observation refused")


HTTP = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect())


def get_json(path):
    # Endpoints are source-defined constants; no URL is taken from game content.
    with HTTP.open("http://127.0.0.1:6397" + path, timeout=0.3) as response:
        payload = response.read(1024 * 1024 + 1)
        if len(payload) > 1024 * 1024:
            raise ValueError("Response too large")
        return json.loads(payload)


def verify(pid):
    inventory = json.loads((PROJECT / "inventory.json").read_text(encoding="utf-8-sig"))
    root = Path(inventory["install_path"])
    for item in inventory["files"]:
        if (
            hashlib.sha256((root / item["relative_path"]).read_bytes()).hexdigest().upper()
            != item["sha256"]
        ):
            raise ValueError("Installed build differs from pinned inventory")
    handle = KERNEL.OpenProcess(0x1000, False, pid)
    if not handle:
        raise OSError(ctypes.get_last_error(), "Cannot query LMU process path")
    try:
        buf = ctypes.create_unicode_buffer(32768)
        size = wintypes.DWORD(len(buf))
        if not KERNEL.QueryFullProcessImageNameW(handle, 0, buf, ctypes.byref(size)):
            raise OSError(ctypes.get_last_error(), "Cannot identify process")
        if Path(buf.value) != root / "Le Mans Ultimate.exe":
            raise ValueError("PID is not inventoried LMU")
    finally:
        KERNEL.CloseHandle(handle)
    contract = json.loads((PROJECT / "ui-contract.json").read_text(encoding="utf-8-sig"))
    with zipfile.ZipFile(root / "Bin/UI.zip") as archive:
        member = archive.read(contract["member"])
    if hashlib.sha256(member).hexdigest() != contract["sha256"]:
        raise ValueError("UI discriminator contract changed")
    chain = json.loads((PROJECT / "toolchain.json").read_text(encoding="utf-8-sig"))
    binary = PROJECT / "bin/diagnostic.exe"
    if hashlib.sha256(binary.read_bytes()).hexdigest().upper() != chain["bin_sha256"]:
        raise ValueError("Diagnostic binary differs from tested build")
    return binary, contract, chain["bin_sha256"]


def run(pid, seconds, label):
    binary, contract, binary_hash = verify(pid)
    return capture_verified(pid, seconds, label, binary, contract, binary_hash)


def capture_verified(
    pid, seconds, label, binary, contract, binary_hash, *, observation_build=14130
):
    """Read-only capture after a caller has verified its exact telemetry profile."""
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    basename = PROJECT / "logs" / f"{stamp}_{label}"
    native_path = basename.with_suffix(".jsonl")
    context_path = Path(str(basename) + ".context.jsonl")
    metadata = {
        "started_utc": datetime.now(timezone.utc).isoformat(),
        "observation_build": observation_build,
        "requested_seconds": seconds,
        "diagnostic_sha256": binary_hash,
        "ui_member_sha256": contract["sha256"],
        "rest_methods": ["GET"],
        "mode_semantics": "same SETTING_GRANDPRIX equality used by the installed UI",
        "limitation": "Sequential external observations; not simulation-thread authorization.",
    }
    Path(str(context_path) + ".metadata.json").write_text(
        json.dumps(metadata, indent=2), encoding="utf-8"
    )
    native_path.touch(exist_ok=False)
    counts = {}
    errors = 0
    last = None
    with Path(str(basename) + ".native-result.txt").open("w") as console:
        child = subprocess.Popen(
            [str(binary), str(pid), str(seconds), str(native_path)], stdout=console, stderr=console
        )
        try:
            with (
                native_path.open(encoding="utf-8") as incoming,
                context_path.open("w", encoding="utf-8") as output,
            ):
                pending = ""
                while child.poll() is None:
                    started = KERNEL.GetTickCount64()
                    try:
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
                        decision = assess(
                            before,
                            after,
                            game,
                            last,
                            now,
                            started,
                            observation_build=observation_build,
                        )
                        row = {
                            "host_tick_ms": now,
                            "native_sample": None if last is None else last["sample"],
                            "native_age_ms": None if last is None else now - last["host_tick_ms"],
                            "collection_ms": now - started,
                            "navigation": {
                                k: after.get(k)
                                for k in (
                                    "appBuild",
                                    "settingMode",
                                    "gameSession",
                                    "gameState",
                                    "navigationState",
                                    "gamePhase",
                                )
                            },
                            "game": {
                                k: game.get(k)
                                for k in (
                                    "MultiStintState",
                                    "inMonitor",
                                    "isReplayActive",
                                    "inControlOfVehicle",
                                    "playerVehicleLoaded",
                                )
                            },
                            **decision,
                        }
                    except Exception as error:
                        errors += 1
                        row = {
                            "host_tick_ms": KERNEL.GetTickCount64(),
                            "context_decision": "unknown",
                            "reason": "observation_failed",
                            "error_type": type(error).__name__,
                            "singleplayer_evidence": False,
                            "reset_permitted": False,
                        }
                    counts[row["context_decision"]] = counts.get(row["context_decision"], 0) + 1
                    output.write(json.dumps(row) + "\n")
                    output.flush()
                    time.sleep(0.25)
        finally:
            # The child has a finite lifetime. Do not terminate it while it owns the shared lock.
            child.wait()
    summary = {
        "context_log": context_path.name,
        "native_log": native_path.name,
        "decisions": counts,
        "observation_errors": errors,
        "native_exit_code": child.returncode,
        "reset_operations": 0,
    }
    Path(str(context_path) + ".summary.json").write_text(
        json.dumps(summary, indent=2), encoding="utf-8"
    )
    print(json.dumps(summary, indent=2))
    return child.returncode


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--pid", required=True, type=int)
    parser.add_argument("--seconds", type=int, default=60)
    parser.add_argument("--label", default="context")
    args = parser.parse_args()
    if not 1 <= args.seconds <= 3600 or not re.fullmatch(r"[a-zA-Z0-9_-]+", args.label):
        parser.error("Invalid duration or label")
    raise SystemExit(run(args.pid, args.seconds, args.label))
