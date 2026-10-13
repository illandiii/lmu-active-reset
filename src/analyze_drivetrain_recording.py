"""Quantify candidate-field agreement using an existing read-only capture."""

import argparse
import json
import math
from pathlib import Path
import statistics


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("recording", type=Path)
    a = p.parse_args()
    record = json.loads(a.recording.read_text())
    rows = record["samples"]
    positive = [r for r in rows if r["native"]["rpm"] > 0]
    settled = (
        [
            r
            for r in positive
            if r["tick_ms"] >= positive[0]["tick_ms"] + 5000
            and r["native"]["rpm"] > 1000
            and r["native"]["throttle"] == 0
            and math.sqrt(sum(v * v for v in r["native"]["velocity_local_mps"])) < 0.5
        ]
        if positive
        else []
    )

    def errors(key):
        values = [abs(r[key]) for r in settled]
        return {"median": statistics.median(values), "max": max(values)} if values else None

    result = {
        "source": a.recording.name,
        "samples": len(rows),
        "positive_rpm_samples": len(positive),
        "settled_samples": len(settled),
        "settled_rule": "At least 5 s after first positive RPM; native RPM >1000; zero throttle; speed <0.5 m/s.",
        "rpm_range": [
            min(r["native"]["rpm"] for r in settled),
            max(r["native"]["rpm"] for r in settled),
        ]
        if settled
        else None,
        "rad_s_to_rpm_absolute_error": errors("engine_rad_s_as_rpm_error"),
        "raw_as_rpm_absolute_error": errors("engine_as_rpm_error"),
        "fuel_absolute_error_max": max(
            (abs(r["fields"]["vehicle_448"] - r["native"]["fuel_l"]) for r in rows), default=None
        ),
        "gears": sorted({r["native"]["gear"] for r in rows}),
        "physics_writes": record["physics_writes"],
        "reset_operations": record["reset_operations"],
        "limits": "Asynchronous 14130 reads; verifies candidate scale at idle, not complete engine/gearbox state or compatibility with 14150.",
    }
    output = a.recording.with_suffix(".analysis.json")
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
