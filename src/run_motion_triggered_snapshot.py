"""Wait for three fresh offline throttle samples before starting a bounded observer."""

import argparse, datetime, json, math, subprocess, time
from pathlib import Path
from capture_context import PROJECT, KERNEL, get_json
from capture_preparation_steam25661166 import verify_telemetry
from context_gate import assess_player_observation


def throttle_active(sample, decision):
    value = sample.get("throttle") if isinstance(sample, dict) else None
    return (
        decision.get("context_decision") == "allowed"
        and type(value) in (int, float)
        and math.isfinite(value)
        and 0.1 < value <= 1
    )


def wait_for_throttle(pid, seconds):
    binary, _, _ = verify_telemetry(pid)
    prefix = (
        PROJECT
        / "logs"
        / (
            datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
            + "_throttle_trigger"
        )
    )
    path = prefix.with_suffix(".jsonl")
    path.touch(exist_ok=False)
    process = None
    pending = ""
    last = None
    tick = None
    streak = 0
    triggered = False
    try:
        with prefix.with_suffix(".console.txt").open("w") as console:
            process = subprocess.Popen(
                [str(binary), str(pid), str(seconds + 2), str(path)], stdout=console, stderr=console
            )
            print(
                json.dumps(
                    {
                        "capture": "waiting_for_throttle",
                        "timeout_seconds": seconds,
                        "physics_writes": 0,
                    }
                ),
                flush=True,
            )
            deadline = time.monotonic() + seconds
            with path.open() as incoming:
                while time.monotonic() < deadline and process.poll() is None:
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
                    decision = assess_player_observation(
                        before, after, game, last, now, started, observation_build=14200
                    )
                    if decision["reason"] in (
                        "multiplayer_or_replay",
                        "non_practice_session",
                        "network_scoring_conflicts_with_singleplayer",
                    ):
                        raise ValueError("Offline Practice ended")
                    if last and last.get("host_tick_ms") != tick:
                        tick = last.get("host_tick_ms")
                        streak = streak + 1 if throttle_active(last, decision) else 0
                    elif decision["context_decision"] != "allowed":
                        streak = 0
                    if streak >= 3:
                        triggered = True
                        break
                    time.sleep(0.1)
    finally:
        if process and process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
        prefix.with_suffix(".result.json").write_text(
            json.dumps({"triggered": triggered, "samples": streak, "physics_writes": 0}, indent=2)
        )
    return triggered


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--pid", type=int, required=True)
    p.add_argument("--wait-seconds", type=int, default=120)
    p.add_argument("--offline-launch-confirmed", action="store_true")
    a = p.parse_args()
    if not a.offline_launch_confirmed or not 10 <= a.wait_seconds <= 180:
        p.error("Confirmed offline launch and 10..180 second window required")
    if wait_for_throttle(a.pid, a.wait_seconds):
        from run_snapshot_observer_v28_25661166 import run

        print(json.dumps({"capture": "throttle_detected"}), flush=True)
        run(a.pid, 60, True, True)
    else:
        print(
            json.dumps(
                {"capture": "not_started", "reason": "no_verified_throttle", "physics_writes": 0}
            ),
            flush=True,
        )
