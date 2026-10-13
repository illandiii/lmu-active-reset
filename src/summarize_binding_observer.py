"""Summarize completed 14200 thread-bound wheel/hub observations."""

import argparse, collections, hashlib, json, statistics
from pathlib import Path


def summarize(path):
    rows = [json.loads(s) for s in path.read_text().splitlines() if s]
    return dict(
        source=path.name,
        sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        frames=len(rows),
        physics_threads=sorted({r["physics_thread_id"] for r in rows}),
        validation={
            phase: dict(collections.Counter(str(r[phase]["validation"]) for r in rows))
            for phase in ("before", "after", "last_transform")
        },
        wheel_call_patterns=dict(collections.Counter(str(r["wheel_calls"]) for r in rows)),
        hub_mismatches=sum(r["hub_mismatches"] for r in rows),
        max_abs_wheel_rate=max(
            (abs(s["rate"]) for r in rows for s in r["after"]["spins"]), default=None
        ),
        median_original_us=statistics.median(r["original_us"] for r in rows) if rows else None,
        median_observer_total_us=statistics.median(r["observer_total_us"] for r in rows)
        if rows
        else None,
        physics_writes=0,
        reset_operations=0,
        limits="Dynamics boundaries are same-thread samples. last_transform combines each wheel after its final natural update, not one simultaneous checkpoint. No suspension/tire/history restore or timing preservation tested.",
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("frames", type=Path)
    a = p.parse_args()
    r = summarize(a.frames)
    a.frames.with_suffix(".analysis.json").write_text(json.dumps(r, indent=2) + "\n")
    print(json.dumps(r, indent=2))
