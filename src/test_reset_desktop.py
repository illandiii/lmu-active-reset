import json
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch
import wave

from physical_reset_hotkeys import HotkeyCommands
from reset_desktop_session import DesktopSession
from reset_feedback import Feedback, report_feedback, write_sounds
from start_reset_hotkeys import supervise


class DesktopTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)
        self.session = DesktopSession(self.path, clock=lambda: 1000)
        (self.path / "heartbeat").touch()
        os.utime(self.path / "heartbeat", (1000, 1000))

    def test_checkpoint_does_not_expire_with_healthy_gui(self):
        self.session.report({"action": 1, "result": 0, "id": 1})
        for now in (1000, 1000 + 301, 1000 + 86400):
            self.session.clock = lambda: now
            os.utime(self.path / "heartbeat", (now, now))
            self.assertFalse(self.session.stopped())
            self.assertTrue(self.session.state["checkpoint"])
        self.session.publish(checkpoint=False, message="Vehicle reset.")
        self.assertFalse(json.loads((self.path / "status.json").read_text())["checkpoint"])

    def test_close_and_crashed_gui_request_stop(self):
        self.assertFalse(self.session.stopped())
        self.session.clock = lambda: 1011
        self.assertTrue(self.session.stopped())
        self.session.clock = lambda: 1000
        (self.path / "stop").touch()
        self.assertTrue(self.session.stopped())

    def test_status_write_failure_cannot_interrupt_native_cleanup(self):
        with patch.object(Path, "replace", side_effect=PermissionError):
            self.session.report({"action": 1, "result": 0, "id": 1})
        self.assertTrue(self.session.state["checkpoint"])
        self.session.publish(phase="active")
        self.assertTrue(json.loads((self.path / "status.json").read_text())["checkpoint"])

    def test_checkpoint_summary_uses_acknowledgements_and_survives_failed_save(self):
        self.session.command(1)
        self.assertFalse(self.session.state["checkpoint"])
        self.session.report({"action": 1, "result": 0, "id": 1, "speed": 25.0})
        self.assertEqual(self.session.state["saved_speed_kmh"], 90.0)
        self.assertEqual(self.session.state["saved_at"], 1000)
        self.session.command(2)
        self.session.report({"action": 2, "result": 0, "id": 2})
        self.assertEqual(self.session.state["restore_count"], 0)
        self.session.report({"action": 3, "result": 0, "id": 3})
        self.session.report({"action": 1, "result": 9, "cause": 3, "id": 4})
        self.assertEqual(self.session.state["saved_speed_kmh"], 90.0)
        self.assertEqual(self.session.state["restore_count"], 1)
        self.session.publish(phase="waiting", checkpoint=False)
        self.assertIsNone(self.session.state["saved_speed_kmh"])
        self.assertIsNone(self.session.state["saved_at"])
        self.assertEqual(self.session.state["restore_count"], 0)

    def test_invalid_speed_does_not_break_status_or_cleanup(self):
        for speed in (float("nan"), float("inf"), -1, None, "fast", True):
            self.session.report({"action": 1, "result": 0, "id": 1, "speed": speed})
            snapshot = json.loads((self.path / "status.json").read_text())
            self.assertIsNone(snapshot["saved_speed_kmh"])
            self.assertTrue(snapshot["checkpoint"])

    def test_settings_validation_and_atomic_replacement(self):
        self.session.settings_file = self.path / "settings.json"
        values = {"save_key": "F8", "restore_key": "F9", "sound": False}
        self.session.settings_file.write_text(json.dumps(values))
        self.assertEqual(self.session.settings(), ("F8", "F9", False))
        for invalid in (
            dict(values, restore_key="F8"),
            dict(values, save_key="Ctrl+G"),
            dict(values, sound="false"),
            dict(values, restore_key=None),
        ):
            self.session.settings_file.write_text(json.dumps(invalid))
            self.assertIsNone(self.session.settings())
        self.session.settings_file.write_text("{")
        self.assertIsNone(self.session.settings())

    def test_rebind_retains_checkpoint_and_requires_release(self):
        keys = HotkeyCommands()
        keys.saved = True
        keys.armed = True
        self.assertTrue(keys.rebind())
        self.assertTrue(keys.saved)
        self.assertEqual(keys.poll(True, True, False, True, False, 90000), {})
        keys.poll(True, True, False, False, False, 90000)
        self.assertEqual(keys.poll(True, True, False, True, False, 90001)["action"], 2)
        self.assertFalse(keys.rebind())  # Do not change a preparation in flight.

    def test_native_save_ack_survives_focus_loss(self):
        keys = HotkeyCommands()
        keys.poll(True, True, False, False, False, 0)
        keys.poll(True, True, True, False, False, 0)
        keys.poll(False, True, False, False, False, 0.1, {"id": 1, "result": 0})
        self.assertTrue(keys.saved)

    def test_failed_save_preserves_previous_checkpoint(self):
        self.session.report({"action": 1, "result": 0, "id": 1})
        self.session.command(1)
        self.session.report({"action": 1, "result": 9, "cause": 3, "id": 2})
        self.assertTrue(self.session.state["checkpoint"])
        keys = HotkeyCommands()
        keys.saved = True
        keys.poll(True, True, False, False, False, 0)
        keys.poll(True, True, True, False, False, 0)
        keys.poll(True, True, False, False, False, 0.1, {"id": 1, "result": 9})
        self.assertTrue(keys.saved)

    def test_restore_without_checkpoint_has_feedback_once_per_press(self):
        keys = HotkeyCommands()
        keys.poll(True, True, False, False, False, 0)
        self.assertEqual(keys.poll(True, True, False, True, False, 1), {})
        self.assertIn("Save a checkpoint", keys.notice)
        keys.poll(True, True, False, True, False, 2)
        self.assertIsNone(keys.notice)

    def test_lifecycle_exit_rearms_but_errors_never_retry(self):
        args = SimpleNamespace(pid=7, desktop=self.session)
        alive = Mock(return_value=True)
        run = Mock(side_effect=["vehicle_reset", "context_changed", RuntimeError("quarantined")])
        wait = Mock(return_value=True)
        audit = Mock()
        with self.assertRaisesRegex(RuntimeError, "quarantined"):
            supervise(args, run, audit, alive, wait=wait, sleep=lambda _: None)
        self.assertEqual(run.call_count, 3)
        self.assertEqual(audit.call_count, 3)
        self.assertEqual(args.revision, 15)
        self.assertFalse(self.session.state["checkpoint"])
        self.assertFalse(hasattr(args, "seconds"))

    def test_unknown_shutdown_does_not_autorestart(self):
        args = SimpleNamespace(pid=7, desktop=self.session)
        run = Mock(return_value="stopped")
        with self.assertRaisesRegex(RuntimeError, "unexpectedly"):
            supervise(args, run, Mock(), lambda: True, wait=Mock(return_value=True))
        self.assertEqual(run.call_count, 1)

    def test_user_shutdown_does_not_error_or_autorestart(self):
        args = SimpleNamespace(pid=7, desktop=self.session)
        run = Mock(return_value="user_stopped")
        audit = Mock()
        wait = Mock(return_value=True)
        supervise(args, run, audit, lambda: True, wait=wait)
        run.assert_called_once_with(args)
        audit.assert_called_once_with(7)
        self.assertEqual(wait.call_count, 1)

    def test_stop_while_waiting_never_loads_host(self):
        (self.path / "stop").touch()
        run = Mock()
        supervise(SimpleNamespace(pid=7, desktop=self.session), run, Mock(), lambda: True)
        run.assert_not_called()

    def test_sounds_are_distinct_valid_wav_and_ack_only(self):
        paths = write_sounds(self.path / "audio")
        self.assertEqual(len({p.read_bytes() for p in paths.values()}), 3)
        for path in paths.values():
            with wave.open(str(path)) as sound:
                self.assertEqual((sound.getnchannels(), sound.getsampwidth()), (1, 2))
                self.assertLess(sound.getnframes() / sound.getframerate(), 0.5)
        self.assertIsNone(report_feedback({"action": 2, "result": 0}))
        self.assertEqual(report_feedback({"action": 1, "result": 0})[0], "save")
        self.assertEqual(report_feedback({"action": 3, "result": 0})[0], "restore")
        self.assertEqual(report_feedback({"action": 2, "result": 5, "cause": 1})[0], "rejected")

    def test_audio_is_async_and_can_be_muted_live(self):
        audio = Mock(SND_FILENAME=1, SND_ASYNC=2, SND_NODEFAULT=4)
        with patch.dict("sys.modules", {"winsound": audio}):
            feedback = Feedback(self.path / "audio", muted=True)
            feedback.play("save")
            audio.PlaySound.assert_not_called()
            feedback.muted = False
            feedback.play("restore")
            audio.PlaySound.assert_called_once_with(str(feedback.paths["restore"]), 7)
            audio.PlaySound.side_effect = RuntimeError("No device")
            feedback.play("rejected")  # Audio failure never kills the controller.


if __name__ == "__main__":
    unittest.main()
