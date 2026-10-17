import unittest
from sector.runtime.physical_reset_hotkeys import (
    HotkeyCommands,
    expected_restore_fields,
    virtual_key,
    preparation_ready,
)


class HotkeyTests(unittest.TestCase):
    def setUp(self):
        self.keys = HotkeyCommands(10085)

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
            self.poll(restore=True, report={"id": 2, "action": 2, "result": 0, "fields": 10085}),
            {"id": 3, "action": 3},
        )
        self.poll(restore=True, report={"id": 3, "result": 0})
        self.assertEqual(self.poll(restore=True), {})

    def test_no_checkpoint_or_unarmed_key(self):
        self.assertEqual(self.poll(save=True), {})
        self.poll()
        self.assertEqual(self.poll(restore=True), {})

    def test_vehicle_specific_native_preflight_controls_restore(self):
        # The incident prepared 10122 fields successfully; the old launcher
        # compared it to 10084 and never issued HOST_RESTORE.
        self.assertIsNone(expected_restore_fields(17))
        for count in (10084, 10122, 10085, 10123, 11000):
            self.keys = HotkeyCommands()
            self.saved()
            self.poll(restore=True)
            report = dict(
                id=2,
                action=2,
                result=0,
                fields=count,
                verified_fields=count,
                mismatched_fields=0,
                readable=1,
                epoch=1,
            )
            self.assertTrue(preparation_ready(report))
            self.assertEqual(self.poll(report=report), {"id": 3, "action": 3})
            self.assertEqual(self.poll(report=report), {})

    def test_v17_rejects_missing_partial_or_failed_native_preflight(self):
        for patch in (
            {"verified_fields": 0},
            {"verified_fields": 10122},
            {"mismatched_fields": 1},
            {"readable": 0},
            {"epoch": 0},
            {"result": 1},
            {"action": 1},
            {"fields": 16385},
            {"fields": True},
            {"id": 99},
        ):
            self.keys = HotkeyCommands()
            self.saved()
            self.poll(restore=True)
            report = dict(
                id=2,
                action=2,
                result=0,
                fields=10123,
                verified_fields=10123,
                mismatched_fields=0,
                readable=1,
                epoch=1,
            )
            report.update(patch)
            self.assertEqual(self.poll(report=report), {})

    def test_native_revision_controls_both_preparation_gates(self):
        # The live v15 incident acknowledged PREPARE with 10084 fields, but the
        # two launcher gates still expected 10077 and never sent RESTORE.
        for revision, count in ((14, 10077), (15, 10084), (16, 10085)):
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
        self.assertIn("complete native preflight required", self.keys.notice)
        self.assertTrue(self.keys.saved)
        for revision in (0, 18):
            with self.assertRaises(ValueError):
                expected_restore_fields(revision)

    def test_focus_context_and_timeout_cancel_prepare(self):
        for kwargs in ({"focused": False}, {"allowed": False}, {"modifiers": True}, {"now": 3}):
            self.keys = HotkeyCommands(10085)
            self.saved()
            self.poll(restore=True)
            self.assertEqual(
                self.poll(report={"id": 2, "action": 2, "result": 0, "fields": 10085}, **kwargs), {}
            )
            self.assertEqual(
                self.poll(report={"id": 2, "action": 2, "result": 0, "fields": 10085}), {}
            )

    def test_failed_or_unrelated_preparation_never_restores(self):
        for report in (
            {"id": 2, "action": 2, "result": 2, "fields": 10085},
            {"id": 2, "action": 2, "result": 0, "fields": 0},
            {"id": 99, "action": 2, "result": 0, "fields": 10085},
        ):
            self.keys = HotkeyCommands(10085)
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
