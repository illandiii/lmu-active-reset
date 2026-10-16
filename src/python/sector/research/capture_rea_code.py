"""Read-only .text capture for static REA analysis; never launches the derived PE.

The derived artifact combines captured executable code with original disk data.
It is not a process snapshot and is not evidence of live heap/vtable contents.
"""

import argparse
import ctypes
from ctypes import wintypes as W
import datetime
import hashlib
import json
from pathlib import Path
import struct
from sector.observers.run_physics_observer import modules

EXPECTED = "66942337813b301b87d12edfa330f33354a2a4b8cd9b048885cbf77eb6f940cf"
ANCHORS = {0xECB4E0: "488bc4488958104889701848897820", 0xECD670: "48895c2408574881ec90000000"}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def derive(raw, code, runtime_base):
    pe = struct.unpack_from("<I", raw, 60)[0]
    if raw[pe : pe + 4] != b"PE\0\0" or struct.unpack_from("<H", raw, pe + 24)[0] != 0x20B:
        raise ValueError("Expected PE32+")
    opt = pe + 24
    preferred = struct.unpack_from("<Q", raw, opt + 24)[0]
    table = opt + struct.unpack_from("<H", raw, pe + 20)[0]
    sections = [
        struct.unpack_from("<8sIIIIIIHHI", raw, table + i * 40)
        for i in range(struct.unpack_from("<H", raw, pe + 6)[0])
    ]
    text = next(s for s in sections if s[0].rstrip(b"\0") == b".text")
    if len(code) != text[1] or not 0 < len(code) <= 32 * 1024 * 1024 or text[1] > text[3]:
        raise ValueError("Unexpected code section size")
    for rva, expected in ANCHORS.items():
        if code[rva - text[2] : rva - text[2] + len(bytes.fromhex(expected))] != bytes.fromhex(
            expected
        ):
            raise ValueError(f"Loaded code anchor mismatch at {rva:#x}")

    def offset(rva, size):
        s = next(s for s in sections if s[2] <= rva and rva + size <= s[2] + s[3])
        return s[4] + rva - s[2]

    normalized = bytearray(code)
    reloc_rva, reloc_size = struct.unpack_from("<II", raw, opt + 112 + 5 * 8)
    cursor = 0
    count = 0
    while cursor < reloc_size:
        block = offset(reloc_rva + cursor, 8)
        page, length = struct.unpack_from("<II", raw, block)
        if length < 8 or length % 2 or cursor + length > reloc_size:
            raise ValueError("Bad relocation block")
        block = offset(reloc_rva + cursor, length)
        for i in range(8, length, 2):
            item = struct.unpack_from("<H", raw, block + i)[0]
            kind, rva = item >> 12, page + (item & 4095)
            if not kind or not text[2] <= rva < text[2] + len(code):
                continue
            if kind != 10 or rva + 8 > text[2] + len(code):
                raise ValueError("Unsupported text relocation")
            pos = rva - text[2]
            value = struct.unpack_from("<Q", normalized, pos)[0]
            struct.pack_into(
                "<Q", normalized, pos, (value - runtime_base + preferred) & ((1 << 64) - 1)
            )
            count += 1
        cursor += length
    derived = bytearray(raw)
    derived[text[4] : text[4] + len(normalized)] = normalized
    return bytes(derived), {
        "text_rva": hex(text[2]),
        "text_size": len(code),
        "preferred_base": hex(preferred),
        "runtime_base": hex(runtime_base),
        "text_relocations_normalized": count,
        "captured_text_sha256": sha(code),
        "normalized_text_sha256": sha(normalized),
        "anchors": {hex(k): v for k, v in ANCHORS.items()},
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() or args.output.with_suffix(".manifest.json").exists():
        raise ValueError("Output already exists")
    inventory = modules(args.pid)
    if any("easyanticheat" in m["name"].lower() for m in inventory):
        raise ValueError("EAC module present")
    matches = [m for m in inventory if m["name"].lower() == "le mans ultimate.exe"]
    if len(matches) != 1:
        raise ValueError("Ambiguous target module")
    module = matches[0]
    original = Path(module["path"])
    raw = original.read_bytes()
    if sha(raw) != EXPECTED:
        raise ValueError("Unsupported executable")
    pe = struct.unpack_from("<I", raw, 60)[0]
    table = pe + 24 + struct.unpack_from("<H", raw, pe + 20)[0]
    sections = [
        struct.unpack_from("<8sIIIIIIHHI", raw, table + i * 40)
        for i in range(struct.unpack_from("<H", raw, pe + 6)[0])
    ]
    section = next(s for s in sections if s[0].rstrip(b"\0") == b".text")
    if not 0 < section[1] <= 32 * 1024 * 1024:
        raise ValueError("Code budget exceeded")
    k = ctypes.WinDLL("kernel32", use_last_error=True)
    k.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
    k.OpenProcess.restype = W.HANDLE
    k.ReadProcessMemory.argtypes = [
        W.HANDLE,
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t),
    ]
    k.ReadProcessMemory.restype = W.BOOL
    k.CloseHandle.argtypes = [W.HANDLE]
    handle = k.OpenProcess(0x410, False, args.pid)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    try:

        def read(address, size):
            buffer = ctypes.create_string_buffer(size)
            got = ctypes.c_size_t()
            if (
                not k.ReadProcessMemory(handle, address, buffer, size, ctypes.byref(got))
                or got.value != size
            ):
                raise ctypes.WinError(ctypes.get_last_error())
            return buffer.raw

        if read(module["base"], 64) != raw[:64]:
            raise ValueError("Header mismatch")
        code = read(module["base"] + section[2], section[1])
        if read(module["base"] + section[2], section[1]) != code:
            raise ValueError("Code changed during capture")
    finally:
        k.CloseHandle(handle)
    if sha(original.read_bytes()) != EXPECTED or modules(args.pid) != inventory:
        raise ValueError("Module inventory changed")
    derived, detail = derive(raw, code, module["base"])
    manifest = {
        "schema": "lmu-rea-code-v1",
        "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "pid": args.pid,
        "original_path": str(original),
        "original_sha256": EXPECTED,
        "derived_sha256": sha(derived),
        "code_stable_two_reads": True,
        "process_writes": 0,
        "heap_bytes_read": 0,
        "game_executed": False,
        "limitations": [
            "Hybrid artifact: live .text plus original disk headers/data. Not a runtime state snapshot.",
            "Static decompilation does not establish restore completeness or synchronization.",
        ],
        **detail,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("xb") as f:
        f.write(derived)
    with args.output.with_suffix(".manifest.json").open("x") as f:
        json.dump(manifest, f, indent=2)
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
