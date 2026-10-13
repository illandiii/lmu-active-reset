"""Read only code-derived rigid-body fields in an offline Practice monitor.

This is an asynchronous layout check, never a restorable checkpoint.
No object bytes or process addresses are saved to the output.
"""

import argparse, ctypes, datetime, json, math, pathlib, struct
from ctypes import wintypes as W
from capture_context import verify, get_json, KERNEL, PROJECT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--base", type=lambda x: int(x, 0), required=True)
    args = parser.parse_args()
    verify(args.pid)

    def context():
        state = get_json("/navigation/state")["state"]
        keys = ("appBuild", "settingMode", "gameSession", "gameState", "navigationState")
        if (
            state.get("appBuild") != 14130
            or state.get("settingMode") != "SETTING_GRANDPRIX"
            or state.get("gameSession") not in ("PRACTICE1", "PRACTICE2", "PRACTICE3", "PRACTICE4")
            or state.get("navigationState") != "NAV_EVENT"
        ):
            raise ValueError("Only verified offline Practice monitor accepted")
        return {k: state.get(k) for k in keys}

    before = context()
    KERNEL.ReadProcessMemory.argtypes = (
        W.HANDLE,
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t),
    )
    KERNEL.ReadProcessMemory.restype = W.BOOL
    handle = KERNEL.OpenProcess(0x410, False, args.pid)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    total = 0
    try:

        def read(address, size):
            nonlocal total
            if (
                not 0x10000 <= address < 0x800000000000
                or not 0 < size <= 512
                or total + size > 16384
            ):
                raise ValueError("Read bounds refused")
            buf = ctypes.create_string_buffer(size)
            n = ctypes.c_size_t()
            if (
                not KERNEL.ReadProcessMemory(handle, address, buf, size, ctypes.byref(n))
                or n.value != size
            ):
                raise ctypes.WinError(ctypes.get_last_error())
            total += size
            return buf.raw

        def pointer(address):
            return struct.unpack("<Q", read(address, 8))[0]

        inv = json.loads((PROJECT / "inventory.json").read_text(encoding="utf-8-sig"))
        with (pathlib.Path(inv["install_path"]) / "Le Mans Ultimate.exe").open("rb") as f:
            header = f.read(64)
        if read(args.base, 64) != header:
            raise ValueError("Module base mismatch")
        owner = pointer(args.base + 0x1DD94D8)
        vehicle = pointer(owner + 0x15978)
        vectors = {}
        for offset in (0x23F8, 0x2440):
            begin, end = struct.unpack("<QQ", read(vehicle + offset, 16))
            if not 0 < end - begin <= 32 * 8 or (end - begin) % 8:
                raise ValueError("Component count refused")
            pointers = struct.unpack("<" + "Q" * ((end - begin) // 8), read(begin, end - begin))
            if len(set(pointers)) != len(pointers):
                raise ValueError("Duplicate component")
            vectors[offset] = (begin, end, pointers)
        components = vectors[0x23F8][2]
        if not set(vectors[0x2440][2]).issubset(components):
            raise ValueError("Aggregate bodies outside integration list")
        bodies = []
        for index, ptr in enumerate(components):
            # Explicit numerical fields only; no vtable, links, handles or padding.
            fields = {}
            for name, offset, count in (
                ("force", 0x10, 3),
                ("world_velocity", 0x28, 3),
                ("world_position", 0x40, 3),
                ("inverse_mass", 0x58, 1),
                ("world_inverse_inertia_symmetric", 0x60, 6),
                ("orientation_row_major", 0x90, 9),
                ("world_angular_velocity", 0xE0, 3),
                ("local_inverse_inertia_diagonal", 0xF8, 3),
                ("world_angular_momentum", 0x110, 3),
                ("torque", 0x128, 3),
                ("local_inertia_diagonal", 0x140, 3),
            ):
                values = struct.unpack("<" + "d" * count, read(ptr + offset, count * 8))
                if not all(math.isfinite(x) for x in values):
                    raise ValueError("Non-finite body field")
                fields[name] = list(values)
            r = fields["orientation_row_major"]
            d = fields["local_inverse_inertia_diagonal"]
            expected = [
                sum(r[i * 3 + k] * d[k] * r[j * 3 + k] for k in range(3))
                for i, j in ((0, 0), (0, 1), (1, 1), (0, 2), (1, 2), (2, 2))
            ]
            measured = fields["world_inverse_inertia_symmetric"]
            ortho = max(
                abs(sum(r[i * 3 + k] * r[j * 3 + k] for k in range(3)) - (i == j))
                for i in range(3)
                for j in range(3)
            )
            determinant = (
                r[0] * (r[4] * r[8] - r[5] * r[7])
                - r[1] * (r[3] * r[8] - r[5] * r[6])
                + r[2] * (r[3] * r[7] - r[4] * r[6])
            )
            bodies.append(
                {
                    "index": index,
                    "contributes_to_vehicle_aggregate": ptr in vectors[0x2440][2],
                    "fields": fields,
                    "algebra_checks": {
                        "rotation_orthogonality_max_error": ortho,
                        "rotation_determinant": determinant,
                        "world_inverse_inertia_max_error": max(
                            abs(a - b) for a, b in zip(expected, measured)
                        ),
                        "local_inertia_reciprocal_max_error": max(
                            abs(a * b - 1) for a, b in zip(d, fields["local_inertia_diagonal"])
                        ),
                    },
                }
            )
        for offset, (begin, end, pointers) in vectors.items():
            if (
                struct.unpack("<QQ", read(vehicle + offset, 16)) != (begin, end)
                or struct.unpack("<" + "Q" * len(pointers), read(begin, end - begin)) != pointers
            ):
                raise ValueError("Component lifetime changed")
        if pointer(args.base + 0x1DD94D8) != owner or pointer(owner + 0x15978) != vehicle:
            raise ValueError("Vehicle lifetime changed")
    finally:
        KERNEL.CloseHandle(handle)
    after = context()
    if before != after:
        raise ValueError("Context changed during read")
    result = {
        "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "context": after,
        "bytes_read": total,
        "physics_writes": 0,
        "component_count": len(bodies),
        "aggregate_body_count": len(vectors[0x2440][2]),
        "bodies": bodies,
        "limitations": "Asynchronous monitor readings. Field meanings inferred from verified integration arithmetic; no restore, contact reconstruction, or lifetime guarantee.",
    }
    destination = PROJECT / "rigid-body-layout-observation.json"
    destination.write_text(json.dumps(result, indent=2, allow_nan=False) + "\n")
    print(json.dumps({k: v for k, v in result.items() if k != "bodies"}, indent=2))


if __name__ == "__main__":
    main()
