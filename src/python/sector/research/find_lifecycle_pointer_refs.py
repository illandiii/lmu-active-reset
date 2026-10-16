"""Find absolute function pointers in a verified PE capture's data sections.

The capture combines live code with original disk data. Optional --pid validates
only the matching pointer slots against the running module, with read-only access.
Results are candidates for indirect callers, not a synchronization proof.
"""

import argparse
import hashlib
import json
import struct
from pathlib import Path
import pefile


def scan(capture, targets):
    raw = capture.read_bytes()
    manifest = json.loads(capture.with_suffix(".manifest.json").read_text())
    if hashlib.sha256(raw).hexdigest() != manifest["derived_sha256"]:
        raise ValueError("Capture hash mismatch")
    image = pefile.PE(data=raw)
    base = image.OPTIONAL_HEADER.ImageBase
    matches = []
    for section in image.sections:
        if section.Characteristics & 0x20000000:
            continue
        data = section.get_data()
        for target in targets:
            pattern = struct.pack("<Q", base + target)
            position = data.find(pattern)
            while position >= 0:
                matches.append(
                    {
                        "slot_rva": hex(section.VirtualAddress + position),
                        "target_rva": hex(target),
                        "section": section.Name.rstrip(b"\0").decode(),
                        "aligned": position % 8 == 0,
                    }
                )
                position = data.find(pattern, position + 1)
    return {
        "capture_sha256": manifest["derived_sha256"],
        "executable_sha256": manifest["original_sha256"],
        "matches": matches,
        "process_writes": 0,
        "limitations": "Original disk data candidates; no coverage of encoded pointers, runtime registrations or all indirect callers.",
    }


def verify_live(result, pid):
    from sector.observers.run_physics_observer import k, modules, read_process

    loaded = modules(pid)
    game = next(module for module in loaded if module["name"].lower() == "le mans ultimate.exe")
    if hashlib.sha256(Path(game["path"]).read_bytes()).hexdigest() != result["executable_sha256"]:
        raise ValueError("Running executable does not match the capture build")
    handle = k.OpenProcess(0x10 | 0x1000, False, pid)
    if not handle:
        raise OSError("Cannot open process for read-only validation")
    try:
        for match in result["matches"]:
            value = struct.unpack(
                "<Q", read_process(handle, game["base"] + int(match["slot_rva"], 16), 8)
            )[0]
            match["live_matches_target"] = value == game["base"] + int(match["target_rva"], 16)
        result["pid"] = pid
    finally:
        k.CloseHandle(handle)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("rvas", nargs="+", type=lambda value: int(value, 16))
    parser.add_argument("--pid", type=int)
    args = parser.parse_args()
    result = scan(args.capture, args.rvas)
    if args.pid:
        verify_live(result, args.pid)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2))
    print(
        json.dumps(
            {
                "matches": len(result["matches"]),
                "output": str(args.output),
                "live_validated": args.pid is not None,
                "process_writes": 0,
            }
        )
    )
