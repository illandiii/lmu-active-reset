"""Verify stopped resident snapshot modules and original Dynamics instructions."""

import ctypes as C, hashlib, importlib, json
import re
from ctypes import wintypes as W
from pathlib import Path
from capture_context import PROJECT
from capture_preparation_steam25661166 import verify_telemetry
from run_physics_observer import modules, k, api, read_process


def audit(pid):
    verify_telemetry(pid)
    listed = modules(pid)
    reports = {}
    registry = {
        "lmu-physical-reset-v8-25661166.dll": (
            "run_physical_reset_host_25661166",
            "steam-25661166-physical-reset-v8.json",
            "LMU_PhysicalReset25661166_v8_",
            0x4C4D555253543038,
        ),
        "lmu-physical-reset-v7-25661166.dll": (
            "run_physical_reset_host_25661166",
            "steam-25661166-physical-reset-v7.json",
            "LMU_PhysicalReset25661166_v7_",
            0x4C4D555253543037,
        ),
        "lmu-physical-reset-v6-25661166.dll": (
            "run_physical_reset_host_25661166",
            "steam-25661166-physical-reset-v6.json",
            "LMU_PhysicalReset25661166_v6_",
            0x4C4D555253543036,
        ),
        "lmu-physical-reset-v5-25661166.dll": (
            "run_physical_reset_host_25661166",
            "steam-25661166-physical-reset-v5.json",
            "LMU_PhysicalReset25661166_v5_",
            0x4C4D555253543035,
        ),
        "lmu-physical-reset-v4-25661166.dll": (
            "run_physical_reset_host_25661166",
            "steam-25661166-physical-reset-v4.json",
            "LMU_PhysicalReset25661166_v4_",
            0x4C4D555253543034,
        ),
        "lmu-physical-reset-v3-25661166.dll": (
            "run_physical_reset_host_25661166",
            "steam-25661166-physical-reset-v3.json",
            "LMU_PhysicalReset25661166_v3_",
            0x4C4D555253543033,
        ),
        "lmu-physical-reset-v2-25661166.dll": (
            "run_physical_reset_host_25661166",
            "steam-25661166-physical-reset-v2.json",
            "LMU_PhysicalReset25661166_v2_",
            0x4C4D555253543032,
        ),
        "lmu-physical-reset-v1-25661166.dll": (
            "run_physical_reset_host_25661166",
            "steam-25661166-physical-reset-v1.json",
            "LMU_PhysicalReset25661166_v1_",
            0x4C4D555253543031,
        ),
        "lmu-boundary-observer-v4-25661166.dll": (
            "run_boundary_observer_v4_25661166",
            "steam-25661166-boundary-observer-v4.json",
            "LMU_Boundary25661166_v4_",
            0x4C4D555048415335,
        ),
        "lmu-boundary-observer-v3-25661166.dll": (
            "run_boundary_observer_v3_25661166",
            "steam-25661166-boundary-observer-v3.json",
            "LMU_Boundary25661166_v3_",
            0x4C4D555048415334,
        ),
        "lmu-boundary-observer-v2-25661166.dll": (
            "run_boundary_observer_v2_25661166",
            "steam-25661166-boundary-observer-v2.json",
            "LMU_Boundary25661166_v2_",
            0x4C4D555048415333,
        ),
        "lmu-boundary-observer-25661166.dll": (
            "run_boundary_observer_25661166",
            "steam-25661166-boundary-observer.json",
            "LMU_Boundary25661166_v1_",
            0x4C4D555048415332,
        ),
        "lmu-snapshot-observer-25661166-v31.dll": (
            "run_snapshot_observer_v31_25661166",
            "steam-25661166-snapshot-observer-v31.json",
            "LMU_Snapshot25661166_v31_",
            0x4C4D55534E503055,
        ),
        "lmu-snapshot-observer-25661166-v30.dll": (
            "run_snapshot_observer_v30_25661166",
            "steam-25661166-snapshot-observer-v30.json",
            "LMU_Snapshot25661166_v30_",
            0x4C4D55534E503054,
        ),
        "lmu-snapshot-observer-25661166-v29.dll": (
            "run_snapshot_observer_v29_25661166",
            "steam-25661166-snapshot-observer-v29.json",
            "LMU_Snapshot25661166_v29_",
            0x4C4D55534E503053,
        ),
        "lmu-phase-observer-25661166.dll": (
            "run_phase_observer_25661166",
            "steam-25661166-phase-observer.json",
            "LMU_Phase25661166_v1_",
            0x4C4D555048415331,
        ),
        "lmu-snapshot-observer-25661166-v28.dll": (
            "run_snapshot_observer_v28_25661166",
            "steam-25661166-snapshot-observer-v28.json",
            "LMU_Snapshot25661166_v28_",
            0x4C4D55534E503052,
        ),
        "lmu-snapshot-observer-25661166-v27.dll": (
            "run_snapshot_observer_v27_25661166",
            "steam-25661166-snapshot-observer-v27.json",
            "LMU_Snapshot25661166_v27_",
            0x4C4D55534E503051,
        ),
        "lmu-snapshot-observer-25661166-v26.dll": (
            "run_snapshot_observer_v26_25661166",
            "steam-25661166-snapshot-observer-v26.json",
            "LMU_Snapshot25661166_v26_",
            0x4C4D55534E503050,
        ),
        "lmu-snapshot-observer-25661166-v25.dll": (
            "run_snapshot_observer_v25_25661166",
            "steam-25661166-snapshot-observer-v25.json",
            "LMU_Snapshot25661166_v25_",
            0x4C4D55534E50304F,
        ),
        "lmu-snapshot-observer-25661166-v24.dll": (
            "run_snapshot_observer_v24_25661166",
            "steam-25661166-snapshot-observer-v24.json",
            "LMU_Snapshot25661166_v24_",
            0x4C4D55534E50304E,
        ),
        "lmu-snapshot-observer-25661166-v23.dll": (
            "run_snapshot_observer_v23_25661166",
            "steam-25661166-snapshot-observer-v23.json",
            "LMU_Snapshot25661166_v23_",
            0x4C4D55534E50304D,
        ),
        "lmu-snapshot-observer-25661166-v22.dll": (
            "run_snapshot_observer_v22_25661166",
            "steam-25661166-snapshot-observer-v22.json",
            "LMU_Snapshot25661166_v22_",
            0x4C4D55534E50304C,
        ),
        "lmu-snapshot-observer-25661166-v21.dll": (
            "run_snapshot_observer_v21_25661166",
            "steam-25661166-snapshot-observer-v21.json",
            "LMU_Snapshot25661166_v21_",
            0x4C4D55534E50304B,
        ),
        "lmu-snapshot-observer-25661166-v20.dll": (
            "run_snapshot_observer_v20_25661166",
            "steam-25661166-snapshot-observer-v20.json",
            "LMU_Snapshot25661166_v20_",
            0x4C4D55534E50304A,
        ),
        "lmu-snapshot-observer-25661166-v19.dll": (
            "run_snapshot_observer_v19_25661166",
            "steam-25661166-snapshot-observer-v19.json",
            "LMU_Snapshot25661166_v19_",
            0x4C4D55534E503049,
        ),
        "lmu-contact-native-v2-25661166.dll": (
            "run_contact_v2_25661166",
            "steam-25661166-contact-experiment-v2.json",
            "LMU_Contact25661166_v2_",
            0x4C4D55434F4E5432,
        ),
        "lmu-scoring-observer-25661166.dll": (
            "run_scoring_observer_25661166",
            "steam-25661166-scoring-observer.json",
            "LMU_Scoring25661166_v1_",
            0x4C4D5553434F5231,
        ),
        "lmu-snapshot-observer-25661166-v18.dll": (
            "run_snapshot_observer_v18_25661166",
            "steam-25661166-snapshot-observer-v18.json",
            "LMU_Snapshot25661166_v18_",
            0x4C4D55534E503048,
        ),
        "lmu-snapshot-observer-25661166-v17.dll": (
            "run_snapshot_observer_v17_25661166",
            "steam-25661166-snapshot-observer-v17.json",
            "LMU_Snapshot25661166_v17_",
            0x4C4D55534E503047,
        ),
        "lmu-snapshot-observer-25661166-v16.dll": (
            "run_snapshot_observer_v16_25661166",
            "steam-25661166-snapshot-observer-v16.json",
            "LMU_Snapshot25661166_v16_",
            0x4C4D55534E503046,
        ),
        "lmu-snapshot-observer-25661166-v15.dll": (
            "run_snapshot_observer_v15_25661166",
            "steam-25661166-snapshot-observer-v15.json",
            "LMU_Snapshot25661166_v15_",
            0x4C4D55534E503045,
        ),
        "lmu-snapshot-observer-25661166-v14.dll": (
            "run_snapshot_observer_v14_25661166",
            "steam-25661166-snapshot-observer-v14.json",
            "LMU_Snapshot25661166_v14_",
            0x4C4D55534E503044,
        ),
        "lmu-contact-native-25661166.dll": (
            "run_contact_25661166",
            "steam-25661166-contact-experiment.json",
            "LMU_Contact25661166_v1_",
            0x4C4D55434F4E5431,
        ),
        "lmu-snapshot-observer-25661166-v13.dll": (
            "run_snapshot_observer_v13_25661166",
            "steam-25661166-snapshot-observer-v13.json",
            "LMU_Snapshot25661166_v13_",
            0x4C4D55534E503043,
        ),
        "lmu-snapshot-observer-25661166-v12.dll": (
            "run_snapshot_observer_v12_25661166",
            "steam-25661166-snapshot-observer-v12.json",
            "LMU_Snapshot25661166_v12_",
            0x4C4D55534E503042,
        ),
        "lmu-snapshot-observer-25661166-v11.dll": (
            "run_snapshot_observer_v11_25661166",
            "steam-25661166-snapshot-observer-v11.json",
            "LMU_Snapshot25661166_v11_",
            0x4C4D55534E503041,
        ),
        "lmu-snapshot-observer-25661166-v10.dll": (
            "run_snapshot_observer_v10_25661166",
            "steam-25661166-snapshot-observer-v10.json",
            "LMU_Snapshot25661166_v10_",
            0x4C4D55534E503040,
        ),
        "lmu-snapshot-observer-25661166-v9.dll": (
            "run_snapshot_observer_v9_25661166",
            "steam-25661166-snapshot-observer-v9.json",
            "LMU_Snapshot25661166_v9_",
            0x4C4D55534E503039,
        ),
        "lmu-snapshot-observer-25661166-v8.dll": (
            "run_snapshot_observer_v8_25661166",
            "steam-25661166-snapshot-observer-v8.json",
            "LMU_Snapshot25661166_v8_",
            0x4C4D55534E503038,
        ),
        "lmu-snapshot-observer-25661166-v7.dll": (
            "run_snapshot_observer_v7_25661166",
            "steam-25661166-snapshot-observer-v7.json",
            "LMU_Snapshot25661166_v7_",
            0x4C4D55534E503037,
        ),
        "lmu-snapshot-observer-25661166-v6.dll": (
            "run_snapshot_observer_v6_25661166",
            "steam-25661166-snapshot-observer-v6.json",
            "LMU_Snapshot25661166_v6_",
            0x4C4D55534E503036,
        ),
        "lmu-snapshot-observer-25661166-v5.dll": (
            "run_snapshot_observer_v5_25661166",
            "steam-25661166-snapshot-observer-v5.json",
            "LMU_Snapshot25661166_v5_",
            0x4C4D55534E503035,
        ),
        "lmu-snapshot-observer-25661166-v4.dll": (
            "run_snapshot_observer_v4_25661166",
            "steam-25661166-snapshot-observer-v4.json",
            "LMU_Snapshot25661166_v4_",
            0x4C4D55534E503034,
        ),
        "lmu-snapshot-observer-25661166-v3.dll": (
            "run_snapshot_observer_v3_25661166",
            "steam-25661166-snapshot-observer-v3.json",
            "LMU_Snapshot25661166_v3_",
            0x4C4D55534E503033,
        ),
        "lmu-snapshot-observer-25661166.dll": (
            "run_snapshot_observer_25661166",
            "steam-25661166-snapshot-observer.json",
            "LMU_Snapshot25661166_v1_",
            0x4C4D55534E503031,
        ),
        "lmu-snapshot-observer-25661166-v2.dll": (
            "run_snapshot_observer_v2_25661166",
            "steam-25661166-snapshot-observer-v2.json",
            "LMU_Snapshot25661166_v2_",
            0x4C4D55534E503032,
        ),
    }
    api("OpenFileMappingW", [W.DWORD, W.BOOL, W.LPCWSTR], W.HANDLE)
    for entry in listed:
        name = entry["name"].lower()
        if re.fullmatch(r"lmu-physical-reset-v(?:9|10)-25661166-[0-9a-f]{32}\.dll", name):
            registry[name] = (
                "run_physical_reset_host_25661166",
                f"steam-25661166-physical-reset-v{name.split('-v')[1].split('-')[0]}.json",
                f"LMU_PhysicalReset_{name}_",
                0x4C4D555253543030 + int(name.split("-v")[1].split("-")[0]),
            )
        if not name.startswith("lmu-"):
            continue
        if name not in registry:
            raise ValueError("Unknown project module: " + name)
        module, manifest, prefix, magic = registry[name]
        path = (PROJECT / "bin" / name).resolve()
        profile = json.loads((PROJECT / "compatibility" / manifest).read_text())
        # The initial repository migration moved the unchanged bin directory.
        # Windows keeps the original mapped path until process exit. Accept only
        # an explicit local migration record for this exact former project path.
        former = PROJECT / "outputs" / "lmu-active-reset" / "bin" / name
        migrated = (
            profile.get("previous_dll_path") == str(former)
            and Path(entry["path"]) == former
            and not former.exists()
        )
        if (Path(entry["path"]) != path and not migrated) or hashlib.sha256(
            path.read_bytes()
        ).hexdigest() != profile["dll_sha256"]:
            raise ValueError("Resident identity mismatch")
        IPC = importlib.import_module(module).IPC
        mapping = k.OpenFileMappingW(4, False, f"Local\\{prefix}{pid}")
        view = None
        try:
            if not mapping:
                raise ValueError("Missing snapshot IPC")
            view = k.MapViewOfFile(mapping, 4, 0, 0, C.sizeof(IPC))
            if not view:
                raise ValueError("Unreadable snapshot IPC")
            state = IPC.from_buffer_copy(C.string_at(view, C.sizeof(IPC)))
            if (
                state.magic != magic
                or state.pid != pid
                or state.version != 1
                or state.status != 3
                or state.error
                or state.stop != 1
                or state.allow_observation
            ):
                raise ValueError("Snapshot module not cleanly stopped")
            reports[name] = dict(
                status=state.status,
                error=state.error,
                stop=state.stop,
                allow=state.allow_observation,
                physics_writes=getattr(state, "physics_writes", 0),
            )
        finally:
            if view:
                k.UnmapViewOfFile(view)
            if mapping:
                k.CloseHandle(mapping)
    image = next(m for m in listed if m["name"].lower() == "le mans ultimate.exe")
    handle = k.OpenProcess(0x410, False, pid)
    if not handle:
        raise OSError("Cannot verify restored code")
    try:
        for revision in (1, 2, 3, 4, 5, 6, 7, 8, 9, 10):
            if f"lmu-physical-reset-v{revision}-25661166.dll" not in reports and not (
                revision >= 9
                and any(n.startswith(f"lmu-physical-reset-v{revision}-") for n in reports)
            ):
                continue
            profile = json.loads(
                (
                    PROJECT / f"compatibility/steam-25661166-physical-reset-v{revision}.json"
                ).read_text()
            )
            for target in profile["targets"]:
                expected = bytes.fromhex(target["prolog"])
                if read_process(handle, image["base"] + target["rva"], len(expected)) != expected:
                    raise ValueError("Physical reset target not restored: " + target["name"])
        raw = read_process(handle, image["base"] + 0xECB4E0, 15)
        if raw != bytes.fromhex("488bc4488958104889701848897820"):
            raise ValueError("Dynamics instructions not original")
        contact_prolog = bytes.fromhex("4c894c242048894c2408555356574154415541564157")
        if (
            any(
                name.startswith("lmu-contact-native")
                or name
                in (
                    "lmu-snapshot-observer-25661166-v30.dll",
                    "lmu-snapshot-observer-25661166-v31.dll",
                )
                for name in reports
            )
            and read_process(handle, image["base"] + 0xEF8880, len(contact_prolog))
            != contact_prolog
        ):
            raise ValueError("Contact instructions not original")
        scoring_prolog = bytes.fromhex("488bc448895820f30f114810555657")
        if (
            any(
                name in reports
                for name in (
                    "lmu-scoring-observer-25661166.dll",
                    "lmu-phase-observer-25661166.dll",
                    "lmu-boundary-observer-25661166.dll",
                    "lmu-boundary-observer-v2-25661166.dll",
                    "lmu-boundary-observer-v3-25661166.dll",
                    "lmu-boundary-observer-v4-25661166.dll",
                )
            )
            and read_process(handle, image["base"] + 0xCF19F0, len(scoring_prolog))
            != scoring_prolog
        ):
            raise ValueError("Scoring instructions not original")
        if any(
            n in reports
            for n in (
                "lmu-boundary-observer-25661166.dll",
                "lmu-boundary-observer-v2-25661166.dll",
                "lmu-boundary-observer-v3-25661166.dll",
                "lmu-boundary-observer-v4-25661166.dll",
            )
        ) and read_process(handle, image["base"] + 0xCD1F40, 15) != bytes.fromhex(
            "488bc4488958104889701848897820"
        ):
            raise ValueError("Outer update instructions not original")
    finally:
        k.CloseHandle(handle)
    return dict(
        modules=reports,
        outer_update_original=any(
            n in reports
            for n in (
                "lmu-boundary-observer-25661166.dll",
                "lmu-boundary-observer-v2-25661166.dll",
                "lmu-boundary-observer-v3-25661166.dll",
                "lmu-boundary-observer-v4-25661166.dll",
            )
        ),
        dynamics_original=True,
        contact_original=any(
            name.startswith("lmu-contact-native")
            or name
            in ("lmu-snapshot-observer-25661166-v30.dll", "lmu-snapshot-observer-25661166-v31.dll")
            for name in reports
        ),
        scoring_original=any(
            name in reports
            for name in (
                "lmu-scoring-observer-25661166.dll",
                "lmu-phase-observer-25661166.dll",
                "lmu-boundary-observer-25661166.dll",
                "lmu-boundary-observer-v2-25661166.dll",
                "lmu-boundary-observer-v3-25661166.dll",
                "lmu-boundary-observer-v4-25661166.dll",
            )
        ),
        physics_writes=sum(m["physics_writes"] for m in reports.values()),
    )
