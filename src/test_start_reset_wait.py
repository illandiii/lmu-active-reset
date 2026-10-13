import unittest
from unittest.mock import patch
from start_reset_hotkeys import wait_for_driving


class WaitTests(unittest.TestCase):
    def test_menu_monitor_then_driving(self):
        state = {"appBuild": 14200, "settingMode": "SETTING_GRANDPRIX", "gameSession": "PRACTICE1"}
        driving = {
            "inControlOfVehicle": True,
            "inMonitor": False,
            "playerVehicleLoaded": True,
            "isReplayActive": False,
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
            patch("start_reset_hotkeys.get_json", side_effect=replies) as read,
            patch("start_reset_hotkeys.time.sleep") as sleep,
        ):
            wait_for_driving()
        self.assertEqual(read.call_count, 6)
        self.assertEqual(sleep.call_count, 2)

    def test_wait_can_be_interrupted(self):
        with (
            patch("start_reset_hotkeys.get_json", side_effect=TimeoutError),
            patch("start_reset_hotkeys.time.sleep", side_effect=KeyboardInterrupt),
        ):
            with self.assertRaises(KeyboardInterrupt):
                wait_for_driving()


if __name__ == "__main__":
    unittest.main()
