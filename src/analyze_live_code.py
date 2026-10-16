import sys
import pathlib
import ctypes
import struct
import hashlib
import json
import re
import bisect
import urllib.request
from ctypes import wintypes

root = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "work/pydeps"))
import capstone  # noqa: E402 - Load the project-local optional disassembler first.

pid = int(sys.argv[1])
base = int(sys.argv[2], 0)
extra_rvas = [int(value, 0) for value in sys.argv[3:]]
exe = pathlib.Path(r"D:\SteamLibrary\steamapps\common\Le Mans Ultimate\Le Mans Ultimate.exe")
raw = exe.read_bytes()
digest = hashlib.sha256(raw).hexdigest()
if digest != "bb8661cf6f96a056374afac29018c1fdd71f86164c158a71043bb7c244f7407e":
    raise ValueError("Build mismatch")
http = urllib.request.build_opener(urllib.request.ProxyHandler({}))


def check_context():
    with http.open("http://127.0.0.1:6397/navigation/state", timeout=2) as response:
        s = json.load(response)["state"]
    if (
        s.get("appBuild") != 14130
        or s.get("settingMode") != "SETTING_GRANDPRIX"
        or s.get("gameSession") not in ["PRACTICE1", "PRACTICE2", "PRACTICE3", "PRACTICE4"]
    ):
        raise ValueError("Context refused")
    return {k: s[k] for k in ("appBuild", "settingMode", "gameSession")}


before = check_context()
peoff = struct.unpack_from("<I", raw, 0x3C)[0]
n = struct.unpack_from("<H", raw, peoff + 6)[0]
optlen = struct.unpack_from("<H", raw, peoff + 20)[0]
table = peoff + 24 + optlen
sections = [struct.unpack_from("<8sIIIIIIHHI", raw, table + i * 40) for i in range(n)]


def rva_at(offset):
    return next(s[2] + offset - s[4] for s in sections if s[4] <= offset < s[4] + s[3])


textsec = next(s for s in sections if s[0].rstrip(b"\0") == b".text")
pdata = next(s for s in sections if s[0].rstrip(b"\0") == b".pdata")
funlen = struct.unpack_from("<I", raw, peoff + 24 + 112 + 3 * 8 + 4)[0]
runtime_entries = [struct.unpack_from("<III", raw, pdata[4] + i) for i in range(0, funlen, 12)]
functions = [entry[:2] for entry in runtime_entries]
functions.sort()
starts = [f[0] for f in functions]


def raw_offset(rva, size):
    sec = next((s for s in sections if s[2] <= rva and rva + size <= s[2] + s[3]), None)
    if sec is None:
        raise ValueError("Unwind RVA outside file-backed sections")
    return sec[4] + rva - sec[2]


def primary_entry(entry):
    seen = set()
    for depth in range(16):
        if entry in seen:
            raise ValueError("Cyclic unwind chain")
        seen.add(entry)
        offset = raw_offset(entry[2], 4)
        version = raw[offset] & 7
        flags = raw[offset] >> 3
        slots = raw[offset + 2]
        if version not in (1, 2):
            raise ValueError("Unsupported unwind version")
        if not flags & 4:
            return entry
        if flags & 3:
            raise ValueError("Invalid combined unwind flags")
        chain_rva = entry[2] + 4 + 2 * ((slots + 1) & ~1)
        entry = struct.unpack_from("<III", raw, raw_offset(chain_rva, 12))
    raise ValueError("Unwind chain too deep")


primary_by_start = {entry[0]: primary_entry(entry)[0] for entry in runtime_entries}
k = ctypes.WinDLL("kernel32", use_last_error=True)
k.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
k.OpenProcess.restype = wintypes.HANDLE
k.ReadProcessMemory.argtypes = (
    wintypes.HANDLE,
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.c_size_t,
    ctypes.POINTER(ctypes.c_size_t),
)
k.ReadProcessMemory.restype = wintypes.BOOL
k.QueryFullProcessImageNameW.argtypes = (
    wintypes.HANDLE,
    wintypes.DWORD,
    wintypes.LPWSTR,
    ctypes.POINTER(wintypes.DWORD),
)
k.QueryFullProcessImageNameW.restype = wintypes.BOOL
k.CloseHandle.argtypes = (wintypes.HANDLE,)
h = k.OpenProcess(0x410, False, pid)
if not h:
    raise OSError(ctypes.get_last_error(), "Ordinary read access denied")
try:
    path = ctypes.create_unicode_buffer(32768)
    length = wintypes.DWORD(len(path))
    if (
        not k.QueryFullProcessImageNameW(h, 0, path, ctypes.byref(length))
        or pathlib.Path(path.value) != exe
    ):
        raise ValueError("Wrong process")

    def read_code(address, size):
        buf = ctypes.create_string_buffer(size)
        count = ctypes.c_size_t()
        if (
            not k.ReadProcessMemory(h, address, buf, size, ctypes.byref(count))
            or count.value != size
        ):
            raise OSError(ctypes.get_last_error(), "Read denied or incomplete")
        return buf.raw

    if read_code(base, 64) != raw[:64]:
        raise ValueError("Module base does not match image")
    code = read_code(base + textsec[2], textsec[1])
finally:
    k.CloseHandle(h)
after = check_context()
if before != after:
    raise ValueError("Context changed")
names = [
    b"Cannot save game while in multiplayer",
    b"Not allowed in multiplayer",
    b"Racing Offline",
    b"Begin LoadGame",
    b"Finished LoadGame",
    b"Physics::ProcessQueue",
    b"Physics::PreInput",
    b"Physics::PostInput",
    b"Physics::Collision",
    b"Physics::HighDetailVehicle::Integrate",
    b"Physics::HighDetailVehicle::IntegrationSubLoops",
    b"REPLAY RESTORE SESSION",
    b"/rest/sessions/SaveLoad/loadGame",
    b"/rest/sessions/SaveLoad/saveGame",
    b"[TELEPORT]",
    b"Vehicle (%s) in slot %d teleported from %f, %f, %f to %f, %f, %f (%fm in %fs)",
    b"Physics::HighDetailVehicle::Integrate::Driveline",
    b"Physics::HighDetailVehicle::ForcesAndTorques",
    b"Physics::FuelAndEngineTemps",
    b"Physics::Vehicle::Dynamics",
    b"Track Limits: Off Track - Lap Invalidated; Lap: %d LapDist: %.2f",
    b"Lap will not count",
    b"This and next lap will not count",
    b"Invalid Lap Cut Track",
    b"Invalid Lap Off Track",
]
targets = {}
for name in names:
    for match in re.finditer(re.escape(name) + b"\0", raw):
        targets[rva_at(match.start())] = name.decode()
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
refs = []
disassembled = {}
for match in re.finditer(rb"[\x48\x4c]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d].{4}", code, re.DOTALL):
    rva = textsec[2] + match.start()
    target = rva + 7 + int.from_bytes(match.group()[-4:], "little", signed=True)
    if target not in targets:
        continue
    fi = bisect.bisect_right(starts, rva) - 1
    if fi < 0 or not functions[fi][0] <= rva < functions[fi][1]:
        continue
    start, end = functions[fi]
    if start not in disassembled:
        disassembled[start] = list(
            md.disasm_lite(code[start - textsec[2] : end - textsec[2]], start)
        )
    insns = disassembled[start]
    if not any(i[0] == rva for i in insns):
        continue
    refs.append(
        {
            "string": targets[target],
            "string_rva": hex(target),
            "reference_rva": hex(rva),
            "function_begin_rva": hex(start),
            "function_end_rva": hex(end),
            "boundary_checked": True,
        }
    )
outdir = root / "work/live-disassembly"
outdir.mkdir(exist_ok=True)
call_refs = []
for match in re.finditer(rb"\xe8.{4}", code, re.DOTALL):
    rva = textsec[2] + match.start()
    target = rva + 5 + int.from_bytes(match.group()[1:], "little", signed=True)
    if target not in (0xCCC0E0, 0xCCC720, 0xEADBE0, 0xEB04A0, 0xEC7240, 0xEAD230):
        continue
    fi = bisect.bisect_right(starts, rva) - 1
    if fi < 0 or not functions[fi][0] <= rva < functions[fi][1]:
        continue
    start, end = functions[fi]
    if start not in disassembled:
        disassembled[start] = list(
            md.disasm_lite(code[start - textsec[2] : end - textsec[2]], start)
        )
    if not any(i[0] == rva and i[2] == "call" and i[3] == hex(target) for i in disassembled[start]):
        continue
    call_refs.append(
        {
            "call_rva": hex(rva),
            "target_rva": hex(target),
            "caller_primary_rva": hex(primary_by_start[start]),
            "instruction_boundary_checked": True,
        }
    )
score_refs = []
for match in re.finditer(
    rb"(?:\x60\x01\x00\x00|\x64\x01\x00\x00)", code[0xCB0000 - textsec[2] : 0xCF0000 - textsec[2]]
):
    rva = 0xCB0000 + match.start()
    fi = bisect.bisect_right(starts, rva) - 1
    if fi < 0 or not functions[fi][0] <= rva < functions[fi][1]:
        continue
    start, end = functions[fi]
    if start not in disassembled:
        disassembled[start] = list(
            md.disasm_lite(code[start - textsec[2] : end - textsec[2]], start)
        )
    for a, size, m, op in disassembled[start]:
        if a <= rva < a + size and (" + 0x160]" in op or " + 0x164]" in op):
            score_refs.append(
                {
                    "instruction_rva": hex(a),
                    "procedure_primary_rva": hex(primary_by_start[start]),
                    "mnemonic": m,
                    "operands": op,
                }
            )
for requested in (
    0xB8E230,
    0xB836C0,
    0xB847D0,
    0x2DA200,
    0x2DA960,
    0xB8C4E0,
    0xECE500,
    0xE79170,
    0xB16F20,
    0xB142C0,
    0xEAFAA0,
    0xEAF560,
    0xEAFD10,
    0xEAFDA0,
    0xFFF500,
    0xFFF000,
    0xFFDBA0,
    0xFFD5F0,
    0xFFF9F0,
    0xFFB8C0,
    0xFFBA90,
    0x3D4F60,
    0x3D4F90,
    0x3D50C0,
    0x3D5180,
    *extra_rvas,
):
    fi = bisect.bisect_right(starts, requested) - 1
    if fi >= 0 and functions[fi][0] <= requested < functions[fi][1]:
        start, end = functions[fi]
        disassembled[start] = list(
            md.disasm_lite(code[start - textsec[2] : end - textsec[2]], start)
        )
selected_primaries = {primary_by_start[start] for start in disassembled}
for start, end in functions:
    if primary_by_start[start] in selected_primaries and start not in disassembled:
        disassembled[start] = list(
            md.disasm_lite(code[start - textsec[2] : end - textsec[2]], start)
        )
leaf_ranges = {}
for start in (0x3D4F60, 0x3D4F90, 0xB026B0, 0xB026C0, 0x29A700, 0xB03480, 0xB03490, 0xB03510):
    # Direct call targets in verified component integration; no unwind record.
    # Only accept a bounded straight-line leaf ending in RET, never a guessed
    # extent borrowed from the preceding RUNTIME_FUNCTION.
    insns = []
    for insn in md.disasm_lite(code[start - textsec[2] : start - textsec[2] + 256], start):
        insns.append(insn)
        if insn[2] == "ret":
            break
        if insn[2].startswith(("j", "loop", "call")):
            raise ValueError("Leaf control flow requires separate analysis")
    if not insns or insns[-1][2] != "ret":
        raise ValueError("Leaf end not found")
    disassembled[start] = insns
    leaf_ranges[start] = insns[-1][0] + insns[-1][1]
for start, insns in disassembled.items():
    lines = []
    for a, size, m, op in insns:
        annotation = ""
        match = re.search(r"\[rip ([+-]) (0x[0-9a-f]+)\]", op)
        if match:
            target = a + size + int(match[2], 16) * (1 if match[1] == "+" else -1)
            sec = next((s for s in sections if s[2] <= target < s[2] + s[3]), None)
            if sec:
                offset = sec[4] + target - sec[2]
                candidate = raw[offset : offset + 160].split(b"\0")[0]
                if len(candidate) > 3 and all(32 <= b < 127 for b in candidate):
                    annotation = " ; " + candidate.decode()
        lines.append(f"{a:08x} {m} {op}{annotation}")
    (outdir / f"{start:08x}.asm").write_text("\n".join(lines), encoding="utf-8")
result = {
    "exe_sha256": digest,
    "context": after,
    "read_access": "PROCESS_VM_READ | PROCESS_QUERY_INFORMATION",
    "code_bytes_read": len(code),
    "data_or_heap_read": False,
    "write_access_requested": False,
    "references": refs,
    "call_references": call_refs,
    "score_field_references": score_refs,
    "score_field_search_region": ["0xcb0000", "0xcf0000"],
    "limitations": "References and function ranges only. ABI, ownership, synchronization and restore contracts remain unverified. Score field references are candidates in a bounded code region, not exhaustive proof of all writers.",
}
result["decoded_ranges"] = []
for start, insns in disassembled.items():
    if start in leaf_ranges:
        continue
    _, end = functions[bisect.bisect_left(starts, start)]
    decoded_until = insns[-1][0] + insns[-1][1] if insns else start
    result["decoded_ranges"].append(
        {
            "runtime_begin_rva": hex(start),
            "runtime_end_rva": hex(end),
            "procedure_primary_rva": hex(primary_by_start[start]),
            "decoded_until_rva": hex(decoded_until),
            "range_fully_decoded": decoded_until == end,
        }
    )
result["bounded_leaf_ranges"] = [
    {
        "begin_rva": hex(s),
        "end_rva": hex(e),
        "boundary_basis": "Verified direct call target, straight-line decoding to RET within 256 bytes; no unwind record",
    }
    for s, e in leaf_ranges.items()
]
(root / "live-code-findings.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
print(json.dumps(result, indent=2))
