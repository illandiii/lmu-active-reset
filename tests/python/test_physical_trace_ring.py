import ctypes as C
import io
import json
import unittest
from unittest.mock import patch

from sector.analysis.analyze_physical_reset_trace import analyze
from sector.runtime.physical_trace import RecordV12, TraceReader


class RingMemory:
    def __init__(self):
        self.buffer = C.create_string_buffer(704520)
        self.reader = TraceReader.__new__(TraceReader)
        self.reader.view = C.addressof(self.buffer)
        self.reader.record = RecordV12
        self.reader.payload_end = 700424
        self.reader.ring = True
        self.reader.count = 0
        self.reader.output = io.StringIO()

    def publish(self, index):
        slot = index % 512
        row = RecordV12()
        row.request = index + 1
        row.phase = 11
        row.collision_body[17] = -(index + 1)
        stamp = C.c_int64.from_address(self.reader.view + 700424 + slot * 8)
        stamp.value = -(index + 1)
        C.memmove(self.reader.view + 8 + slot * 1368, C.addressof(row), 1368)
        stamp.value = index + 1
        C.c_int64.from_address(self.reader.view).value = index + 1

    def rows(self):
        return [json.loads(line) for line in self.reader.output.getvalue().splitlines()]


class RingTraceTests(unittest.TestCase):
    def test_legacy_reader_still_uses_append_only_layout(self):
        memory = RingMemory()
        memory.reader.ring = False
        for i in range(512):
            memory.publish(i)
        memory.reader.drain()
        rows = memory.rows()
        self.assertEqual([r["request"] for r in rows], list(range(1, 513)))
        self.assertTrue(all("sequence" not in row for row in rows))
        memory.reader.drain()
        self.assertEqual(len(memory.rows()), 512)

    def test_draining_across_multiple_wraps_preserves_every_record(self):
        memory = RingMemory()
        for i in range(1555):
            memory.publish(i)
            if i % 37 == 0:
                memory.reader.drain()
        memory.reader.drain()
        rows = memory.rows()
        self.assertEqual([r["sequence"] for r in rows], list(range(1, 1556)))
        self.assertTrue(all(r["collision_body"][17] == -r["sequence"] for r in rows))

    def test_slow_reader_reports_exact_loss_and_reads_latest_window(self):
        memory = RingMemory()
        for i in range(600):
            memory.publish(i)
        memory.reader.drain()
        rows = memory.rows()
        self.assertEqual(
            rows[0], dict(type="trace_gap", first_sequence=1, last_sequence=88, dropped=88)
        )
        self.assertEqual([r["sequence"] for r in rows[1:]], list(range(89, 601)))
        memory.reader.drain()
        self.assertEqual(len(memory.rows()), 513)

    def test_overwrite_during_payload_copy_is_not_published(self):
        memory = RingMemory()
        for i in range(512):
            memory.publish(i)
        original_copy = C.string_at

        def interrupted_copy(address, size):
            raw = original_copy(address, size)
            memory.publish(512)
            return raw

        with patch("sector.runtime.physical_trace.C.string_at", side_effect=interrupted_copy):
            memory.reader.drain()
        self.assertEqual(memory.rows()[0]["dropped"], 1)
        self.assertEqual([r["sequence"] for r in memory.rows()[1:]], list(range(2, 513)))
        memory.reader.drain()
        rows = memory.rows()
        self.assertEqual(rows[0]["dropped"], 1)
        self.assertEqual([r["sequence"] for r in rows[1:]], list(range(2, 514)))

    def test_crash_during_wrap_does_not_hide_other_completed_records(self):
        memory = RingMemory()
        for i in range(512):
            memory.publish(i)
        # Writer starts replacing record 1 with record 513, then terminates
        # before publishing its payload or incrementing the global counter.
        C.c_int64.from_address(memory.reader.view + 700424).value = -513
        memory.reader.drain()
        rows = memory.rows()
        self.assertEqual(rows[0]["dropped"], 1)
        self.assertEqual([r["sequence"] for r in rows[1:]], list(range(2, 513)))

    def test_busy_slot_is_deferred_without_inventing_loss(self):
        memory = RingMemory()
        memory.publish(0)
        C.c_int64.from_address(memory.reader.view + 700424).value = -1
        memory.reader.drain()
        self.assertEqual(memory.rows(), [])
        C.c_int64.from_address(memory.reader.view + 700424).value = 1
        memory.reader.drain()
        self.assertEqual(memory.rows()[0]["sequence"], 1)

    def test_analysis_never_pairs_callbacks_across_a_gap(self):
        from tests.python.test_physical_trace_analysis import record

        rows = [
            record(1, 0),
            record(3, 20),
            record(11, 17),
            dict(type="trace_gap", first_sequence=4, last_sequence=10, dropped=7),
            record(12, 17, position=11),
            record(3, 20, request=9),
        ]
        result = analyze(rows)
        self.assertEqual(result["restores"][0]["collision_corrections"], [])
        self.assertIsNone(result["restores"][1]["saved_vs_applied"])
        self.assertEqual(result["trace_gaps"][0]["dropped"], 7)


if __name__ == "__main__":
    unittest.main()
