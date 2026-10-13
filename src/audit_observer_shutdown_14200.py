"""Verify resident experimental modules are stopped and native prologs restored. Read-only."""

import argparse, ctypes as C, datetime, hashlib, importlib, json
from pathlib import Path
from ctypes import wintypes as W
from capture_context import PROJECT
from capture_preparation_14200 import verify_telemetry
from run_physics_observer import k, api, modules, read_process

REGISTRY = [
    (
        "lmu-vehicle-observer-14200-v4.dll",
        "14200-vehicle-observer-v4.json",
        "run_vehicle_observer_v4_14200",
        "LMU_VehicleObserver14200_v4_",
        0x4C4D555645483234,
        "allow_observation",
    ),
    (
        "lmu-ori-observer-14200.dll",
        "14200-ori-observer.json",
        "run_ori_observer_14200",
        "LMU_OriObserver14200_v1_",
        0x4C4D554F52493231,
        "allow_observation",
    ),
    (
        "lmu-hinge-observer-14200.dll",
        "14200-hinge-observer.json",
        "run_hinge_observer_14200",
        "LMU_HingeObserver14200_v1_",
        0x4C4D5548494E3231,
        "allow_observation",
    ),
    (
        "lmu-ball-observer-14200.dll",
        "14200-ball-observer.json",
        "run_ball_observer_14200",
        "LMU_BallObserver14200_v1_",
        0x4C4D5542414C3231,
        "allow_observation",
    ),
    (
        "lmu-contact-native-14200.dll",
        "14200-contact-experiment.json",
        "run_contact_14200",
        "LMU_Contact14200_v1_",
        0x4C4D55434F4E5431,
        "allow_rebuild",
    ),
    (
        "lmu-vehicle-observer-14200.dll",
        "14200-vehicle-observer.json",
        "run_vehicle_observer_14200",
        "LMU_VehicleObserver14200_v1_",
        0x4C4D555645483231,
        "allow_observation",
    ),
    (
        "lmu-vehicle-observer-14200-v2.dll",
        "14200-vehicle-observer-v2.json",
        "run_vehicle_observer_v2_14200",
        "LMU_VehicleObserver14200_v2_",
        0x4C4D555645483232,
        "allow_observation",
    ),
    (
        "lmu-vehicle-observer-14200-v3.dll",
        "14200-vehicle-observer-v3.json",
        "run_vehicle_observer_v3_14200",
        "LMU_VehicleObserver14200_v3_",
        0x4C4D555645483233,
        "allow_observation",
    ),
    (
        "lmu-bar-observer-14200.dll",
        "14200-bar-observer.json",
        "run_bar_observer_14200",
        "LMU_BarObserver14200_v1_",
        0x4C4D554241523231,
        "allow_observation",
    ),
    (
        "lmu-bar-observer-14200-v2.dll",
        "14200-bar-observer-v2.json",
        "run_bar_observer_v2_14200",
        "LMU_BarObserver14200_v2_",
        0x4C4D554241523232,
        "allow_observation",
    ),
    (
        "lmu-bar-observer-14200-v3.dll",
        "14200-bar-observer-v3.json",
        "run_bar_observer_v3_14200",
        "LMU_BarObserver14200_v3_",
        0x4C4D554241523233,
        "allow_observation",
    ),
]


def audit(pid):
    verify_telemetry(pid)
    listed = modules(pid)
    known = {r[0] for r in REGISTRY}
    unknown = [
        m["name"]
        for m in listed
        if m["name"].lower().startswith("lmu-") and m["name"].lower() not in known
    ]
    if unknown:
        raise ValueError("Unknown resident project modules: " + repr(unknown))
    api("OpenFileMappingW", [W.DWORD, W.BOOL, W.LPCWSTR], W.HANDLE)
    reports = {}
    targets = {}
    for name, manifest, module, mapping_prefix, magic, allow in REGISTRY:
        resident = [m for m in listed if m["name"].lower() == name]
        if not resident:
            continue
        path = (PROJECT / "bin" / name).resolve()
        profile = json.loads((PROJECT / "compatibility" / manifest).read_text())
        if (
            len(resident) != 1
            or Path(resident[0]["path"]) != path
            or hashlib.sha256(path.read_bytes()).hexdigest() != profile["dll_sha256"]
        ):
            raise ValueError("Module identity mismatch: " + name)
        IPC = importlib.import_module(module).IPC
        mapping = k.OpenFileMappingW(4, False, f"Local\\{mapping_prefix}{pid}")
        view = None
        try:
            if not mapping:
                raise ValueError("Missing module IPC: " + name)
            view = k.MapViewOfFile(mapping, 4, 0, 0, C.sizeof(IPC))
            if not view:
                raise ValueError("Unreadable module IPC: " + name)
            s = IPC.from_buffer_copy(C.string_at(view, C.sizeof(IPC)))
            if (
                s.magic != magic
                or s.pid != pid
                or s.version != 1
                or s.status != 3
                or s.error
                or s.stop != 1
                or getattr(s, allow) != 0
            ):
                raise ValueError("Module not cleanly stopped: " + name)
            reports[name] = dict(
                status=s.status,
                error=s.error,
                stop=s.stop,
                allow=0,
                dll_sha256=profile["dll_sha256"],
            )
        finally:
            if view:
                k.UnmapViewOfFile(view)
            if mapping:
                k.CloseHandle(mapping)
        entries = profile.get(
            "targets",
            [dict(rva=int(profile["target_rva"], 0), prolog=profile["prolog_hex"], name="Contacts")]
            if "target_rva" in profile
            else [],
        )
        for t in entries:
            raw = bytes.fromhex(t["prolog"])
            rva = t["rva"]
            if rva in targets and targets[rva][1] != raw:
                raise ValueError("Conflicting prolog manifests")
            targets[rva] = (t["name"], raw)
    base = next(m["base"] for m in listed if m["name"].lower() == "le mans ultimate.exe")
    handle = k.OpenProcess(0x410, False, pid)
    if not handle:
        raise OSError("Cannot inspect native instructions")
    try:
        checks = {
            name: read_process(handle, base + rva, len(raw)) == raw
            for rva, (name, raw) in targets.items()
        }
        if not all(checks.values()):
            raise ValueError("Native instruction mismatch")
    finally:
        k.CloseHandle(handle)
    return dict(
        utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        pid=pid,
        modules=reports,
        native_prologs=checks,
        physics_writes=0,
    )


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--pid", type=int, required=True)
    a = p.parse_args()
    result = audit(a.pid)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    (PROJECT / "logs" / f"{stamp}_shutdown_audit_14200.json").write_text(
        json.dumps(result, indent=2) + "\n"
    )
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
