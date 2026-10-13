import unittest
from physical_reset_hotkeys import HotkeyCommands, function_key


class HotkeyTests(unittest.TestCase):
    def setUp(self):
        self.keys = HotkeyCommands()

    def poll(
        self,
        save=False,
        restore=False,
        focused=True,
        allowed=True,
        modifiers=False,
        now=0,
        report=None,
    ):
        return self.keys.poll(focused, allowed, save, restore, modifiers, now, report)

    def saved(self):
        self.poll()
        self.assertEqual(self.poll(save=True), {"id": 1, "action": 1})
        self.poll(report={"id": 1, "result": 0})

    def test_acknowledged_sequence(self):
        self.saved()
        self.assertEqual(self.poll(restore=True), {"id": 2, "action": 2})
        self.assertEqual(self.poll(restore=True), {})
        self.assertEqual(
            self.poll(restore=True, report={"id": 2, "result": 0, "fields": 10077}),
            {"id": 3, "action": 3},
        )
        self.poll(restore=True, report={"id": 3, "result": 0})
        self.assertEqual(self.poll(restore=True), {})

    def test_no_checkpoint_or_unarmed_key(self):
        self.assertEqual(self.poll(save=True), {})
        self.poll()
        self.assertEqual(self.poll(restore=True), {})

    def test_focus_context_and_timeout_cancel_prepare(self):
        for kwargs in ({"focused": False}, {"allowed": False}, {"modifiers": True}, {"now": 3}):
            self.keys = HotkeyCommands()
            self.saved()
            self.poll(restore=True)
            self.assertEqual(
                self.poll(report={"id": 2, "result": 0, "fields": 10077}, **kwargs), {}
            )
            self.assertEqual(self.poll(report={"id": 2, "result": 0, "fields": 10077}), {})

    def test_failed_or_unrelated_preparation_never_restores(self):
        for report in (
            {"id": 2, "result": 2, "fields": 10077},
            {"id": 2, "result": 0, "fields": 0},
            {"id": 99, "result": 0, "fields": 10077},
        ):
            self.keys = HotkeyCommands()
            self.saved()
            self.poll(restore=True)
            self.assertEqual(self.poll(report=report), {})

    def test_chords_require_clean_release(self):
        self.poll()
        self.assertEqual(self.poll(save=True, restore=True), {})
        self.assertEqual(self.poll(save=True), {})
        self.poll()
        self.assertEqual(self.poll(save=True)["action"], 1)

    def test_key_configuration(self):
        self.assertEqual(function_key("f6"), 0x75)
        for key in ("A", "F0", "F25"):
            with self.assertRaises(ValueError):
                function_key(key)


if __name__ == "__main__":
    unittest.main()
