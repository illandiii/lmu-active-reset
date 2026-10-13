import unittest
from unittest.mock import patch
from probe_contacts_14200 import context


class Context14200Tests(unittest.TestCase):
    def nav(self, **overrides):
        s = dict(
            appBuild=14200,
            settingMode="SETTING_GRANDPRIX",
            gameSession="PRACTICE1",
            gameState="GSTATE_DYN",
            navigationState="NAV_REALTIME",
            gamePhase="GREEN",
        )
        s.update(overrides)
        return {"state": s}

    def test_main_menu_rejected_before_unavailable_game_endpoint(self):
        with patch(
            "probe_contacts_14200.get_json",
            return_value=self.nav(gameState="GSTATE_SETUP", navigationState="NAV_MAIN_MENU"),
        ) as get:
            with self.assertRaises(ValueError):
                context()
            get.assert_called_once_with("/navigation/state")

    def test_online_rejected_before_game_endpoint(self):
        with patch(
            "probe_contacts_14200.get_json",
            return_value=self.nav(settingMode="SETTING_MULTIPLAYER"),
        ) as get:
            with self.assertRaises(ValueError):
                context()
            self.assertEqual(get.call_count, 1)

    def test_active_offline_observation_context(self):
        game = dict(playerVehicleLoaded=True, isReplayActive=False, inMonitor=False)
        with patch("probe_contacts_14200.get_json", side_effect=[self.nav(), game]):
            self.assertEqual(context()["navigation"]["appBuild"], 14200)

    def test_replay_refused_even_if_navigation_claims_practice(self):
        with patch(
            "probe_contacts_14200.get_json",
            side_effect=[self.nav(), dict(playerVehicleLoaded=True, isReplayActive=True)],
        ):
            with self.assertRaises(ValueError):
                context()


if __name__ == "__main__":
    unittest.main()
