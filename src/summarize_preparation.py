"""Summarize a completed native recording; no game access."""

import argparse
import hashlib
import json
import math
from pathlib import Path


def summarize(path):
    samples = [json.loads(line) for line in path.read_text().splitlines() if line]
    driving = [
        s
        for s in samples
        if s.get("in_realtime") is True
        and s.get("control") == 0
        and s.get("telemetry_matched") is True
    ]

    def extent(key):
        values = [
            s[key] for s in driving if type(s.get(key)) in (int, float) and math.isfinite(s[key])
        ]
        return None if not values else {"min": min(values), "max": max(values)}

    bests = [
        {"sample": s["sample"], "best_lap_s": s["best_lap_s"], "total_laps": s["total_laps"]}
        for s in driving
        if type(s.get("best_lap_s")) in (int, float) and s["best_lap_s"] > 0
    ]
    speeds = [
        math.sqrt(sum(x * x for x in s["velocity_local_mps"])) * 3.6
        for s in driving
        if isinstance(s.get("velocity_local_mps"), list)
    ]
    return {
        "source": path.name,
        "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "native_samples": len(samples),
        "driving_samples": len(driving),
        "observed_builds": sorted({s["game_version"] for s in samples}),
        "rpm": extent("rpm"),
        "fuel_l": extent("fuel_l"),
        "battery_fraction": extent("battery_fraction"),
        "gears": sorted({s["gear"] for s in driving}),
        "count_lap_flags": sorted({s["count_lap_flag"] for s in driving}),
        "max_speed_kmh": max(speeds) if speeds else None,
        "total_laps": extent("total_laps"),
        "first_valid_best": bests[0] if bests else None,
        "last_valid_best": bests[-1] if bests else None,
        "reset_operations": 0,
        "limits": "Baseline observations only; no reset, record-preservation or post-reset timing test.",
    }


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("native_log", type=Path)
    args = p.parse_args()
    result = summarize(args.native_log)
    output = args.native_log.with_suffix(".analysis.json")
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
