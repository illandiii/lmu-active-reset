import ctypes
import struct
import json
import sys
import pathlib
import collections
from capture_context import verify, get_json, KERNEL, PROJECT
from ctypes import wintypes as W


root = pathlib.Path(__file__).resolve().parents[1]

pid = int(sys.argv[1])
base = int(sys.argv[2], 0)
verify(pid)


def context():
    s = get_json("/navigation/state")["state"]
    keys = ("appBuild", "settingMode", "gameSession", "navigationState")
    if (
        s.get("appBuild") != 14130
        or s.get("settingMode") != "SETTING_GRANDPRIX"
        or s.get("gameSession") not in ("PRACTICE1", "PRACTICE2", "PRACTICE3", "PRACTICE4")
        or s.get("navigationState") != "NAV_EVENT"
    ):
        raise ValueError("Practice monitor required")
    return {k: s[k] for k in keys}


before = context()
h = KERNEL.OpenProcess(0x410, False, pid)
if not h:
    raise ctypes.WinError(ctypes.get_last_error())
KERNEL.ReadProcessMemory.argtypes = (
    W.HANDLE,
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.c_size_t,
    ctypes.POINTER(ctypes.c_size_t),
)
KERNEL.ReadProcessMemory.restype = W.BOOL
total = 0
try:

    def read(address, size):
        global total
        if not 0x10000 <= address < 0x800000000000 or not 0 < size <= 512 or total + size > 65536:
            raise ValueError("Bound refused")
        buf = ctypes.create_string_buffer(size)
        n = ctypes.c_size_t()
        if not KERNEL.ReadProcessMemory(h, address, buf, size, ctypes.byref(n)) or n.value != size:
            raise ctypes.WinError(ctypes.get_last_error())
        total += size
        return buf.raw

    def pointer(a):
        return struct.unpack("<Q", read(a, 8))[0]

    inv = json.loads((PROJECT / "inventory.json").read_text(encoding="utf-8-sig"))
    exe = pathlib.Path(inv["install_path"]) / "Le Mans Ultimate.exe"
    raw = exe.read_bytes()
    if read(base, 64) != raw[:64]:
        raise ValueError("Base mismatch")
    pe = struct.unpack_from("<I", raw, 0x3C)[0]
    image_size = struct.unpack_from("<I", raw, pe + 24 + 56)[0]

    def in_image(a, n=1):
        return base <= a and a + n <= base + image_size

    owner = pointer(base + 0x1DD94D8)
    vehicle = pointer(owner + 0x15978)

    def type_info(obj, slots):
        vt = pointer(obj)
        if not in_image(vt, slots * 8):
            return {"type": "unresolved: first word outside image"}
        result = {"vtable_rva": hex(vt - base)}
        col = pointer(vt - 8)
        if in_image(col, 24):
            signature, offset, constructor_disp, type_rva, hierarchy, self_rva = struct.unpack(
                "<6I", read(col, 24)
            )
            if signature == 1 and col - self_rva == base and in_image(base + type_rva + 16, 192):
                name = read(base + type_rva + 16, 192).split(b"\0")[0]
                if name.startswith(b".?A") and all(32 <= b < 127 for b in name):
                    result.update(type=name.decode(), subobject_offset=offset)
        entries = struct.unpack("<" + "Q" * slots, read(vt, slots * 8))
        result["slots"] = {
            hex(i * 8): hex(addr - base) if in_image(addr) else "outside_image"
            for i, addr in enumerate(entries)
        }
        return result

    rows = {"vehicle": type_info(vehicle, 64)}
    vectors = {}
    objects = {}
    game_vehicle = pointer(vehicle + 0x4BC0)
    scoring = game_vehicle + 0x1CE90
    if pointer(owner) != game_vehicle or pointer(scoring) != owner:
        raise ValueError("Scoring ownership did not match code-derived relation")
    score_fields = {
        "owner_relation_verified": True,
        "candidate_lap_10": struct.unpack("<i", read(scoring + 0x10, 4))[0],
        "candidate_lap_distance_24": struct.unpack("<f", read(scoring + 0x24, 4))[0],
        "current_count_flag_160": struct.unpack("<i", read(scoring + 0x160, 4))[0],
        "next_lap_flag_164": read(scoring + 0x164, 1)[0],
        "off_track_marker_170": read(scoring + 0x170, 1)[0],
        "remembered_count_flag_27288": struct.unpack("<i", read(scoring + 0x27288, 4))[0],
    }
    for off in (0x46F0, 0x46F8):
        obj = pointer(vehicle + off)
        objects[off] = obj
        rows[hex(off)] = type_info(obj, 24)
    for off in (0x23F8, 0x2410, 0x2428, 0x2440):
        begin, end = struct.unpack("<QQ", read(vehicle + off, 16))
        if not 0 < end - begin <= 64 * 8 or (end - begin) % 8:
            raise ValueError("Vector bound refused")
        ptrs = struct.unpack("<" + "Q" * ((end - begin) // 8), read(begin, end - begin))
        vectors[off] = (begin, end, ptrs)
        rows[hex(off)] = [
            {"index": i, **type_info(obj, 21 if off == 0x2410 else 4)} for i, obj in enumerate(ptrs)
        ]
    for off, (begin, end, ptrs) in vectors.items():
        if read(vehicle + off, 16) != struct.pack("<QQ", begin, end) or read(
            begin, end - begin
        ) != struct.pack("<" + "Q" * len(ptrs), *ptrs):
            raise ValueError("Component lifetime changed")
    if (
        any(pointer(vehicle + off) != obj for off, obj in objects.items())
        or pointer(base + 0x1DD94D8) != owner
        or pointer(owner + 0x15978) != vehicle
        or pointer(owner) != game_vehicle
        or pointer(scoring) != owner
    ):
        raise ValueError("Owner lifetime changed")
finally:
    KERNEL.CloseHandle(h)
if context() != before:
    raise ValueError("Context changed")
result = {
    "context": before,
    "bytes_read": total,
    "physics_writes": 0,
    "types": rows,
    "scoring_candidate": score_fields,
    "limitations": "Read-only pointer and compiler type metadata observation. Vtable slots are not verified method contracts.",
}
(PROJECT / "component-types.json").write_text(json.dumps(result, indent=2) + "\n")
for key, value in rows.items():
    print(
        key,
        json.dumps(dict(collections.Counter(x.get("type", "unknown") for x in value)))
        if isinstance(value, list)
        else json.dumps(value),
    )
print("Scoring", json.dumps(score_fields))
