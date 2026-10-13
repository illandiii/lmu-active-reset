import ctypes as C
import unittest
from urllib.error import URLError

from physical_reset_hotkeys import HotkeyCommands
from run_physical_reset_host_25661166 import IPC, context_attempt, final_acknowledgement


class TransportTests(unittest.TestCase):
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
