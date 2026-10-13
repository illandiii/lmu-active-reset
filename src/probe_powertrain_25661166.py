"""Bounded pointer/virtual-entry observations; never invokes native routines."""

import argparse, json, struct, sys
from pathlib import Path
from capture_preparation_steam25661166 import verify_telemetry
from capture_context import PROJECT, get_json
from run_physics_observer import modules, k, read_process

sys.path.insert(0, str(PROJECT / "work/pydeps"))
import capstone


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--pid", type=int, required=True)
    a = p.parse_args()
    verify_telemetry(a.pid)
    state = get_json("/navigation/state")["state"]
    if state.get("settingMode") != "SETTING_GRANDPRIX" or not state.get(
        "gameSession", ""
    ).startswith("PRACTICE"):
        raise ValueError("Offline Practice required")
    loaded = modules(a.pid)
    if any("easyanti" in m["name"].lower() for m in loaded):
        raise ValueError("EAC present")
    image = next(m for m in loaded if m["name"].lower() == "le mans ultimate.exe")
    base = image["base"]
    raw = Path(image["path"]).read_bytes()
    pe = struct.unpack_from("<I", raw, 0x3C)[0]
    size = struct.unpack_from("<I", raw, pe + 24 + 56)[0]
    h = k.OpenProcess(0x410, False, a.pid)
    total = 0
    if not h:
        raise OSError("Read access failed")

    def read(addr, n):
        nonlocal total
        if not 0x10000 <= addr < 0x800000000000 or n > 128 or total + n > 8192:
            raise ValueError("Read budget")
        total += n
        return read_process(h, addr, n)

    def ptr(addr):
        return struct.unpack("<Q", read(addr, 8))[0]

    try:
        owner = ptr(base + 0x1E194C8)
        vehicle = ptr(owner + 0x159A8)
        engine = ptr(vehicle + 0x46F0)
        vt = ptr(engine)
        if not base <= vt < base + size:
            raise ValueError("Vtable outside image")
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        getters = []
        for slot in (0x38, 0x58, 0xA8):
            address = ptr(vt + slot)
            if not base <= address < base + size - 64:
                raise ValueError("Getter outside image")
            ins = []
            for at, n, m, o in md.disasm_lite(read(address, 64), address - base):
                ins.append(dict(rva=hex(at), instruction=m + " " + o))
                if m == "ret":
                    break
            getters.append(dict(slot=hex(slot), instructions=ins))
        result = dict(
            engine_vtable_rva=hex(vt - base),
            getters=getters,
            engine_candidates={
                hex(o): struct.unpack("<d", read(engine + o, 8))[0] for o in (0x1F8, 0x250, 0x280)
            },
            hdv_persistent={
                hex(o): struct.unpack("<d", read(vehicle + o, 8))[0]
                for o in (0x48A0, 0x48A8, 0x48B0, 0x48B8, 0x48E8, 0x48F0, 0x48F8)
            },
        )
        entries = {}
        for offset in range(0, 0xC0, 8):
            entry = ptr(vt + offset)
            if not base <= entry < base + size:
                raise ValueError("Virtual entry outside image")
            entries[hex(offset)] = hex(entry - base)
        result["virtual_entries"] = entries
        if (
            ptr(base + 0x1E194C8) != owner
            or ptr(owner + 0x159A8) != vehicle
            or ptr(vehicle + 0x46F0) != engine
        ):
            raise ValueError("Lifetime changed")
        result.update(
            bytes_read=total,
            physics_writes=0,
            limits="Asynchronous observations; candidate semantics and full powertrain coverage not certified.",
        )
        (PROJECT / "compatibility/steam-25661166-powertrain.json").write_text(
            json.dumps(result, indent=2) + "\n"
        )
        print(json.dumps(result, indent=2))
    finally:
        k.CloseHandle(h)


if __name__ == "__main__":
    main()
