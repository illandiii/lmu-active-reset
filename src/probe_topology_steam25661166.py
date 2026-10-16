"""Locate the player-owner access and typed constraint tables. Read-only.

Code/rdata scans are bounded by PE sections. Heap reads are limited to named
links and bounded vectors, never an object dump. Does not approve a restore.
"""

import argparse
import collections
import hashlib
import json
import re
import struct
from pathlib import Path
from capture_preparation_steam25661166 import verify_telemetry
from run_physics_observer import modules, k, read_process
from capture_context import PROJECT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    args = parser.parse_args()
    verify_telemetry(args.pid)
    listed = modules(args.pid)
    if any("easyanti" in m["name"].lower() or m["name"].lower().startswith("eac") for m in listed):
        raise ValueError("EAC present")
    image = next(m for m in listed if m["name"].lower() == "le mans ultimate.exe")
    base = image["base"]
    raw = Path(image["path"]).read_bytes()
    pe = struct.unpack_from("<I", raw, 0x3C)[0]
    n = struct.unpack_from("<H", raw, pe + 6)[0]
    opt = struct.unpack_from("<H", raw, pe + 20)[0]
    if not 0 < n <= 96:
        raise ValueError("Section budget")
    sections = [struct.unpack_from("<8sIIIIIIHHI", raw, pe + 24 + opt + 40 * i) for i in range(n)]
    handle = k.OpenProcess(0x410, False, args.pid)
    if not handle:
        raise OSError("Read-only process access failed")
    heap_bytes = 0

    def read(p, size):
        nonlocal heap_bytes
        if not 0x10000 <= p < 0x800000000000 or not 0 < size <= 1024 or heap_bytes + size > 32768:
            raise ValueError("Typed read budget")
        heap_bytes += size
        return read_process(handle, p, size)

    def pointer(p):
        return struct.unpack("<Q", read(p, 8))[0]

    def vector(p):
        begin, end, capacity = struct.unpack("<3Q", read(p, 24))
        if (
            not 0x10000 <= begin <= end <= capacity
            or (end - begin) % 8
            or (capacity - begin) % 8
            or not 0 < (end - begin) // 8 <= 64
        ):
            raise ValueError("Vector bounds")
        return list(struct.unpack("<" + "Q" * ((end - begin) // 8), read(begin, end - begin)))

    try:
        loaded = {}
        for name in (b".text", b".rdata"):
            section = next(s for s in sections if s[0].rstrip(b"\0") == name)
            if not 0 < section[1] <= 32 * 1024 * 1024:
                raise ValueError("Section size")
            loaded[name] = (section[2], read_process(handle, base + section[2], section[1]))
        text_rva, code = loaded[b".text"]
        data_rva, data = loaded[b".rdata"]
        owner_candidates = []
        for offset in (0x159A0, 0x159A8):
            pattern = b"\x48\x8b\x05....\x48\x8b\xb8" + re.escape(struct.pack("<I", offset))
            for match in re.finditer(pattern, code, re.DOTALL):
                at = match.start()
                global_rva = text_rva + at + 7 + struct.unpack_from("<i", code, at + 3)[0]
                owner_candidates.append((text_rva + at, global_rva, offset))
        if not 0 < len(owner_candidates) <= 8:
            raise ValueError("Owner access ambiguous")
        routine = {"bar": 0x3EF740, "ball": 0x3E4030, "hinge": 0x3F57C0, "ori": 0x3FC4B0}
        tables = {}
        for kind, rva in routine.items():
            found = [
                data_rva + m.start() - 0xA0
                for m in re.finditer(re.escape(struct.pack("<Q", base + rva)), data)
                if (data_rva + m.start()) % 8 == 0
            ]
            if not found or len(found) > 8:
                raise ValueError("Vtable candidates missing or ambiguous: " + kind)
            tables[kind] = found
        observations = []
        for instruction, global_rva, member in owner_candidates:
            try:
                owner = pointer(base + global_rva)
                player = pointer(owner + member)
                bodies = vector(player + 0x23F8)
                constraints = vector(player + 0x2410)
                if len(set(bodies)) != len(bodies) or len(set(constraints)) != len(constraints):
                    raise ValueError("Duplicate object links")
                kinds = collections.Counter()
                used = {}
                links = []
                for i, c in enumerate(constraints):
                    vt = pointer(c) - base
                    matches = [name for name, values in tables.items() if vt in values]
                    if len(matches) != 1:
                        raise ValueError("Unknown constraint type")
                    kind = matches[0]
                    used[kind] = hex(vt)
                    kinds[kind] += 1
                    pair = struct.unpack(
                        "<2Q",
                        read(c + (0xA0 if kind == "bar" else 0x30 if kind == "ori" else 0x98), 16),
                    )
                    if pair[0] == pair[1] or any(p not in bodies for p in pair):
                        raise ValueError("Constraint body ownership")
                    links.append(
                        dict(identity=i, kind=kind, body_indices=[bodies.index(p) for p in pair])
                    )
                if (
                    pointer(base + global_rva) != owner
                    or pointer(owner + member) != player
                    or vector(player + 0x23F8) != bodies
                    or vector(player + 0x2410) != constraints
                ):
                    raise ValueError("Topology changed during observation")
                observations.append(
                    dict(
                        instruction_rva=hex(instruction),
                        owner_global_rva=hex(global_rva),
                        player_member=hex(member),
                        root_bodies=len(bodies),
                        constraint_count=len(constraints),
                        kinds=dict(kinds),
                        vtables=used,
                        links=links,
                    )
                )
            except (ValueError, OSError) as error:
                observations.append(
                    dict(
                        instruction_rva=hex(instruction),
                        rejected=type(error).__name__ + ": " + str(error),
                    )
                )
        report = dict(
            steam_buildid="25661166",
            exe_sha256=hashlib.sha256(raw).hexdigest(),
            pid=args.pid,
            owner_candidates=observations,
            vtable_candidates={k: [hex(v) for v in vs] for k, vs in tables.items()},
            heap_bytes_read=heap_bytes,
            physics_writes=0,
            hook_loaded=False,
            physics_compatible=False,
            limitations="Sequential external topology observation, not a coherent physics snapshot or write authorization. Body-field, fuel/game-vehicle, scoring and subsystem layouts remain separate.",
        )
        (PROJECT / "compatibility/steam-25661166-topology.json").write_text(
            json.dumps(report, indent=2) + "\n"
        )
        print(json.dumps(report, indent=2))
    finally:
        k.CloseHandle(handle)


if __name__ == "__main__":
    main()
