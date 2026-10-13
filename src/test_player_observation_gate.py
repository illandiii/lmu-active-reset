import unittest
import test_context_gate as baseline
from context_gate import assess, assess_player_observation


class PlayerObservationGateTests(unittest.TestCase):
    def setUp(self):
        baseline.GateTests.setUp(self)
        self.nav["appBuild"] = self.native["game_version"] = 14200
        self.native["vehicles"] = 20
        self.native["active_vehicles"] = 3

    def check(self):
        return assess_player_observation(self.nav, self.nav, self.game, self.native, 10000, 9800)

    def test_ai_observation_does_not_enable_writer(self):
        self.assertEqual(self.check()["context_decision"], "allowed")
        self.assertFalse(self.check()["reset_permitted"])
        self.assertEqual(
            assess(
                self.nav, self.nav, self.game, self.native, 10000, 9800, observation_build=14200
            )["context_decision"],
            "forbidden",
        )

    def test_network_still_refused(self):
        self.native["game_mode"] = 1
        self.assertEqual(self.check()["context_decision"], "forbidden")

    def test_replay_still_refused(self):
        self.game["isReplayActive"] = True
        self.assertEqual(self.check()["context_decision"], "forbidden")

    def test_stale_sample_still_refused(self):
        self.native["host_tick_ms"] = 9000
        self.assertEqual(self.check()["context_decision"], "unknown")

    def test_bad_vehicle_counts_refused(self):
        for vehicles, active in ((0, 0), (129, 3), (2, 3), (2, 0)):
            self.native["vehicles"] = vehicles
            self.native["active_vehicles"] = active
            self.assertNotEqual(self.check()["context_decision"], "allowed")


if __name__ == "__main__":
    unittest.main()
