"""Bounded asynchronous wheel-state research. Read-only; never a checkpoint.

Offsets below are derived from build 14200 code, not a relocated old profile.
Stable double reads reduce tearing; they do not establish a simulation boundary.
"""

import argparse, ctypes, datetime, json, math, struct, time
from ctypes import wintypes as W
from capture_context import PROJECT, KERNEL, get_json
from capture_preparation_steam25661166 import verify_telemetry
from run_physics_observer import modules

OWNER_SLOT = 0x1E194C8
VEHICLE_OFFSET = 0x159A8


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--pid", type=int, required=True)
    p.add_argument("--seconds", type=int, default=30)
    a = p.parse_args()
    if not 1 <= a.seconds <= 120:
        p.error("Duration must be 1..120 seconds")
    verify_telemetry(a.pid)
    loaded = modules(a.pid)
    if any("easyanti" in m["name"].lower() for m in loaded):
        raise ValueError("EAC module present")
    main = [m for m in loaded if m["name"].lower() == "le mans ultimate.exe"]
    if len(main) != 1:
        raise ValueError("Ambiguous module")
    base = main[0]["base"]
    KERNEL.ReadProcessMemory.argtypes = (
        W.HANDLE,
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t),
    )
    KERNEL.ReadProcessMemory.restype = W.BOOL
    h = KERNEL.OpenProcess(0x410, False, a.pid)
    if not h:
        raise ctypes.WinError(ctypes.get_last_error())
    total = 0
    rows = []
    rejected = 0

    def read(addr, n):
        nonlocal total
        if not 0x10000 <= addr < 0x800000000000 or not 0 < n <= 256 or total + n > 16 * 1024 * 1024:
            raise ValueError("Read budget")
        b = ctypes.create_string_buffer(n)
        got = ctypes.c_size_t()
        if not KERNEL.ReadProcessMemory(h, addr, b, n, ctypes.byref(got)) or got.value != n:
            raise ctypes.WinError(ctypes.get_last_error())
        total += n
        return b.raw

    def ptr(addr):
        return struct.unpack("<Q", read(addr, 8))[0]

    def doubles(addr, n):
        v = struct.unpack("<" + "d" * n, read(addr, n * 8))
        if not all(math.isfinite(x) for x in v):
            raise ValueError("Nonfinite numeric state")
        return v

    def context():
        s = get_json("/navigation/state")["state"]
        g = get_json("/rest/sessions/GetGameState")
        if (
            s.get("appBuild") != 14200
            or s.get("settingMode") != "SETTING_GRANDPRIX"
            or s.get("gameSession") not in ("PRACTICE1", "PRACTICE2", "PRACTICE3", "PRACTICE4")
            or s.get("gameState") != "GSTATE_DYN"
            or g.get("isReplayActive") is not False
        ):
            raise ValueError("Context changed")
        return tuple(
            s.get(k)
            for k in ("appBuild", "settingMode", "gameSession", "gameState", "navigationState")
        )

    try:
        # Check the exact live owner-chain instruction before any heap reads.
        if read(base + 0xBA5C0F, 14) != bytes.fromhex("488b05b2382701488bb8a8590100"):
            raise ValueError("Owner code mismatch")
        deadline = time.monotonic() + a.seconds
        while time.monotonic() < deadline:
            before = context()
            owner = ptr(base + OWNER_SLOT)
            vehicle = ptr(owner + VEHICLE_OFFSET)
            rear = ptr(vehicle + 0x20D8)
            for i in range(4):
                wheel = vehicle + 0x618 + i * 0x6B0
                body = ptr(wheel + 0x1D0)
                hub = rear if i >= 2 and rear else ptr(wheel + 0x1D8)

                def numeric():
                    return (
                        read(wheel + 0x301, 2),
                        doubles(wheel + 0x120, 2),
                        doubles(hub + 0x90, 9),
                        doubles(hub + 0xE0, 3),
                        doubles(body + 0x90, 9),
                        doubles(body + 0xE0, 3),
                    )

                first = numeric()
                second = numeric()
                if (
                    first != second
                    or ptr(wheel + 0x1D0) != body
                    or (rear if i >= 2 and rear else ptr(wheel + 0x1D8)) != hub
                ):
                    rejected += 1
                    continue
                flags, spin, r, w, observed_r, observed_w = first
                if flags != b"\0\0":
                    rejected += 1
                    continue
                angle, rate = spin
                s = math.sin(angle)
                c = math.cos(angle)
                predicted = []
                for j in range(3):
                    predicted.extend(
                        (
                            r[3 * j],
                            c * r[3 * j + 1] + s * r[3 * j + 2],
                            c * r[3 * j + 2] - s * r[3 * j + 1],
                        )
                    )
                predicted_w = [w[j] + rate * r[3 * j] for j in range(3)]
                rows.append(
                    dict(
                        host_tick_ms=KERNEL.GetTickCount64(),
                        wheel=i,
                        angle=angle,
                        rate=rate,
                        hub_orientation=r,
                        hub_omega=w,
                        wheel_orientation=observed_r,
                        wheel_omega=observed_w,
                        rotation_error=max(abs(x - y) for x, y in zip(predicted, observed_r)),
                        omega_error=max(abs(x - y) for x, y in zip(predicted_w, observed_w)),
                    )
                )
            if (
                ptr(base + OWNER_SLOT) != owner
                or ptr(owner + VEHICLE_OFFSET) != vehicle
                or ptr(vehicle + 0x20D8) != rear
                or before != context()
            ):
                raise ValueError("Lifetime/context changed")
            time.sleep(0.1)
    finally:
        KERNEL.CloseHandle(h)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    dest = PROJECT / "logs" / f"{stamp}_wheel_binding_25661166.json"
    result = dict(
        build=14200,
        bytes_read=total,
        physics_writes=0,
        hook_loaded=False,
        stable_double_reads=len(rows),
        rejected_reads=rejected,
        rows=rows,
        limits="Asynchronous research only; matching double reads do not prove coherent checkpoint capture. Damaged/detached wheels excluded.",
    )
    dest.write_text(json.dumps(result, indent=2, allow_nan=False) + "\n")
    print(json.dumps({**{k: v for k, v in result.items() if k != "rows"}, "output": str(dest)}))


if __name__ == "__main__":
    main()
