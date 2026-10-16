"""Summarize captured state changes without inferring their native cause."""

import argparse
from collections import Counter
import json
import math
from pathlib import Path


def finite(values):
    return all(math.isfinite(value) for value in values)


def collision_state(row):
    """Decode only readable, finite groups; missing data is never a zero delta."""
    fields = {
        "collision_body": (9, 18),
        "exported_vehicle": (10, 24),
        "hdv_pose": (12, 15),
        "collision_feedback": (13, 6),
    }
    state = {"valid": row["valid"]}
    for name, (bit, count) in fields.items():
        values = row.get(name)
        state[name] = (
            values
            if row["valid"] & (1 << bit)
            and isinstance(values, list)
            and len(values) == count
            and finite(values)
            else None
        )
    state["collision_flags"] = row.get("collision_flags") if row["valid"] & (1 << 11) else None
    proxy, exported = state["collision_body"], state["exported_vehicle"]
    state["proxy_vs_exported"] = None
    if proxy is not None and exported is not None:
        state["proxy_vs_exported"] = {
            "position_delta_m": [a - b for a, b in zip(proxy[:3], exported[9:12])],
            "position_m": math.dist(proxy[:3], exported[9:12]),
            "velocity_ms": math.dist(proxy[3:6], exported[15:18]),
            "orientation_max_abs": max(abs(a - b) for a, b in zip(proxy[6:15], exported[:9])),
        }
    return state


def body_delta(before, after):
    return {
        "position_m": math.dist(before[:3], after[:3]),
        "velocity_ms": math.dist(before[3:6], after[3:6]),
        "orientation_max_abs": max(abs(a - b) for a, b in zip(before[6:15], after[6:15])),
        "angular_momentum": math.dist(before[15:18], after[15:18]),
    }


def rigid_transform_fit(before, after):
    """Fit the root pose, then independently check all five bodies and vectors."""
    if before["valid"] & after["valid"] & 31 != 31 or any(
        not finite(body) for row in (before, after) for body in row["bodies"]
    ):
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
        bodies.append(body_delta(a, b) if valid and finite(a) and finite(b) else None)
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
    restore_before = None
    hook_entries = {}
    requests = {}
    for row in records:
        if row.get("type") == "trace_gap":
            # No before/after pair or checkpoint comparison may bridge lost data.
            saved = previous = pending = restore_before = None
            hook_entries = {}
            continue
        if len(row["bodies"]) != 5 or any(len(body) != 18 for body in row["bodies"]):
            raise ValueError("Expected five 18-component body records")
        phase = row["phase"]
        if phase == 1:
            saved = row
            previous = pending = None
            restore_before = None
            continue
        if phase == 2:
            previous = pending = None
            restore_before = row
            continue
        if phase == 3:
            requests[row["request"]] = {
                "request": row["request"],
                "saved_request": saved["request"] if saved else None,
                "saved_vs_applied": change(saved, row) if saved else None,
                "dynamics_changes": [],
                "between_dynamics_changes": [],
                "unpaired_dynamics_entry": None,
                "collision_corrections": [],
                "collision_preparations": [],
                "saved_collision_state": collision_state(saved) if saved else None,
                "before_restore_collision_state": (
                    collision_state(restore_before)
                    if restore_before and restore_before["request"] == row["request"]
                    else None
                ),
                "applied_collision_state": collision_state(row),
                "unpaired_collision_entries": {},
            }
            previous = pending = None
            hook_entries = {}
        result = requests.get(row["request"])
        if result is None:
            continue
        if phase in (11, 13):
            hook_entries[phase] = row
            result["unpaired_collision_entries"][phase] = row["step"]
        elif phase in (12, 14) and phase - 1 in hook_entries:
            before = hook_entries.pop(phase - 1)
            result["unpaired_collision_entries"].pop(phase - 1)
            entry = change(before, row)
            for side, sample in (("before", before), ("after", row)):
                entry[side] = collision_state(sample)
            key = "collision_corrections" if phase == 12 else "collision_preparations"
            result[key].append(entry)
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
        "phase_counts": dict(Counter(row["phase"] for row in records if "phase" in row)),
        "trace_gaps": [row for row in records if row.get("type") == "trace_gap"],
        "nonfinite_body_records": [
            {"index": i, "phase": row["phase"], "step": row["step"], "body": body_index}
            for i, row in enumerate(records)
            for body_index, body in enumerate(row.get("bodies", []))
            if not finite(body)
        ],
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
