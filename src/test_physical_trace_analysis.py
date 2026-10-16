import unittest
import ctypes as C
import struct
import json

from analyze_physical_reset_trace import analyze, collision_state, rigid_transform_fit
from physical_trace import Record, RecordV12, record_dict


def record(phase, step, position=0, velocity=20, request=3, valid=511):
    body = [position, 0, 0, velocity, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0]
    return dict(
        phase=phase,
        request=request,
        tick=100,
        step=step,
        valid=valid,
        bodies=[body[:] for _ in range(5)],
    )


class TraceAnalysisTests(unittest.TestCase):
    def test_proxy_disagreement_uses_exported_world_velocity(self):
        row = record(11, 17, valid=(1 << 14) - 1)
        row["collision_body"] = row["bodies"][0][:]
        row["collision_body"][:3] = [4, 3, 0]
        row["exported_vehicle"] = [1, 0, 0, 0, 1, 0, 0, 0, 1] + [0] * 15
        row["exported_vehicle"][15:18] = [20, 0, 0]
        row["exported_vehicle"][18:21] = [900, 0, 0]  # local velocity is a different field
        state = collision_state(row)
        self.assertEqual(state["proxy_vs_exported"]["position_m"], 5)
        self.assertEqual(state["proxy_vs_exported"]["velocity_ms"], 0)
        row["valid"] &= ~(1 << 10)
        self.assertIsNone(collision_state(row)["proxy_vs_exported"])

    def test_crash_inside_correction_keeps_unpaired_entry_and_nonfinite_evidence(self):
        rows = [record(3, 20), record(11, 17), record(5, 17)]
        rows[-1]["bodies"][0][0] = float("nan")
        result = analyze(rows)
        self.assertEqual(result["restores"][0]["unpaired_collision_entries"], {11: 17})
        self.assertEqual(result["nonfinite_body_records"][0]["body"], 0)
        json.dumps(result, allow_nan=False)

    def test_native_layout_and_legacy_decoder(self):
        self.assertEqual(C.sizeof(Record), 952)
        raw = bytearray(1368)
        struct.pack_into("Q", raw, 0, 11)
        struct.pack_into("Q", raw, 952, 1)
        struct.pack_into("d", raw, 960, 314)
        struct.pack_into("f", raw, 1104 + 9 * 4, 271)
        struct.pack_into("d", raw, 1200, 42)
        struct.pack_into("d", raw, 1320, 17)
        decoded = record_dict(RecordV12.from_buffer_copy(raw))
        self.assertEqual(decoded["phase"], 11)
        self.assertEqual(decoded["collision_flags"], 1)
        self.assertEqual(decoded["collision_body"][0], 314)
        self.assertEqual(decoded["exported_vehicle"][9], 271)
        self.assertEqual(decoded["hdv_pose"][0], 42)
        self.assertEqual(decoded["collision_feedback"][0], 17)
        self.assertNotIn("collision_body", record_dict(Record.from_buffer_copy(raw[:952])))

    def test_correction_is_attributed_to_its_own_callback(self):
        rows = [
            record(3, 20),
            record(11, 17),
            record(12, 17, position=11),
            record(13, 16, position=11),
            record(14, 16, position=11),
        ]
        result = analyze(rows)["restores"][0]
        self.assertEqual(result["collision_corrections"][0]["bodies"][0]["position_m"], 11)
        self.assertEqual(result["collision_preparations"][0]["bodies"][0]["position_m"], 0)

    def test_common_rotation_is_distinct_from_an_independent_impulse(self):
        before = record(6, 17)
        after = record(5, 16)
        for i, body in enumerate(before["bodies"]):
            body[0] = i
            body[15:18] = [1, 2, 3]
            after["bodies"][i] = [
                10,
                i,
                0,
                0,
                20,
                0,
                0,
                -1,
                0,
                1,
                0,
                0,
                0,
                0,
                1,
                -2,
                1,
                3,
            ]
        fit = rigid_transform_fit(before, after)
        self.assertEqual(fit["translation"], [10, 0, 0])
        self.assertTrue(all(all(value == 0 for value in row.values()) for row in fit["residuals"]))
        after["bodies"][4][4] += 5
        self.assertEqual(rigid_transform_fit(before, after)["residuals"][4]["velocity_ms"], 5)

    def test_saved_velocity_and_later_external_change_are_separate(self):
        rows = [
            record(1, 0, request=1),
            record(2, 0, position=200, velocity=50),
            record(3, 20),
            record(5, 20),
            record(6, 20, position=0.05),
            record(5, 19, position=11, velocity=25),
            record(6, 19, position=11.1, velocity=90),
            record(5, 18, position=11.1, velocity=90),
        ]
        result = analyze(rows)["restores"][0]
        self.assertEqual(result["saved_vs_applied"]["bodies"][0]["velocity_ms"], 0)
        self.assertEqual(result["dynamics_changes"][1]["bodies"][0]["velocity_ms"], 65)
        self.assertAlmostEqual(
            result["between_dynamics_changes"][0]["bodies"][0]["position_m"], 10.95
        )
        self.assertEqual(result["unpaired_dynamics_entry"], 18)

    def test_unavailable_body_is_not_treated_as_zero_change(self):
        rows = [record(1, 0, request=1), record(3, 20, valid=510)]
        self.assertIsNone(analyze(rows)["restores"][0]["saved_vs_applied"]["bodies"][0])

    def test_incomplete_trace_does_not_invent_a_save_or_pair(self):
        result = analyze([record(3, 20), record(5, 20), record(6, 19)])["restores"][0]
        self.assertIsNone(result["saved_vs_applied"])
        self.assertEqual(result["dynamics_changes"], [])
        self.assertEqual(result["unpaired_dynamics_entry"], 20)

    def test_collision_rebuild_precedes_applied_record(self):
        rows = [record(2, 0), record(15, 0), record(16, 0), record(3, 20)]
        rebuilds = analyze(rows)["restores"][0]["collision_rebuilds"]
        self.assertEqual(len(rebuilds), 1)
        self.assertEqual(rebuilds[0]["bodies"][0]["position_m"], 0)
        rows.insert(2, {"type": "trace_gap"})
        self.assertEqual(analyze(rows)["restores"][0]["collision_rebuilds"], [])


if __name__ == "__main__":
    unittest.main()
