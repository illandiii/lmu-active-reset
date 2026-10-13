"""Summarize real diagnostic JSONL; never infer offline from game_mode=0."""

import argparse
from collections import Counter
import json
import math
from pathlib import Path
import statistics


def percentile(values, p):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(p * len(ordered)) - 1)] if ordered else None


def summarize(path):
    rows = [json.loads(line) for line in path.read_text(encoding="utf-8-sig").splitlines() if line]
    if not rows:
        return {"file": path.name, "samples": 0}
    timings = [r["lock_copy_us"] for r in rows]
    transitions = []
    previous = None
    keys = ("session", "phase", "in_realtime", "control", "in_garage", "in_pits", "total_laps")
    for row in rows:
        current = {k: row.get(k) for k in keys}
        if current != previous:
            transitions.append({"elapsed_s": row["elapsed_s"], **current})
            previous = current
    speed = [
        math.sqrt(sum(v * v for v in r.get("velocity_local_mps", [0, 0, 0]))) * 3.6 for r in rows
    ]
    return {
        "file": path.name,
        "samples": len(rows),
        "duration_observed_s": rows[-1]["elapsed_s"],
        "states": {
            k: dict(Counter(str(r.get(k)) for r in rows))
            for k in keys + ("game_mode", "bounds_ok", "fresh_observed", "reset_decision")
        },
        "lock_copy_us": {
            "median": statistics.median(timings),
            "p95": percentile(timings, 0.95),
            "p99": percentile(timings, 0.99),
            "maximum": max(timings),
        },
        "max_speed_kph": max(speed),
        "best_lap_values_s": sorted({r["best_lap_s"] for r in rows if "best_lap_s" in r}),
        "observed_event_arrays": [
            json.loads(s) for s in sorted({json.dumps(r["events"]) for r in rows})
        ],
        "transitions": transitions,
        "offline_proof": "not assessed by this native-log summary; see paired context log where available",
        "reset_executed": False,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("logs", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    result = [
        summarize(p)
        for p in sorted(args.logs.glob("*.jsonl"))
        if not p.name.endswith((".context.jsonl", ".observer.jsonl"))
    ]
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(
        json.dumps(
            [
                {k: r[k] for k in ("file", "samples", "max_speed_kph", "lock_copy_us")}
                for r in result
            ],
            indent=2,
        )
    )
