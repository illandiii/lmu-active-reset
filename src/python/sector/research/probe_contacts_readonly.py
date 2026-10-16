"""Bounded observation of candidate tire contacts in offline Practice, build 14150.

Research only: no DLL, native calls, physics writes or input. Each output is an
asynchronous observation, never a checkpoint or a reconstructed contact set.
Offsets are derived from the 14150 routines documented in CONTACT_REBUILD.md.
"""

import argparse
import ctypes
from ctypes import wintypes as W
import datetime
import json
import math
import struct
from sector.telemetry.capture_preparation import verify_telemetry
from sector.telemetry.capture_context import PROJECT, KERNEL, get_json
from sector.observers.run_physics_observer import modules

MAX_CONTACTS = 512
NODE_STRIDE = 0x110
SURFACE_STRIDE = 0x58


def vector_count(raw, stride, limit):
    begin, end, capacity = struct.unpack("<3Q", raw)
    if begin == end == capacity == 0:
        return 0
    if (
        begin < 0x10000
        or not begin <= end <= capacity
        or (end - begin) % stride
        or (capacity - begin) % stride
        or (end - begin) // stride > limit
    ):
        raise ValueError("Vector bounds refused")
    return (end - begin) // stride


def surface_link(index, count, node_surface, cached_surface, used):
    if not 0 <= index < count:
        raise ValueError("Contact surface index outside cache")
    return {
        "surface_cache_index": index,
        "surface_cache_matches": node_surface != 0 and node_surface == cached_surface,
        "surface_cache_used_bit": bool(used),
    }


def pool_index(address, pool):
    """Bounded index only, not proof of allocation size or object lifetime."""
    delta = address - pool
    if pool < 0x10000 or delta < 0 or delta % NODE_STRIDE or delta // NODE_STRIDE >= 32768:
        raise ValueError("Contact pointer does not fit the candidate node pool")
    return delta // NODE_STRIDE


def describe_node(index, flags, values, surface_present):
    if len(flags) != 2 or len(values) != 5 or not all(math.isfinite(x) for x in values):
        raise ValueError("Non-finite or malformed candidate contact")
    depth, *tail = values
    normal = tail[:3]
    plane = tail[3]
    return {
        "pool_index": index,
        "active_flag_60": flags[0],
        "flag_67": flags[1],
        "candidate_signed_depth_d0": depth,
        "candidate_normal_d8": normal,
        "candidate_plane_f0": plane,
        "surface_reference_present": bool(surface_present),
        "normal_length": math.hypot(*normal),
    }


def context():
    state = get_json("/navigation/state")["state"]
    game = get_json("/rest/sessions/GetGameState")
    if (
        state.get("appBuild") != 14150
        or state.get("settingMode") != "SETTING_GRANDPRIX"
        or state.get("gameSession") not in ("PRACTICE1", "PRACTICE2", "PRACTICE3", "PRACTICE4")
        or state.get("gameState") != "GSTATE_DYN"
        or state.get("navigationState") not in ("NAV_EVENT", "NAV_REALTIME")
        or game.get("playerVehicleLoaded") is not True
        or game.get("isReplayActive") is not False
    ):
        raise ValueError("Active offline Practice with loaded player vehicle required")
    return {
        "navigation": {
            key: state.get(key)
            for key in (
                "appBuild",
                "settingMode",
                "gameSession",
                "gameState",
                "navigationState",
                "gamePhase",
            )
        },
        "game": {
            key: game.get(key)
            for key in ("inMonitor", "playerVehicleLoaded", "isReplayActive", "MultiStintState")
        },
    }


def probe(pid):
    verify_telemetry(pid)
    before = context()
    candidates = [m for m in modules(pid) if m["name"].lower() == "le mans ultimate.exe"]
    if len(candidates) != 1:
        raise ValueError("Ambiguous executable module")
    base = candidates[0]["base"]
    KERNEL.ReadProcessMemory.argtypes = (
        W.HANDLE,
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t),
    )
    KERNEL.ReadProcessMemory.restype = W.BOOL
    handle = KERNEL.OpenProcess(0x410, False, pid)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    total = 0
    wheels = []
    lifetimes = []
    try:

        def read(address, size):
            nonlocal total
            if (
                not 0x10000 <= address < 0x800000000000
                or not 0 < size <= 4096
                or total + size > 512 * 1024
            ):
                raise ValueError("Contact observation read bounds exceeded")
            buffer = ctypes.create_string_buffer(size)
            got = ctypes.c_size_t()
            if (
                not KERNEL.ReadProcessMemory(handle, address, buffer, size, ctypes.byref(got))
                or got.value != size
            ):
                raise ctypes.WinError(ctypes.get_last_error())
            total += size
            return buffer.raw

        def ptr(address):
            return struct.unpack("<Q", read(address, 8))[0]

        # Exact 14150 SaveTemplate caller: instruction 0xB9819F, RIP next
        # 0xB981A6 + displacement 0x124E9E2 = owner slot 0x1DE6B88.
        owner = ptr(base + 0x1DE6B88)
        vehicle = ptr(owner + 0x15978)
        for wheel_index in range(4):
            wheel = vehicle + 0x618 + wheel_index * 0x6B0
            tire = ptr(wheel + 0x3E8)
            pool = ptr(tire + 0x318)
            vector = read(tire + 0x9F0, 24)
            begin, end, capacity = struct.unpack("<3Q", vector)
            count = vector_count(vector, 8, MAX_CONTACTS)
            surfaces = read(tire + 0xA60, 24)
            surface_count = vector_count(surfaces, SURFACE_STRIDE, 1024)
            surface_begin = struct.unpack_from("<Q", surfaces)[0]
            used_vector = read(tire + 0xA78, 24)
            used_count = vector_count(used_vector, 4, 1024)
            used_begin = struct.unpack_from("<Q", used_vector)[0]
            used_bits = read(used_begin, used_count * 4) if used_count else b""
            raw_nodes = read(begin, count * 8) if count else b""
            nodes = struct.unpack("<" + "Q" * count, raw_nodes)
            if len(set(nodes)) != count:
                raise ValueError("Duplicate active-node reference")
            rows = []
            for node in nodes:
                index = pool_index(node, pool)
                flags = read(node + 0x60, 8)
                values = struct.unpack("<5d", read(node + 0xD0, 40))
                node_surface = ptr(node + 0xF8)
                cached_index = struct.unpack_from("<H", flags, 2)[0]
                if cached_index >= surface_count or cached_index // 32 >= used_count:
                    raise ValueError("Contact surface cache index or bitset outside bounds")
                cached_surface = ptr(surface_begin + cached_index * SURFACE_STRIDE)
                used = struct.unpack_from("<I", used_bits, 4 * (cached_index // 32))[0] & (
                    1 << (cached_index % 32)
                )
                row = describe_node(index, (flags[0], flags[7]), values, node_surface != 0)
                row.update(
                    surface_link(cached_index, surface_count, node_surface, cached_surface, used)
                )
                rows.append(row)
            position = struct.unpack("<3d", read(tire + 0xAC0 + 0x40, 24))
            if not all(math.isfinite(x) for x in position):
                raise ValueError("Invalid tire-body position")
            wheels.append(
                {
                    "wheel_index": wheel_index,
                    "contact_count": count,
                    "surface_cache_count": surface_count,
                    "tire_body_position": position,
                    "contacts": rows,
                }
            )
            lifetimes.append((wheel, tire, pool, vector, begin, raw_nodes, surfaces, used_vector))
        for wheel, tire, pool, vector, begin, raw_nodes, surfaces, used_vector in lifetimes:
            if (
                ptr(wheel + 0x3E8) != tire
                or ptr(tire + 0x318) != pool
                or read(tire + 0x9F0, 24) != vector
            ):
                raise ValueError("Contact owner/vector changed during observation")
            if raw_nodes and read(begin, len(raw_nodes)) != raw_nodes:
                raise ValueError("Active contact membership changed during observation")
            if read(tire + 0xA60, 24) != surfaces or read(tire + 0xA78, 24) != used_vector:
                raise ValueError("Surface cache allocation changed during observation")
        if ptr(base + 0x1DE6B88) != owner or ptr(owner + 0x15978) != vehicle:
            raise ValueError("Player owner changed during observation")
    finally:
        KERNEL.CloseHandle(handle)
    if context() != before:
        raise ValueError("Context changed during observation")
    return {
        "context": before,
        "bytes_read": total,
        "wheels": wheels,
        "limitations": "Asynchronous candidate fields; stable pointers do not prove atomic values, absence of ABA, valid reconstruction, or complete tire state.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    args = parser.parse_args()
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    report = {
        "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "build": 14150,
        "physics_writes": 0,
        "native_calls": 0,
        "hook_loaded": False,
        "reset_operations": 0,
        "success": False,
    }
    try:
        report.update(probe(args.pid))
        report["success"] = True
    except Exception as exc:
        report["error"] = f"{type(exc).__name__}: {exc}"
    destination = PROJECT / "logs" / f"{stamp}_contact_probe.json"
    destination.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "wheels"}, indent=2))
    if report["success"]:
        print("contacts_per_wheel", [w["contact_count"] for w in report["wheels"]])
    return 0 if report["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
