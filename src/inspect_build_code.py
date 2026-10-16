"""Read-only relocation study for explicitly pinned builds, anchored by strings.

Never reuses 14130 code addresses or reads/writes heap objects. The result is
research metadata, not a compatibility approval for any hook or reset adapter.
"""

import argparse
import bisect
import ctypes
from ctypes import wintypes as W
import datetime
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
from capture_preparation import verify_telemetry as verify_14150
from capture_preparation_14200 import verify_telemetry as verify_14200
from capture_preparation_steam25661166 import verify_telemetry as verify_25661166
from capture_context import PROJECT
from run_physics_observer import modules

ROOT = PROJECT
sys.path.insert(0, str(ROOT / "work/pydeps"))
import capstone  # noqa: E402 - Load the project-local optional disassembler first.


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--build", type=int, choices=(14150, 14200, 25661166), default=14150)
    parser.add_argument("--leaf-limit", type=int, choices=(256, 512), default=256)
    parser.add_argument(
        "--field-offset",
        type=lambda v: int(v, 0),
        action="append",
        default=[],
        help="Find bounded decoded memory references to a previously observed field offset",
    )
    parser.add_argument(
        "--call-target",
        type=lambda v: int(v, 0),
        action="append",
        default=[],
        help="Find verified direct calls to an already identified native routine",
    )
    parser.add_argument(
        "--constant",
        type=lambda v: int(v, 0),
        action="append",
        default=[],
        help="Find decoded instructions containing a known 32-bit layout stride",
    )
    parser.add_argument(
        "--rva",
        type=lambda v: int(v, 0),
        action="append",
        default=[],
        help="Previously observed direct-call target; exact unwind function entry required",
    )
    parser.add_argument(
        "--leaf-rva",
        type=lambda v: int(v, 0),
        action="append",
        default=[],
        help="Previously observed call target with no unwind entry; decode at most 256 bytes through first return",
    )
    args = parser.parse_args()
    verify_telemetry = {14150: verify_14150, 14200: verify_14200, 25661166: verify_25661166}[
        args.build
    ]
    verify_telemetry(args.pid)
    listed = modules(args.pid)
    main_module = [m for m in listed if m["name"].lower() == "le mans ultimate.exe"]
    if len(main_module) != 1:
        raise ValueError("Ambiguous executable module")
    image = main_module[0]
    base = image["base"]
    raw = Path(image["path"]).read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    expected = {
        14150: "0fe1f3572094c2cb77c1f0be7a4a1b18ee709a645ddd3b94e6ec070795dc5160",
        14200: "b37a1dd3a5912391db236051995caadd6d5558631ed9ce5d68f8397abe2f6a8c",
        25661166: "66942337813b301b87d12edfa330f33354a2a4b8cd9b048885cbf77eb6f940cf",
    }
    if digest != expected[args.build]:
        raise ValueError("Build changed")
    pe = struct.unpack_from("<I", raw, 0x3C)[0]
    count = struct.unpack_from("<H", raw, pe + 6)[0]
    if not 0 < count <= 96:
        raise ValueError("Section count refused")
    optlen = struct.unpack_from("<H", raw, pe + 20)[0]
    table = pe + 24 + optlen
    sections = [struct.unpack_from("<8sIIIIIIHHI", raw, table + i * 40) for i in range(count)]
    text = next(s for s in sections if s[0].rstrip(b"\0") == b".text")
    pdata = next(s for s in sections if s[0].rstrip(b"\0") == b".pdata")
    if not 0 < text[1] <= 32 * 1024 * 1024:
        raise ValueError("Code read budget exceeded")
    unwind_size = struct.unpack_from("<I", raw, pe + 24 + 112 + 3 * 8 + 4)[0]
    if unwind_size % 12 or unwind_size > 1200000:
        raise ValueError("Unwind table refused")
    entries = [struct.unpack_from("<III", raw, pdata[4] + i) for i in range(0, unwind_size, 12)]
    entries.sort()
    starts = [e[0] for e in entries]

    def file_offset(rva, size):
        section = next(s for s in sections if s[2] <= rva and rva + size <= s[2] + s[3])
        return section[4] + rva - section[2]

    def primary(entry):
        seen = set()
        for _ in range(16):
            if entry in seen:
                raise ValueError("Unwind cycle")
            seen.add(entry)
            offset = file_offset(entry[2], 4)
            version = raw[offset] & 7
            flags = raw[offset] >> 3
            slots = raw[offset + 2]
            if version not in (1, 2):
                raise ValueError("Unwind version refused")
            if not flags & 4:
                return entry[0]
            if flags & 3:
                raise ValueError("Combined unwind flags refused")
            entry = struct.unpack_from(
                "<III", raw, file_offset(entry[2] + 4 + 2 * ((slots + 1) & ~1), 12)
            )
        raise ValueError("Unwind depth exceeded")

    primaries = {entry[0]: primary(entry) for entry in entries}
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = (W.DWORD, W.BOOL, W.DWORD)
    kernel.OpenProcess.restype = W.HANDLE
    kernel.ReadProcessMemory.argtypes = (
        W.HANDLE,
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t),
    )
    kernel.ReadProcessMemory.restype = W.BOOL
    kernel.CloseHandle.argtypes = (W.HANDLE,)
    handle = kernel.OpenProcess(0x410, False, args.pid)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    try:

        def read(address, size):
            buf = ctypes.create_string_buffer(size)
            got = ctypes.c_size_t()
            if (
                not kernel.ReadProcessMemory(handle, address, buf, size, ctypes.byref(got))
                or got.value != size
            ):
                raise ctypes.WinError(ctypes.get_last_error())
            return buf.raw

        if read(base, 64) != raw[:64]:
            raise ValueError("Module header mismatch")
        code = read(base + text[2], text[1])
    finally:
        kernel.CloseHandle(handle)
    verify_telemetry(args.pid)
    labels = (
        b"Physics::HighDetailVehicle::Integrate",
        b"Physics::HighDetailVehicle::Integrate::Driveline",
        b"Physics::HighDetailVehicle::IntegrationSubLoops",
        b"Physics::HighDetailVehicle::ForcesAndTorques",
        b"Physics::FuelAndEngineTemps",
        b"Physics::Vehicle::Dynamics",
        b"Physics::ProcessQueue",
        b"Physics::PreInput",
        b"Physics::PostInput",
        b"Track Limits: Off Track - Lap Invalidated; Lap: %d LapDist: %.2f",
        b"SaveTemplate",
        b"tempGarage.svm",
    )
    targets = {}
    for name in labels:
        for match in re.finditer(re.escape(name) + b"\0", raw):
            s = next(s for s in sections if s[4] <= match.start() < s[4] + s[3])
            targets[s[2] + match.start() - s[4]] = name.decode()
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    decoded = {}
    refs = []

    def instructions(entry):
        start, end, _ = entry
        if start not in decoded:
            decoded[start] = list(decoder.disasm_lite(code[start - text[2] : end - text[2]], start))
        return decoded[start]

    for match in re.finditer(
        rb"[\x48\x4c]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d].{4}", code, re.DOTALL
    ):
        address = text[2] + match.start()
        target = address + 7 + int.from_bytes(match.group()[-4:], "little", signed=True)
        if target not in targets:
            continue
        index = bisect.bisect_right(starts, address) - 1
        if index < 0 or not entries[index][0] <= address < entries[index][1]:
            continue
        entry = entries[index]
        if not any(ins[0] == address for ins in instructions(entry)):
            continue
        refs.append(
            {
                "label": targets[target],
                "reference_rva": hex(address),
                "primary_rva": hex(primaries[entry[0]]),
                "runtime_begin": hex(entry[0]),
                "boundary_verified": True,
            }
        )
    selected = {int(row["primary_rva"], 0) for row in refs}
    if len(args.call_target) > 4:
        raise ValueError("Call target budget refused")
    call_refs = []
    for match in re.finditer(rb"(?=(\xe8....))", code, re.DOTALL):
        address = text[2] + match.start()
        target = address + 5 + int.from_bytes(match.group(1)[1:], "little", signed=True)
        if target not in args.call_target:
            continue
        index = bisect.bisect_right(starts, address) - 1
        if index < 0 or not entries[index][0] <= address < entries[index][1]:
            continue
        entry = entries[index]
        if not any(
            i[0] == address and i[2] == "call" and i[3] == hex(target) for i in instructions(entry)
        ):
            continue
        call_refs.append(
            dict(
                target_rva=hex(target),
                instruction_rva=hex(address),
                primary_rva=hex(primaries[entry[0]]),
            )
        )
        if len(call_refs) > 128:
            raise ValueError("Call reference budget exceeded")
        selected.add(primaries[entry[0]])
    if len(args.field_offset) > 4 or any(not 0x100 <= v <= 0xFFFFFF for v in args.field_offset):
        raise ValueError("Field search budget refused")
    field_refs = []
    for offset in args.field_offset:
        pattern = re.compile(r"\[[^\]]*\+ " + hex(offset) + r"\]")
        checked = set()
        for match in re.finditer(re.escape(struct.pack("<I", offset)), code):
            address = text[2] + match.start()
            index = bisect.bisect_right(starts, address) - 1
            if index < 0 or not entries[index][0] <= address < entries[index][1]:
                continue
            entry = entries[index]
            if entry[0] in checked:
                continue
            checked.add(entry[0])
            for ins in instructions(entry):
                if not pattern.search(ins[3]):
                    continue
                field_refs.append(
                    dict(
                        offset=hex(offset),
                        instruction_rva=hex(ins[0]),
                        instruction=ins[2] + " " + ins[3],
                        primary_rva=hex(primaries[entry[0]]),
                    )
                )
                if len(field_refs) > 128:
                    raise ValueError("Too many field references; narrow search")
                selected.add(primaries[entry[0]])
    constant_refs = []
    if len(args.constant) > 2 or any(not 0x100 <= v <= 0xFFFFFF for v in args.constant):
        raise ValueError("Constant search budget refused")
    for value in args.constant:
        checked = set()
        pattern = re.compile(r"(?<![0-9a-f])" + hex(value) + r"(?![0-9a-f])")
        for match in re.finditer(re.escape(struct.pack("<I", value)), code):
            address = text[2] + match.start()
            index = bisect.bisect_right(starts, address) - 1
            if index < 0 or not entries[index][0] <= address < entries[index][1]:
                continue
            entry = entries[index]
            if entry[0] in checked:
                continue
            checked.add(entry[0])
            for ins in instructions(entry):
                if not pattern.search(ins[3]):
                    continue
                if ins[2] != "add" or not ins[3].endswith(", " + hex(value)):
                    continue
                constant_refs.append(
                    dict(
                        value=hex(value),
                        instruction_rva=hex(ins[0]),
                        instruction=ins[2] + " " + ins[3],
                        primary_rva=hex(primaries[entry[0]]),
                    )
                )
                if len(constant_refs) > 128:
                    raise ValueError("Too many constant references")
                selected.add(primaries[entry[0]])
    if len(args.rva) > 32:
        raise ValueError("Too many requested routines")
    for requested in args.rva:
        if requested not in primaries:
            raise ValueError(f"Requested RVA {requested:#x} is not an exact unwind entry")
        selected.add(primaries[requested])
    for entry in entries:
        if primaries[entry[0]] in selected:
            instructions(entry)
    folder = ROOT / f"work/live-disassembly-{args.build}"
    folder.mkdir(exist_ok=True)
    ranges = []
    if len(args.leaf_rva) > 8:
        raise ValueError("Too many leaf targets")
    leaves = []
    for requested in args.leaf_rva:
        if not text[2] <= requested < text[2] + len(code):
            raise ValueError("Leaf outside code")
        if any(begin <= requested < end for begin, end, _ in entries):
            raise ValueError("Leaf overlaps unwind range")
        instructions_leaf = []
        ended = False
        for address, size, mnemonic, operands in decoder.disasm_lite(
            code[requested - text[2] : requested - text[2] + args.leaf_limit], requested
        ):
            instructions_leaf.append(f"{address:08x} {mnemonic} {operands}")
            if mnemonic in ("ret", "retf"):
                ended = True
                break
            if mnemonic in ("int3", "ud2"):
                break
        (folder / f"{requested:08x}-leaf.asm").write_text("\n".join(instructions_leaf))
        leaves.append(
            {
                "begin_rva": hex(requested),
                "first_return_reached": ended,
                "byte_limit": args.leaf_limit,
                "scope": "Bounded linear prefix only; branches may leave this range; not a complete function contract.",
            }
        )
    for entry in entries:
        start, end, _ = entry
        if start not in decoded:
            continue
        insns = decoded[start]
        lines = []
        for address, size, mnemonic, operands in insns:
            annotation = ""
            match = re.search(r"\[rip ([+-]) (0x[0-9a-f]+)\]", operands)
            if match:
                target = address + size + int(match[2], 0) * (1 if match[1] == "+" else -1)
                try:
                    offset = file_offset(target, 1)
                    candidate = raw[offset : offset + 160].split(b"\0")[0]
                    if len(candidate) > 3 and all(32 <= b < 127 for b in candidate):
                        annotation = " ; " + candidate.decode()
                except StopIteration:
                    pass
            lines.append(f"{address:08x} {mnemonic} {operands}{annotation}")
        (folder / f"{start:08x}.asm").write_text("\n".join(lines))
        ranges.append(
            {
                "begin_rva": hex(start),
                "end_rva": hex(end),
                "primary_rva": hex(primaries[start]),
                "fully_decoded": bool(insns) and insns[-1][0] + insns[-1][1] == end,
            }
        )
    result = {
        "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "exe_sha256": digest,
        "build": args.build,
        "code_bytes_read": len(code) + 64,
        "heap_bytes_read": 0,
        "physics_writes": 0,
        "hook_loaded": False,
        "physics_compatible": False,
        "references": refs,
        "field_references": field_refs,
        "call_references": call_refs,
        "constant_references": constant_refs,
        "decoded_ranges": ranges,
        "leaf_prefixes": leaves,
        "limitations": "String-anchored code references, not verified object offsets, ABI equivalence, synchronization or restore contracts.",
    }
    (PROJECT / f"compatibility/{args.build}-code-analysis.json").write_text(
        json.dumps(result, indent=2) + "\n"
    )
    print(json.dumps({k: v for k, v in result.items() if k != "decoded_ranges"}, indent=2))


if __name__ == "__main__":
    main()
