"""Discover the current worker owner, then run the bounded hotkey host."""

import argparse
import struct
import time
from pathlib import Path

from capture_preparation_steam25661166 import verify_telemetry
from run_physics_observer import modules, k, read_process
from run_physical_reset_host_25661166 import run


def observed_worker(pid):
    verify_telemetry(pid)
    image = next(m for m in modules(pid) if m["name"].lower() == "le mans ultimate.exe")
    handle = k.OpenProcess(0x410, False, pid)
    if not handle:
        raise OSError("Cannot read LMU; run under the same Windows user/privilege level")
    owners = []
    try:
        for _ in range(100):
            raw = read_process(handle, image["base"] + 0x3B8A968, 40)
            recursion, owner = struct.unpack_from("<iQ", raw, 12)
            if recursion > 0 and owner:
                owners.append(owner)
            time.sleep(0.01)
    finally:
        k.CloseHandle(handle)
    if len(owners) < 5 or len(set(owners)) != 1:
        raise ValueError("Physics worker not stable; remain in GUIDA and retry")
    # Observation is only a candidate. The native host additionally checks its
    # actual callback thread and lock ownership on every command.
    return owners[0]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--save-key", default="F6")
    parser.add_argument("--restore-key", default="F7")
    parser.add_argument("--offline-launch-confirmed", action="store_true")
    parser.add_argument("--allow-writes", action="store_true")
    args = parser.parse_args()
    if not args.offline_launch_confirmed or not args.allow_writes:
        parser.error("Offline launch confirmation and write opt-in required")
    args.owner_thread = observed_worker(args.pid)
    resident = {m["name"].lower() for m in modules(args.pid)}
    args.revision = next(
        (
            v
            for v in (6,)
            if f"lmu-physical-reset-v{v}-25661166.dll" not in resident
            and (
                Path(__file__).resolve().parents[1] / f"bin/lmu-physical-reset-v{v}-25661166.dll"
            ).exists()
        ),
        None,
    )
    if args.revision is None:
        raise ValueError("All built host revisions are resident; restart LMU before retrying")
    args.seconds = 240
    args.hotkeys = True
    args.request_file = Path("work/unused-hotkey-command.json")
    run(args)
