"""Offline cross-checks of every forward checkpoint pair and representative full plans."""

from sector.paths import PROJECT_ROOT

import argparse
import ctypes as C
import hashlib
import json
import subprocess
import sys
from pathlib import Path
from sector.replay.test_shift_transient_recording import Bank, ShiftObserved, state, make_shift_bank


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("frames", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    a = parser.parse_args()
    rows = [json.loads(x) for x in a.frames.read_text().splitlines() if x]
    assert rows
    a.output.mkdir(parents=True, exist_ok=False)
    dll_path = PROJECT_ROOT / "work/checkpoint-bank-fixture.dll"
    dll = C.CDLL(str(dll_path))
    prepare = dll.fixture_shift_prepare
    prepare.argtypes = [
        C.POINTER(Bank),
        C.c_uint64,
        C.POINTER(ShiftObserved),
        C.POINTER(ShiftObserved),
    ]
    prepare.restype = C.c_int
    states = [state(r) for r in rows]
    pairs = backward = 0
    max_age_error = 0.0
    out = ShiftObserved()
    for i, saved in enumerate(states):
        bank = make_shift_bank(dll, 42, rows[i])
        saved_bytes = bytes(bank)
        age = saved.clock_a8 - saved.event_50
        for j, current in enumerate(states):
            before = bytes(out)
            result = prepare(C.byref(bank), 42, C.byref(current), C.byref(out))
            if j < i:
                assert result == 0 and bytes(out) == before
                backward += 1
                continue
            assert result == 1 and bytes(bank) == saved_bytes
            error = abs((out.clock_a8 - out.event_50) - age)
            max_age_error = max(max_age_error, error)
            assert error < 1e-8
            expected = ShiftObserved.from_buffer_copy(bytes(saved))
            expected.clock_a8 = current.clock_a8
            expected.event_50 = current.clock_a8 - age
            assert bytes(out) == bytes(expected)
            pairs += 1
    selected = {}
    for gear in sorted({r["shift_observed"]["gears"][1] for r in rows}):
        selected["gear_" + str(gear)] = next(
            i for i, r in enumerate(rows) if r["shift_observed"]["gears"][1] == gear
        )
    selected["minimum_timer"] = min(
        range(len(rows)), key=lambda i: rows[i]["shift_observed"]["timer_4ac0"]
    )
    selected["minimum_accumulator"] = min(
        range(len(rows)), key=lambda i: rows[i]["shift_observed"]["accumulators"][0]
    )
    selected["maximum_accumulator"] = max(
        range(len(rows)), key=lambda i: rows[i]["shift_observed"]["accumulators"][0]
    )
    scenarios = []
    for index in sorted(set(selected.values())):
        path = a.output / f"checkpoint_{index:03d}.frames.jsonl"
        subset = [rows[index]] if index == len(rows) - 1 else [rows[index], rows[-1]]
        path.write_text("".join(json.dumps(r) + "\n" for r in subset))
        process = subprocess.run(
            [
                sys.executable,
                str(Path(__file__).with_name("test_vehicle_partial_checkpoint.py")),
                str(path),
                "--verify-plan",
            ],
            capture_output=True,
            text=True,
        )
        if process.returncode:
            raise RuntimeError(process.stdout + "\n" + process.stderr)
        result = json.loads(process.stdout)
        plan = result["mapped_field_plan"]
        assert (
            result["first_to_current_preparations"] == len(subset)
            and not result["rejected_incomplete_tire_mass_dependency"]
        )
        assert (
            plan["fields"] == 10074
            and plan["bytes"] == 255090
            and plan["joint_physics_scoring"]["fields"] == 10077
        )
        assert (
            plan["native_self_memory_full_plan"]
            and plan["native_context_loss_rollback"]
            and plan["joint_physics_scoring"]["all_three_scoring_failures_rollback_physics"]
        )
        scenarios.append(
            dict(
                index=index,
                cases=[name for name, value in selected.items() if value == index],
                physical_fields=plan["fields"],
                joint_fields=plan["joint_physics_scoring"]["fields"],
                local_execution_and_rollback=True,
            )
        )
        print(
            json.dumps({"checkpoint_verified": index, "cases": scenarios[-1]["cases"]}), flush=True
        )
    rotation_error = 0.0
    for row in rows:
        for body in row["bodies"]:
            r = body["orientation"]
            for i in range(3):
                for j in range(3):
                    rotation_error = max(
                        rotation_error,
                        abs(sum(r[3 * i + k] * r[3 * j + k] for k in range(3)) - (i == j)),
                    )
    report = dict(
        source_sha256=hashlib.sha256(a.frames.read_bytes()).hexdigest(),
        fixture_sha256=hashlib.sha256(dll_path.read_bytes()).hexdigest(),
        frames=len(rows),
        forward_checkpoint_pairs=pairs,
        backward_time_rejections=backward,
        max_request_age_error_s=max_age_error,
        max_rotation_orthogonality_error=rotation_error,
        max_native_inertia_error=max(r["inertia_error"] for r in rows),
        max_native_omega_error=max(r["omega_error"] for r in rows),
        representative_plans=scenarios,
        physics_writes=0,
        full_live_reset_proven=False,
        limitations=[
            "Synthetic replay identity; session/setup identity incomplete.",
            "All writes target fixture-owned memory, never the game.",
            "No post-teleport contact recovery or real scoring/record preservation test.",
            "Samples do not cover every substep of a gear shift.",
        ],
    )
    (a.output / "summary.json").write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
