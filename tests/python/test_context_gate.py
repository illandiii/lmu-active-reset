import copy
import unittest
from sector.telemetry.context_gate import assess


class GateTests(unittest.TestCase):
    def setUp(self):
        self.nav = dict(
            appBuild=14130,
            settingMode="SETTING_GRANDPRIX",
            gameSession="PRACTICE1",
            gameState="GSTATE_DYN",
            navigationState="NAV_REALTIME",
            gamePhase="GREEN",
        )
        self.game = dict(
            inMonitor=False,
            isReplayActive=False,
            inControlOfVehicle=True,
            playerVehicleLoaded=True,
            MultiStintState="DRIVING",
        )
        self.native = dict(
            host_tick_ms=9900,
            game_version=14130,
            session=1,
            game_mode=0,
            bounds_ok=True,
            fresh_observed=True,
            player_found=True,
            telemetry_matched=True,
            in_realtime=True,
            control=0,
            vehicles=1,
            active_vehicles=1,
            phase=5,
        )

    def run_gate(self, before=None, now=10000, start=9800):
        r = assess(
            before if before is not None else self.nav, self.nav, self.game, self.native, now, start
        )
        self.assertFalse(r["reset_permitted"])
        return r

    def test_verified_observation(self):
        self.nav["navigationState"] = "NAV_REALTIME"
        self.assertEqual(self.run_gate()["context_decision"], "allowed")

    def test_unknown_navigation(self):
        self.nav["navigationState"] = "NAV_UNRECOGNIZED"
        self.assertEqual(self.run_gate()["context_decision"], "unknown")

    def test_driving_with_monitor_navigation(self):
        self.nav["navigationState"] = "NAV_EVENT"
        self.assertEqual(self.run_gate()["context_decision"], "unknown")

    def test_network_heuristics_alone_never_allow(self):
        del self.nav["settingMode"]
        self.assertEqual(self.run_gate()["context_decision"], "unknown")

    def test_multiplayer(self):
        self.nav["settingMode"] = "SETTING_MULTIPLAYER"
        self.assertEqual(self.run_gate()["context_decision"], "forbidden")

    def test_replay(self):
        self.game["isReplayActive"] = True
        self.assertEqual(self.run_gate()["context_decision"], "forbidden")

    def test_unsupported_build(self):
        self.nav["appBuild"] = 14131
        self.assertEqual(self.run_gate()["context_decision"], "unknown")

    def test_new_telemetry_profile_does_not_enable_old_physics_tools(self):
        self.nav["appBuild"] = 14150
        self.native["game_version"] = 14150
        self.assertEqual(self.run_gate()["context_decision"], "unknown")
        r = assess(self.nav, self.nav, self.game, self.native, 10000, 9800, observation_build=14150)
        self.assertEqual(r["context_decision"], "allowed")
        self.assertFalse(r["reset_permitted"])
        self.native["game_version"] = 14130
        self.assertEqual(
            assess(
                self.nav, self.nav, self.game, self.native, 10000, 9800, observation_build=14150
            )["context_decision"],
            "unknown",
        )

    def test_arbitrary_observation_profile_refused(self):
        for build in (14151, True, "14150", None):
            self.assertEqual(
                assess(
                    self.nav, self.nav, self.game, self.native, 10000, 9800, observation_build=build
                )["context_decision"],
                "unknown",
            )

    def test_14200_is_explicit_observation_only(self):
        self.nav["appBuild"] = 14200
        self.native["game_version"] = 14200
        for old in (14130, 14150):
            self.assertEqual(
                assess(
                    self.nav, self.nav, self.game, self.native, 10000, 9800, observation_build=old
                )["context_decision"],
                "unknown",
            )
        r = assess(self.nav, self.nav, self.game, self.native, 10000, 9800, observation_build=14200)
        self.assertEqual(r["context_decision"], "allowed")
        self.assertFalse(r["reset_permitted"])
        self.native["game_version"] = 14150
        self.assertEqual(
            assess(
                self.nav, self.nav, self.game, self.native, 10000, 9800, observation_build=14200
            )["context_decision"],
            "unknown",
        )

    def test_transition_between_reads(self):
        before = copy.deepcopy(self.nav)
        before["gameSession"] = "PRACTICE2"
        self.assertEqual(self.run_gate(before)["context_decision"], "unknown")

    def test_other_sessions(self):
        for session in ("TESTDAY", "WARMUP", "QUALIFY1", "RACE1", "FUTURE_MODE"):
            with self.subTest(session=session):
                self.nav["gameSession"] = session
                self.assertNotEqual(self.run_gate()["context_decision"], "allowed")

    def test_native_session_conflict(self):
        self.native["session"] = 2
        self.assertEqual(self.run_gate()["context_decision"], "unknown")

    def test_stale_sample(self):
        self.native["host_tick_ms"] = 9000
        self.assertEqual(self.run_gate()["context_decision"], "unknown")

    def test_future_sample(self):
        self.native["host_tick_ms"] = 10001
        self.assertEqual(self.run_gate()["context_decision"], "unknown")

    def test_slow_rest(self):
        self.assertEqual(self.run_gate(start=9000)["context_decision"], "unknown")

    def test_pause(self):
        self.game["MultiStintState"] = "PAUSED"
        self.assertNotEqual(self.run_gate()["context_decision"], "allowed")

    def test_not_advancing(self):
        self.native["fresh_observed"] = False
        self.assertEqual(self.run_gate()["context_decision"], "unknown")

    def test_monitor(self):
        self.game["inMonitor"] = True
        self.assertEqual(self.run_gate()["context_decision"], "forbidden")

    def test_no_local_control(self):
        self.native["control"] = 1
        self.assertEqual(self.run_gate()["context_decision"], "forbidden")

    def test_traffic(self):
        self.native["vehicles"] = 2
        self.assertEqual(self.run_gate()["context_decision"], "forbidden")

    def test_network_conflict(self):
        self.native["game_mode"] = 2
        self.assertEqual(self.run_gate()["context_decision"], "forbidden")

    def test_bool_string_rejected(self):
        self.game["isReplayActive"] = "false"
        self.assertEqual(self.run_gate()["context_decision"], "unknown")

    def test_native_numeric_types_rejected(self):
        for name in (
            "game_version",
            "session",
            "game_mode",
            "phase",
            "control",
            "vehicles",
            "active_vehicles",
        ):
            original = self.native[name]
            for value in (True, False, float(original), str(original), None):
                with self.subTest(field=name, value=value):
                    self.native[name] = value
                    self.assertEqual(self.run_gate()["context_decision"], "unknown")
            self.native[name] = original

    def test_missing_source(self):
        r = assess(self.nav, None, self.game, self.native, 10000, 9800)
        self.assertEqual(r["context_decision"], "unknown")
        self.assertFalse(r["reset_permitted"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
