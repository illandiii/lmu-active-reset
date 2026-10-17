"""The v21 extension must preserve the layout of already loaded v20 hosts."""

import ctypes as C
import unittest

from sector.runtime.reset_host_runner import (
    IPC,
    LegacyIPC,
    bodywork_diagnostics,
    final_acknowledgement,
    ipc_type_for_magic,
)
from sector.runtime.physical_reset_hotkeys import rejection_reason


class BodyworkProtocolTests(unittest.TestCase):
    def test_legacy_prefix_and_resident_size(self):
        self.assertEqual(C.sizeof(LegacyIPC), 328)
        self.assertEqual(C.sizeof(IPC), 384)
        for name, _ in LegacyIPC._fields_:
            self.assertEqual(getattr(IPC, name).offset, getattr(LegacyIPC, name).offset)
        for revision in range(1, 21):
            self.assertIs(ipc_type_for_magic(0x4C4D555253543030 + revision), LegacyIPC)
        self.assertIs(ipc_type_for_magic(0x4C4D555253543030 + 21), IPC)
        self.assertEqual(bodywork_diagnostics(LegacyIPC()), {})

    def test_completed_ack_includes_graphics_diagnostics(self):
        ipc = IPC()
        ipc.magic = 0x4C4D555253543030 + 21
        ipc.report_sequence = 2
        ipc.completed_id = 1
        ipc.bodywork_meshes = 4
        ipc.bodywork_vertex_bytes = 2889128
        ipc.bodywork_attempted_writes = 50
        ipc.bodywork_verified_writes = 50
        row = final_acknowledgement(C.addressof(ipc), ipc, 0)
        self.assertEqual(row["bodywork_meshes"], 4)
        self.assertEqual(row["bodywork_vertex_bytes"], 2889128)
        self.assertEqual(row["bodywork_attempted_writes"], row["bodywork_verified_writes"])
        ipc.report_sequence = 3
        self.assertIsNone(final_acknowledgement(C.addressof(ipc), ipc, 0))

    def test_graphics_refusals_are_actionable(self):
        for cause, word in [
            (92003, "retry"),
            (92005, "buffer"),
            (92006, "repaired"),
            (92007, "checkpoint"),
            (92010, "Restart"),
        ]:
            self.assertIn(word, rejection_reason({"cause": cause}))
