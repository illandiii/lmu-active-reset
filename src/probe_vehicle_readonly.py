"""Bounded observations of code-derived candidate pointers. No writes or calls."""

import ctypes, hashlib, json, pathlib, struct, sys, urllib.request
from ctypes import wintypes

root = pathlib.Path(__file__).resolve().parents[1]
pid = int(sys.argv[1])
base = int(sys.argv[2], 0)
exe = pathlib.Path(r"D:\SteamLibrary\steamapps\common\Le Mans Ultimate\Le Mans Ultimate.exe")
if (
    hashlib.sha256(exe.read_bytes()).hexdigest()
    != "bb8661cf6f96a056374afac29018c1fdd71f86164c158a71043bb7c244f7407e"
):
    raise ValueError("Build refused")


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        raise ValueError("Redirect refused")


http = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect())


def context():
    with http.open("http://127.0.0.1:6397/navigation/state", timeout=2) as r:
        s = json.load(r)["state"]
    if (
        s.get("appBuild") != 14130
        or s.get("settingMode") != "SETTING_GRANDPRIX"
        or s.get("gameSession") not in ("PRACTICE1", "PRACTICE2", "PRACTICE3", "PRACTICE4")
        or s.get("gameState") != "GSTATE_DYN"
    ):
        raise ValueError("Context refused")
    return {k: s[k] for k in ("appBuild", "settingMode", "gameSession", "gameState")}


before = context()
k = ctypes.WinDLL("kernel32", use_last_error=True)
k.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
k.OpenProcess.restype = wintypes.HANDLE
k.ReadProcessMemory.argtypes = (
    wintypes.HANDLE,
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.c_size_t,
    ctypes.POINTER(ctypes.c_size_t),
)
k.ReadProcessMemory.restype = wintypes.BOOL
k.QueryFullProcessImageNameW.argtypes = (
    wintypes.HANDLE,
    wintypes.DWORD,
    wintypes.LPWSTR,
    ctypes.POINTER(wintypes.DWORD),
)
k.QueryFullProcessImageNameW.restype = wintypes.BOOL
k.CloseHandle.argtypes = (wintypes.HANDLE,)
k.GetTickCount64.argtypes = ()
k.GetTickCount64.restype = ctypes.c_ulonglong
h = k.OpenProcess(0x410, False, pid)
if not h:
    raise OSError(ctypes.get_last_error(), "Ordinary read denied; no fallback")
count_total = 0
try:
    path = ctypes.create_unicode_buffer(32768)
    length = wintypes.DWORD(len(path))
    if (
        not k.QueryFullProcessImageNameW(h, 0, path, ctypes.byref(length))
        or pathlib.Path(path.value) != exe
    ):
        raise ValueError("Wrong process")

    def read(address, size):
        global count_total
        if (
            not 0x10000 <= address < 0x800000000000
            or not 0 < size <= 256
            or count_total + size > 4096
        ):
            raise ValueError("Read bound refused")
        buf = ctypes.create_string_buffer(size)
        n = ctypes.c_size_t()
        if not k.ReadProcessMemory(h, address, buf, size, ctypes.byref(n)) or n.value != size:
            raise OSError(ctypes.get_last_error(), "Read incomplete; no fallback")
        count_total += size
        return buf.raw

    def pointer(address):
        return struct.unpack("<Q", read(address, 8))[0]

    if read(base, 64) != exe.read_bytes()[:64]:
        raise ValueError("Wrong base")
    owner_slot = base + 0x1DD94D8  # mov rax,[rip+0x124b222], next RIP 0xb8e2b6
    owner = pointer(owner_slot)
    vehicle = pointer(owner + 0x15978)  # seen passed to wheel-related routine in save path
    state = {
        "candidate_doubles_2498": struct.unpack("<3d", read(vehicle + 0x2498, 24)),
        "candidate_doubles_2550": struct.unpack("<3d", read(vehicle + 0x2550, 24)),
        "candidate_doubles_25c8": struct.unpack("<3d", read(vehicle + 0x25C8, 24)),
        "candidate_doubles_25e0": struct.unpack("<3d", read(vehicle + 0x25E0, 24)),
        "candidate_timestep_b0": struct.unpack("<d", read(vehicle + 0xB0, 8))[0],
    }
    component_ranges = {}
    for offset in (0x23F8, 0x2410, 0x2428, 0x2440):
        start, end = struct.unpack("<QQ", read(vehicle + offset, 16))
        if not start <= end or (end - start) % 8 or end - start > 64 * 8:
            raise ValueError("Component vector bounds refused")
        component_ranges[hex(offset)] = (end - start) // 8
        if offset == 0x2440 and end > start:
            body = pointer(start)
            state["body_candidate_position_10"] = struct.unpack("<3d", read(body + 0x10, 24))
            state["body_candidate_vector_28"] = struct.unpack("<3d", read(body + 0x28, 24))
            state["body_candidate_vector_40"] = struct.unpack("<3d", read(body + 0x40, 24))
    state["component_counts"] = component_ranges
    state["host_tick_ms"] = k.GetTickCount64()
    first = pointer(vehicle)
    state["first_word_is_module_pointer"] = base <= first < base + 63954944
    if state["first_word_is_module_pointer"]:
        state["first_word_rva"] = hex(first - base)
    if pointer(owner_slot) != owner or pointer(owner + 0x15978) != vehicle:
        raise ValueError("Pointer lifetime changed")
finally:
    k.CloseHandle(h)
after = context()
if before != after:
    raise ValueError("Context changed")
result = {
    "context": after,
    "bytes_read": count_total,
    "physics_writes": 0,
    "candidate_observations": state,
    "limitations": "Code-derived candidate pointer only. Not proven authoritative, synchronized, or restorable. Numeric values must be compared with live telemetry.",
}
(root / "vehicle-pointer-probe.json").write_text(
    json.dumps(result, indent=2, allow_nan=False) + "\n"
)
print(json.dumps(result, indent=2, allow_nan=False))
