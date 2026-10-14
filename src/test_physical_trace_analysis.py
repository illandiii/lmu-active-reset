import unittest

from analyze_physical_reset_trace import analyze, rigid_transform_fit


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


if __name__ == "__main__":
    unittest.main()
