import ctypes as C
import subprocess
import sys
import unittest
from urllib.error import URLError

from physical_reset_hotkeys import HotkeyCommands
from run_physical_reset_host_25661166 import (
    IPC,
    context_attempt,
    final_acknowledgement,
    shutdown_failure,
    process_exit_code,
    k,
)


class TransportTests(unittest.TestCase):
    def test_process_exit_is_distinct_from_hook_failure(self):
        message = shutdown_failure(None, 0xC0000005, 2, {"OuterUpdate": None})
        self.assertIn("LMU process exited", message)
        self.assertIn("C0000005", message)
        self.assertNotIn("quarantine", message)
        self.assertIn("00000000", shutdown_failure(None, 0, 2, {}))
        self.assertIsNone(shutdown_failure(None, None, 3, {"OuterUpdate": True}))
        self.assertIn("hook shutdown", shutdown_failure(None, None, 2, {"OuterUpdate": False}))
        self.assertEqual(shutdown_failure("previous", None, 3, {"OuterUpdate": True}), "previous")

    def test_current_process_is_active(self):
        k.GetCurrentProcess.restype = C.c_void_p
        self.assertIsNone(process_exit_code(k.GetCurrentProcess()))

    def test_terminated_child_retains_its_exit_code(self):
        with subprocess.Popen(
            [sys.executable, "-c", "raise SystemExit(23)"],
            creationflags=subprocess.CREATE_NO_WINDOW,
        ) as child:
            child.wait(timeout=5)
            self.assertEqual(process_exit_code(int(child._handle)), 23)

    def test_timeout_at_each_http_stage_revokes_and_can_retry(self):
        for stage in range(3):
            for error in (TimeoutError(), URLError("connection lost"), ConnectionError()):
                ipc = IPC()
                ipc.allow = ipc.permit_writes = 1
                keys = HotkeyCommands()
                keys.pending = (2, 2, 0)
                keys.armed = True
                reasons = {}
                responses = iter(({"state": 1}, {"game": 2}, {"state": 3}))
                calls = []

                def fetch(path):
                    calls.append(path)
                    if len(calls) == stage + 1:
                        raise error
                    return next(responses)

                self.assertIsNone(context_attempt(ipc, keys, reasons, fetch, lambda: 42))
                self.assertEqual((ipc.allow, ipc.permit_writes, ipc.heartbeat_ms), (0, 0, 42))
                self.assertIsNone(keys.pending)
                self.assertFalse(keys.armed)
                self.assertEqual(reasons["context_transport_retry"], 1)
                fresh = iter(({"state": 1}, {"game": 2}, {"state": 3}))
                self.assertEqual(
                    context_attempt(ipc, keys, reasons, lambda _: next(fresh)), (1, {"game": 2}, 3)
                )
                self.assertEqual(ipc.allow, 0)  # Assessment, not transport, grants a lease.

    def test_nontransport_error_is_not_hidden(self):
        with self.assertRaises(ValueError):
            context_attempt(
                IPC(), None, {}, lambda _: (_ for _ in ()).throw(ValueError("bad data"))
            )

    def test_final_recovery_report_after_missed_poll(self):
        ipc = IPC()
        ipc.report_sequence = 18
        ipc.completed_id = 9
        ipc.report.action = 3
        ipc.report.result = 5
        ipc.report.forward_writes = ipc.report.recovery_writes = 10077
        ipc.physical_field_write_attempts = 20154
        row = final_acknowledgement(C.addressof(ipc), ipc, 8)
        self.assertEqual(row["id"], 9)
        self.assertEqual(row["result"], 5)
        self.assertEqual(row["physical_field_write_attempts"], 20154)
        self.assertIsNone(final_acknowledgement(C.addressof(ipc), ipc, 9))
        ipc.report_sequence = 19
        self.assertIsNone(final_acknowledgement(C.addressof(ipc), ipc, 8))


if __name__ == "__main__":
    unittest.main()
