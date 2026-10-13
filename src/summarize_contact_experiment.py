"""Summarize the actual stationary-contact experiment; never infer a car reset."""

import argparse
import json
import math
from pathlib import Path


def summarize(prefix):
    summary = json.loads(Path(str(prefix) + ".summary.json").read_text())
    native = [
        json.loads(line)
        for line in Path(str(prefix) + ".native.jsonl").read_text().splitlines()
        if line
    ]
    results = summary["rebuild_results"]
    ticks = [r["tick_ms"] for r in results if r["retired"]]
    if len(ticks) != 4:
        raise ValueError("No complete four-wheel operation to summarize")
    event = min(ticks)
    reference = summary["before_command"]["position_m"]

    def metrics(rows):
        if not rows:
            return {"samples": 0}
        return {
            "samples": len(rows),
            "duration_s": (rows[-1]["host_tick_ms"] - rows[0]["host_tick_ms"]) / 1000,
            "max_speed_mps": max(math.hypot(*r["velocity_local_mps"]) for r in rows),
            "max_distance_from_precommand_m": max(
                math.dist(r["position_m"], reference) for r in rows
            ),
            "nonfinite_motion_samples": sum(
                not all(math.isfinite(x) for x in r["velocity_local_mps"] + r["position_m"])
                for r in rows
            ),
            "gears": sorted(set(r["gear"] for r in rows)),
        }

    before = [r for r in native if r["host_tick_ms"] < event]
    after = [r for r in native if r["host_tick_ms"] >= event]
    observations = [
        json.loads(line)
        for line in Path(str(prefix) + ".observations.jsonl").read_text().splitlines()
        if line
    ]
    samples = [s for r in observations for s in r["samples"] if s["samples"]]
    report = {
        "source_prefix": prefix.name,
        "scope": "One stationary contact rebuild in offline Practice; no teleport or checkpoint restore.",
        "completed_four_wheels": summary["completed_mask"] == 15 and summary["failed_mask"] == 0,
        "retired_contacts": sum(r["retired"] for r in results),
        "post_contacts": [r["post_count"] for r in results],
        "physics_depth_field_writes": summary["physics_writes"],
        "vehicle_checkpoint_resets": 0,
        "native_call_duration_us": [r["duration_us"] for r in results],
        "duration_limit": "Native refresh only; excludes wrapper observation overhead, hotkey latency and whole reset.",
        "event_tick_span_ms": max(ticks) - min(ticks),
        "bad_prepost_sample_records": sum(s["pre_bad"] != 0 or s["post_bad"] != 0 for s in samples),
        "physics_thread_observations": summary["observations"],
        "physics_thread_skipped": summary["skipped"],
        "before": metrics(before),
        "after": metrics(after),
        "original_instructions_restored": summary["original_instructions_restored"],
        "module_status": summary["status"],
        "module_error": summary["error"],
        "preexisting_valid_best_lap": summary["before_command"]["best_lap_s"] > 0,
        "record_preservation_tested": False,
        "post_teleport_tested": False,
        "limitation": "No valid best lap was present. This does not validate moving checkpoint restore, tire-history restore, collision coherence after relocation, or timing preservation.",
    }
    Path(str(prefix) + ".analysis.json").write_text(
        json.dumps(report, indent=2, allow_nan=False) + "\n"
    )
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prefix", type=Path)
    summarize(parser.parse_args().prefix)
