import unittest
from sector.runtime.physical_reset_hotkeys import (
    HotkeyCommands,
    expected_restore_fields,
    virtual_key,
    preparation_ready,
)


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
            self.poll(restore=True, report={"id": 2, "action": 2, "result": 0, "fields": 10084}),
            {"id": 3, "action": 3},
        )
        self.poll(restore=True, report={"id": 3, "result": 0})
        self.assertEqual(self.poll(restore=True), {})

    def test_no_checkpoint_or_unarmed_key(self):
        self.assertEqual(self.poll(save=True), {})
        self.poll()
        self.assertEqual(self.poll(restore=True), {})

    def test_native_revision_controls_both_preparation_gates(self):
        # The live v15 incident acknowledged PREPARE with 10084 fields, but the
        # two launcher gates still expected 10077 and never sent RESTORE.
        for revision, count in ((14, 10077), (15, 10084)):
            with self.subTest(revision=revision):
                expected = expected_restore_fields(revision)
                self.assertEqual(expected, count)
                self.keys = HotkeyCommands(expected)
                self.saved()
                self.poll(restore=True)
                report = {"id": 2, "action": 2, "result": 0, "fields": count}
                self.assertTrue(preparation_ready(report, expected))
                self.assertEqual(self.poll(report=report), {"id": 3, "action": 3})
                self.assertEqual(self.poll(report=report), {})

    def test_wrong_plan_size_is_rejected_with_a_reason(self):
        self.saved()
        self.poll(restore=True)
        report = {"id": 2, "action": 2, "result": 0, "fields": 10077}
        self.assertFalse(preparation_ready(report, expected_restore_fields(15)))
        self.assertEqual(self.poll(report=report), {})
        self.assertIn("expected 10084", self.keys.notice)
        self.assertTrue(self.keys.saved)
        for revision in (0, 16):
            with self.assertRaises(ValueError):
                expected_restore_fields(revision)

    def test_focus_context_and_timeout_cancel_prepare(self):
        for kwargs in ({"focused": False}, {"allowed": False}, {"modifiers": True}, {"now": 3}):
            self.keys = HotkeyCommands()
            self.saved()
            self.poll(restore=True)
            self.assertEqual(
                self.poll(report={"id": 2, "action": 2, "result": 0, "fields": 10084}, **kwargs), {}
            )
            self.assertEqual(
                self.poll(report={"id": 2, "action": 2, "result": 0, "fields": 10084}), {}
            )

    def test_failed_or_unrelated_preparation_never_restores(self):
        for report in (
            {"id": 2, "action": 2, "result": 2, "fields": 10084},
            {"id": 2, "action": 2, "result": 0, "fields": 0},
            {"id": 99, "action": 2, "result": 0, "fields": 10084},
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
        self.assertEqual(virtual_key("f6"), 0x75)
        self.assertEqual(virtual_key("g"), 0x47)
        self.assertEqual(virtual_key("Space"), 0x20)
        self.assertEqual(virtual_key("D8"), 0x38)
        self.assertEqual(virtual_key("NumPad8"), 0x68)
        for key in ("Shift", "Escape", "Ctrl+F6", "F0", "F25", None):
            with self.assertRaises(ValueError):
                virtual_key(key)


if __name__ == "__main__":
    unittest.main()
