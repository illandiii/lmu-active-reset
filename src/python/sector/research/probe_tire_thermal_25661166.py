"""Bounded topology study of native tire thermal layers. No native calls/writes.
Values are asynchronous and are never accepted as a coherent checkpoint.
"""

import argparse
import json
import struct
import math
import datetime
from sector.telemetry.capture_preparation_steam25661166 import verify_telemetry
from sector.research.probe_contacts_14200 import context
from sector.telemetry.capture_context import PROJECT
from sector.observers.run_physics_observer import modules, k, read_process


def probe(pid):
    verify_telemetry(pid)
    context()
    loaded = modules(pid)
    if any("easyanti" in m["name"].lower() for m in loaded):
        raise ValueError("EAC present")
    base = next(m["base"] for m in loaded if m["name"].lower() == "le mans ultimate.exe")
    h = k.OpenProcess(0x410, False, pid)
    total = 0
    if not h:
        raise OSError("Read access failed")

    def read(address, n):
        nonlocal total
        if not 0x10000 <= address < 0x800000000000 - n or not 0 < n <= 128 or total + n > 400000:
            raise ValueError("Read bounds")
        total += n
        return read_process(h, address, n)

    def ptr(address):
        return struct.unpack("<Q", read(address, 8))[0]

    wheels = []
    try:
        owner = ptr(base + 0x1E194C8)
        vehicle = ptr(owner + 0x159A8)
        for w in range(4):
            tire = ptr(vehicle + 0x618 + w * 0x6B0 + 0x3E8)
            pool = ptr(tire + 0x318)
            inner = ptr(tire + 0xA10)
            outer = ptr(tire + 0xA18)
            table = ptr(tire + 0xA28)
            raw_indices = [read(tire + 0x10 + i * 0x60, 18) for i in range(8)]
            sectors = [list(struct.unpack("<9H", x)) for x in raw_indices]
            indices = sorted(set(x for s in sectors for x in s))
            if not indices or len(indices) > 72 or max(indices) >= 256:
                raise ValueError("Unknown thermal topology")
            states = []
            for index in indices:
                # EFA860 is called on seven +110 nodes and two +60 thermal cells.
                expected = [pool + index * 0x770 + j * 0x110 for j in range(7)] + [
                    inner + index * 0x60,
                    outer + index * 0x60,
                ]
                pointers = list(struct.unpack("<9Q", read(table + index * 72, 72)))
                if len(set(pointers)) != 9 or set(expected) != set(pointers):
                    raise ValueError(
                        "Thermal reference table does not match the three native pools"
                    )
                values = []
                for cell in pointers:
                    layers = struct.unpack("<5d", read(cell, 40))
                    coeff = struct.unpack("<4d", read(cell + 0x40, 32))
                    if not all(math.isfinite(x) for x in layers + coeff):
                        raise ValueError("Nonfinite thermal state")
                    values.append(
                        {"pool": expected.index(cell), "layers": layers, "coefficients": coeff}
                    )
                if list(struct.unpack("<9Q", read(table + index * 72, 72))) != pointers:
                    raise ValueError("Thermal table changed")
                states.append({"angular_index": index, "cells": values})
            if [read(tire + 0x10 + i * 0x60, 18) for i in range(8)] != raw_indices or [
                ptr(tire + x) for x in (0x318, 0xA10, 0xA18, 0xA28)
            ] != [pool, inner, outer, table]:
                raise ValueError("Thermal topology changed")
            if ptr(vehicle + 0x618 + w * 0x6B0 + 0x3E8) != tire:
                raise ValueError("Tire owner changed")
            wheels.append(
                {
                    "wheel": w,
                    "sector_indices": sectors,
                    "angular_count": len(indices),
                    "states": states,
                }
            )
        if ptr(base + 0x1E194C8) != owner or ptr(owner + 0x159A8) != vehicle:
            raise ValueError("Player changed")
        context()
        result = {
            "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "wheels": wheels,
            "bytes_read": total,
            "physics_writes": 0,
            "coherent_checkpoint": False,
            "limits": "Asynchronous read-only topology study. Five layers and four coefficients only; other node history, wear and full tire state not covered.",
        }
        (PROJECT / "compatibility/steam-25661166-tire-thermal.json").write_text(
            json.dumps(result, indent=2) + "\n"
        )
        print(
            json.dumps(
                {
                    "wheels": [
                        {
                            "wheel": w["wheel"],
                            "angular_count": w["angular_count"],
                            "thermal_cells": len(w["states"]) * 9,
                        }
                        for w in wheels
                    ],
                    "bytes_read": total,
                    "physics_writes": 0,
                },
                indent=2,
            )
        )
    finally:
        k.CloseHandle(h)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--pid", type=int, required=True)
    probe(p.parse_args().pid)
