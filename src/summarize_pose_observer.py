"""Summarize coherent pose observations without claiming a live restore."""

import argparse
import json
import statistics
from pathlib import Path


def summarize(prefix):
    rows = [
        json.loads(line)
        for line in Path(str(prefix) + ".frames.jsonl").read_text().splitlines()
        if line
    ]
    summary = json.loads(Path(str(prefix) + ".summary.json").read_text())
    native = [
        json.loads(line)
        for line in Path(str(prefix) + ".native.jsonl").read_text().splitlines()
        if line
    ]
    if not rows:
        raise ValueError("No frames")
    result = {
        "source_prefix": prefix.name,
        "persisted_frames": len(rows),
        "physics_observations": summary["observations"],
        "root_counts": sorted(set(r["root_count"] for r in rows)),
        "total_counts": sorted(set(r["total_count"] for r in rows)),
        "phase_error_frames": sum(r["phase_errors"] != 0 for r in rows),
        "contact_counts": sorted(set(tuple(r["contacts"]) for r in rows)),
        "fresh_masks_before_forces": sorted(set(tuple(r["fresh_mask_at_force"]) for r in rows)),
        "wheel_body_indices": sorted(set(tuple(r["wheel_body_index"]) for r in rows)),
        "strict_preparation_before_status": sorted(set(r["prepare_before"] for r in rows)),
        "strict_preparation_after_status": sorted(set(r["prepare_after"] for r in rows)),
        "strict_saved_with_current_mass_status": sorted(
            set(r["prepare_saved_with_current_mass"] for r in rows)
        ),
        "max_inertia_error": max(r["inertia_error"] for r in rows),
        "max_omega_error": max(r["omega_error"] for r in rows),
        "median_original_call_us": statistics.median(r["original_us"] for r in rows),
        "median_observer_total_us": statistics.median(r["observer_total_us"] for r in rows),
        "brake_min": min(r.get("brake", 0) for r in native),
        "brake_max": max(r.get("brake", 0) for r in native),
        "restored": summary["restored"],
        "physics_writes": 0,
        "live_restore_operations": 0,
        "limitations": "Typed coherent numerical observations, not complete checkpoint coverage. Timings are sampled observations, not reset latency.",
    }
    Path(str(prefix) + ".analysis.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("prefix", type=Path)
    summarize(p.parse_args().prefix)
