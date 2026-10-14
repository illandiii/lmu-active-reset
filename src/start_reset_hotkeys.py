"""Discover the current worker owner, then run the bounded hotkey host."""

import argparse
from pathlib import Path
import time
from urllib.error import URLError
from capture_context import get_json

from run_physical_reset_host_25661166 import run


def wait_for_driving():
    print("In attesa che il player entri in GUIDA in Practice offline...", flush=True)
    while True:
        try:
            state = get_json("/navigation/state")["state"]
            game = get_json("/rest/sessions/GetGameState")
            if (
                state.get("appBuild") == 14200
                and state.get("settingMode") == "SETTING_GRANDPRIX"
                and state.get("gameSession") in ("PRACTICE1", "PRACTICE2", "PRACTICE3", "PRACTICE4")
                and game.get("inControlOfVehicle") is True
                and game.get("inMonitor") is False
                and game.get("playerVehicleLoaded") is True
                and game.get("isReplayActive") is False
            ):
                break
        except (TimeoutError, URLError, ConnectionError):
            pass
        time.sleep(0.5)


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
    wait_for_driving()
    args.revision = 10
    # A just-closed console may still be draining its native callback.
    from audit_snapshot_25661166 import audit

    for attempt in range(20):
        try:
            audit(args.pid)
            break
        except ValueError as error:
            if str(error) != "Snapshot module not cleanly stopped" or attempt == 19:
                raise
            time.sleep(0.2)
    args.seconds = 240
    args.hotkeys = True
    args.request_file = Path("work/unused-hotkey-command.json")
    run(args)
