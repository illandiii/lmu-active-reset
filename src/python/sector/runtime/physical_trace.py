import ctypes as C
import json
from ctypes import wintypes as W


class Record(C.Structure):
    _fields_ = [(n, C.c_uint64) for n in ("phase", "request", "tick", "step", "valid")] + [
        ("bodies", (C.c_double * 18) * 5),
        ("contacts", (C.c_double * 6) * 4),
    ]


class RecordV12(C.Structure):
    _fields_ = Record._fields_ + [
        ("collision_flags", C.c_uint64),
        ("collision_body", C.c_double * 18),
        ("exported_vehicle", C.c_float * 24),
        ("hdv_pose", C.c_double * 15),
        ("collision_feedback", C.c_double * 6),
    ]


assert C.sizeof(Record) == 952 and C.sizeof(RecordV12) == 1368


def record_dict(r):
    row = {n: getattr(r, n) for n in ("phase", "request", "tick", "step", "valid")}
    row.update(bodies=[list(b) for b in r.bodies], contacts=[list(c) for c in r.contacts])
    if isinstance(r, RecordV12):
        row["collision_flags"] = r.collision_flags
        for name in ("collision_body", "exported_vehicle", "hdv_pose", "collision_feedback"):
            row[name] = list(getattr(r, name))
    return row


class TraceReader:
    def __init__(self, name, path, revision=10):
        self.record = RecordV12 if revision >= 12 else Record
        self.ring = revision >= 14
        self.payload_end = 8 + 512 * C.sizeof(self.record)
        self.k = C.WinDLL("kernel32", use_last_error=True)
        self.k.OpenFileMappingW.argtypes = [W.DWORD, W.BOOL, W.LPCWSTR]
        self.k.OpenFileMappingW.restype = W.HANDLE
        self.k.MapViewOfFile.argtypes = [W.HANDLE, W.DWORD, W.DWORD, W.DWORD, C.c_size_t]
        self.k.MapViewOfFile.restype = C.c_void_p
        self.k.UnmapViewOfFile.argtypes = [C.c_void_p]
        self.k.CloseHandle.argtypes = [W.HANDLE]
        self.handle = self.k.OpenFileMappingW(4, False, name + "_trace")
        self.view = (
            self.k.MapViewOfFile(
                self.handle, 4, 0, 0, self.payload_end + (4096 if self.ring else 0)
            )
            if self.handle
            else None
        )
        if not self.view:
            if self.handle:
                self.k.CloseHandle(self.handle)
            raise RuntimeError("Native physical trace unavailable")
        self.output = path.open("w")
        self.count = 0

    def skip_to(self, count):
        self.output.write(
            json.dumps(
                dict(
                    type="trace_gap",
                    first_sequence=self.count + 1,
                    last_sequence=count,
                    dropped=count - self.count,
                )
            )
            + "\n"
        )
        self.count = count

    def drain(self):
        count = C.c_int64.from_address(self.view).value
        if not self.ring:
            count = min(count, 512)
        oldest = max(0, count - 512)
        if self.count < oldest:
            self.skip_to(oldest)
        while self.count < count:
            slot = self.count % 512
            stamp_address = self.view + self.payload_end + slot * 8
            expected = self.count + 1
            stamp = C.c_int64.from_address(stamp_address).value if self.ring else expected
            if abs(stamp) > expected:
                # The old record is already lost, even if a crash interrupts reuse
                # before the writer can advance the global publication counter.
                self.skip_to(expected)
                continue
            if stamp != expected:
                break
            r = self.record.from_buffer_copy(
                C.string_at(self.view + 8 + slot * C.sizeof(self.record), C.sizeof(self.record))
            )
            stamp = C.c_int64.from_address(stamp_address).value if self.ring else expected
            if abs(stamp) > expected:
                self.skip_to(expected)
                continue
            if stamp != expected:
                break  # Never publish a payload overwritten during the copy.
            row = record_dict(r)
            if self.ring:
                row["sequence"] = expected
            self.output.write(json.dumps(row) + "\n")
            self.count += 1
        self.output.flush()

    def close(self):
        self.drain()
        self.output.close()
        self.k.UnmapViewOfFile(self.view)
        self.k.CloseHandle(self.handle)
