"""Read-only PE investigation. Report string references, never hook targets.

Requires pefile 2024.8.26 and capstone 5.0.7. No process access.
"""

import argparse
import bisect
import hashlib
import json
import re
from pathlib import Path

import capstone
import pefile


def inspect(exe: Path):
    raw = exe.read_bytes()
    expected = "bb8661cf6f96a056374afac29018c1fdd71f86164c158a71043bb7c244f7407e"
    if hashlib.sha256(raw).hexdigest() != expected:
        raise ValueError("Unverified executable build")
    pe = pefile.PE(data=raw)
    base = pe.OPTIONAL_HEADER.ImageBase
    image = pe.get_memory_mapped_image()
    functions = sorted(
        (e.struct.BeginAddress, e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION
    )
    starts = [f[0] for f in functions]
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
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
    ]
    targets = {}
    for name in names:
        for m in re.finditer(re.escape(name) + b"\0", raw):
            targets[pe.get_rva_from_offset(m.start())] = name.decode()
    refs = []
    raw_candidates = 0
    target_candidates = 0
    for section in pe.sections:
        if not section.Characteristics & 0x20000000:
            continue
        data = section.get_data()
        for m in re.finditer(
            rb"[\x48\x4c]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d].{4}", data, re.DOTALL
        ):
            raw_candidates += 1
            rva = section.VirtualAddress + m.start()
            target = rva + 7 + int.from_bytes(m.group()[-4:], "little", signed=True)
            if target not in targets:
                continue
            target_candidates += 1
            index = bisect.bisect_right(starts, rva) - 1
            if index < 0 or not functions[index][0] <= rva < functions[index][1]:
                continue
            start, end = functions[index]
            instructions = list(md.disasm_lite(image[start:end], base + start))
            aligned = next((i for i, x in enumerate(instructions) if x[0] == base + rva), None)
            if aligned is None:
                continue
            refs.append(
                {
                    "string": targets[target],
                    "string_rva": hex(target),
                    "instruction_rva": hex(rva),
                    "runtime_function_begin_rva": hex(start),
                    "runtime_function_end_rva": hex(end),
                    "instruction_boundary_verified": True,
                    "meaning": "Reference only; containing runtime-function range is not a verified callable contract.",
                }
            )
    rtti = set()
    for match in re.finditer(
        rb"[^\x00]{0,80}Serialize(?:Vehicle|Tires|TireInventory|Session)@GameMemento@@[^\x00]{0,900}",
        raw,
    ):
        text = match.group().decode("ascii", errors="ignore")
        for field in re.findall(
            r"Serialize(?:Vehicle|Tires|TireInventory|Session)@GameMemento", text
        ):
            rtti.add(field)
    return {
        "sha256": expected,
        "machine": hex(pe.FILE_HEADER.Machine),
        "preferred_image_base": hex(base),
        "runtime_function_entries": len(functions),
        "target_strings_found": len(targets),
        "rip_lea_byte_candidates": raw_candidates,
        "target_reference_byte_candidates": target_candidates,
        "dependencies": {"capstone": capstone.__version__, "pefile": pefile.__version__},
        "sections": [
            {
                "name": s.Name.rstrip(b"\0").decode(),
                "rva": hex(s.VirtualAddress),
                "raw_size": s.SizeOfRawData,
                "entropy": s.get_entropy(),
            }
            for s in pe.sections
        ],
        "serializer_rtti_names": sorted(rtti),
        "references": refs,
        "limitation": "No live addresses, hooks, function contracts, ownership, serialization completeness or restoration feasibility established.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("exe", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    result = inspect(args.exe)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(
        json.dumps(
            {
                "references": len(result["references"]),
                "serializer_rtti_names": result["serializer_rtti_names"],
            }
        )
    )
