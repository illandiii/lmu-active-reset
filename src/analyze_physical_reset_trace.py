"""Summarize captured state changes without inferring their native cause."""

import argparse
from collections import Counter
import json
import math
from pathlib import Path


def body_delta(before, after):
    return {
        "position_m": math.dist(before[:3], after[:3]),
        "velocity_ms": math.dist(before[3:6], after[3:6]),
        "orientation_max_abs": max(abs(a - b) for a, b in zip(before[6:15], after[6:15])),
        "angular_momentum": math.dist(before[15:18], after[15:18]),
    }


def rigid_transform_fit(before, after):
    """Fit the root pose, then independently check all five bodies and vectors."""
    if before["valid"] & after["valid"] & 31 != 31:
        return None
    a, b = before["bodies"][0], after["bodies"][0]
    # R_delta = R_after * transpose(R_before), matrices stored row-major.
    rotation = [
        [sum(b[6 + i * 3 + k] * a[6 + j * 3 + k] for k in range(3)) for j in range(3)]
        for i in range(3)
    ]

    def rotate(vector):
        return [sum(x * y for x, y in zip(row, vector)) for row in rotation]

    translation = [x - y for x, y in zip(b[:3], rotate(a[:3]))]
    errors = []
    for a, b in zip(before["bodies"], after["bodies"]):
        expected_position = [x + y for x, y in zip(rotate(a[:3]), translation)]
        expected_rotation = [
            sum(rotation[i][k] * a[6 + k * 3 + j] for k in range(3))
            for i in range(3)
            for j in range(3)
        ]
        errors.append(
            {
                "position_m": math.dist(expected_position, b[:3]),
                "velocity_ms": math.dist(rotate(a[3:6]), b[3:6]),
                "angular_momentum": math.dist(rotate(a[15:18]), b[15:18]),
                "orientation_max_abs": max(abs(x - y) for x, y in zip(expected_rotation, b[6:15])),
            }
        )
    return dict(rotation=rotation, translation=translation, residuals=errors)


def change(before, after):
    bodies = []
    for index, (a, b) in enumerate(zip(before["bodies"], after["bodies"])):
        valid = before["valid"] & after["valid"] & (1 << index)
        bodies.append(body_delta(a, b) if valid else None)
    return {
        "from_phase": before["phase"],
        "to_phase": after["phase"],
        "from_step": before["step"],
        "to_step": after["step"],
        "elapsed_tick_ms": after["tick"] - before["tick"],
        "bodies": bodies,
        "rigid_transform_fit": rigid_transform_fit(before, after),
    }


def analyze(records):
    saved = previous = pending = None
    requests = {}
    for row in records:
        if len(row["bodies"]) != 5 or any(
            len(body) != 18 or not all(math.isfinite(value) for value in body)
            for body in row["bodies"]
        ):
            raise ValueError("Expected five finite 18-component body records")
        phase = row["phase"]
        if phase == 1:
            saved = row
            previous = pending = None
            continue
        if phase == 2:
            previous = pending = None
            continue
        if phase == 3:
            requests[row["request"]] = {
                "request": row["request"],
                "saved_request": saved["request"] if saved else None,
                "saved_vs_applied": change(saved, row) if saved else None,
                "dynamics_changes": [],
                "between_dynamics_changes": [],
                "unpaired_dynamics_entry": None,
            }
            previous = pending = None
        result = requests.get(row["request"])
        if result is None:
            continue
        if phase == 5:
            if previous is not None and previous["step"] == row["step"] + 1:
                result["between_dynamics_changes"].append(change(previous, row))
            pending = row
            result["unpaired_dynamics_entry"] = row["step"]
        elif phase == 6 and pending is not None and pending["step"] == row["step"]:
            result["dynamics_changes"].append(change(pending, row))
            result["unpaired_dynamics_entry"] = None
            previous, pending = row, None
    return {
        "record_count": len(records),
        "phase_counts": dict(Counter(row["phase"] for row in records)),
        "restores": list(requests.values()),
        "interpretation": "State deltas locate divergence; they do not identify its cause or prove stability.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    records = [json.loads(line) for line in args.trace.read_text().splitlines() if line.strip()]
    text = json.dumps(analyze(records), indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.write_text(text)
    else:
        print(text, end="")
