"""Discover the current worker owner, then run the bounded hotkey host."""

import argparse
from pathlib import Path

from run_physics_observer import modules
from run_physical_reset_host_25661166 import run


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
    args.owner_thread = 0  # Native callback discovery, never sampled lock ownership.
    resident = {m["name"].lower() for m in modules(args.pid)}
    args.revision = next(
        (
            v
            for v in (8,)
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
