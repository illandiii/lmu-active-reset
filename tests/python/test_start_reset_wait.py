import unittest
from unittest.mock import patch
from sector.runtime.start_reset_hotkeys import wait_for_driving


class WaitTests(unittest.TestCase):
    def test_menu_monitor_then_driving(self):
        state = {"appBuild": 14200, "settingMode": "SETTING_GRANDPRIX", "gameSession": "PRACTICE1"}
        driving = {
            "inControlOfVehicle": True,
            "inMonitor": False,
            "playerVehicleLoaded": True,
            "isReplayActive": False,
            "MultiStintState": "DRIVING",
        }
        replies = [
            {"state": {}},
            {},
            {"state": state},
            dict(driving, inMonitor=True),
            {"state": state},
            driving,
        ]
        with (
            patch("sector.runtime.start_reset_hotkeys.get_json", side_effect=replies) as read,
            patch("sector.runtime.start_reset_hotkeys.time.sleep") as sleep,
        ):
            wait_for_driving()
        self.assertEqual(read.call_count, 6)
        self.assertEqual(sleep.call_count, 2)

    def test_paused_control_waits_for_two_consecutive_driving_samples(self):
        state = {"appBuild": 14200, "settingMode": "SETTING_GRANDPRIX", "gameSession": "PRACTICE1"}
        game = dict(
            inControlOfVehicle=True, inMonitor=False, playerVehicleLoaded=True, isReplayActive=False
        )
        replies = []
        # Missing status cannot admit a host either. A pause between the two
        # required driving samples must reset the stability count.
        for phase in (None, "PAUSED", "DRIVING", "PAUSED", "DRIVING", "DRIVING"):
            replies.extend([{"state": state}, dict(game, MultiStintState=phase)])
        with (
            patch("sector.runtime.start_reset_hotkeys.get_json", side_effect=replies) as read,
            patch("sector.runtime.start_reset_hotkeys.time.sleep") as sleep,
        ):
            self.assertTrue(wait_for_driving(stable_samples=2))
        self.assertEqual(read.call_count, 12)
        self.assertEqual(sleep.call_count, 5)

    def test_wait_can_be_interrupted(self):
        with (
            patch("sector.runtime.start_reset_hotkeys.get_json", side_effect=TimeoutError),
            patch("sector.runtime.start_reset_hotkeys.time.sleep", side_effect=KeyboardInterrupt),
        ):
            with self.assertRaises(KeyboardInterrupt):
                wait_for_driving()


if __name__ == "__main__":
    unittest.main()
