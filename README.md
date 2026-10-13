# LMU Active Reset

Experimental checkpoint and physical-state restoration for Le Mans Ultimate,
developed for offline Practice on Windows x64.

**The complete reset is not yet operational in the game.** The physical/scoring
transaction, compensation, four-wheel contact adapter and callback guards have
been tested on owned fixture memory. Live observations confirm the outer physics
boundary and its native critical section. Object lifetime coverage and the live
checkpoint adapter remain under development.

## Repository layout

| Directory | Contents |
| --- | --- |
| `src/` | Native implementation, fixtures, Python capture/replay tools and REA bridge |
| `scripts/` | PowerShell builds, tests and installation inspection |
| `docs/` | Design contracts, development history, evidence summaries and limitations |
| `third_party/minhook/` | MinHook source with its original license and authors |

Generated binaries (`bin/`), captures (`logs/`), reverse-engineering output
(`analysis/`), machine-specific manifests (`compatibility/`) and toolchains/caches
(`work/`) stay local and are excluded from Git. No game executable, memory dump
or decompiled game source is distributed here. Historical observer versions are
retained because replay tools import their exact data layouts.

## Build and test

Requirements: Windows x64, PowerShell 7 and Zig 0.16.0. Python replay tools require
Python 3.11 or later; disassembly tools additionally require `pefile` and `capstone`.

```powershell
./scripts/Test-Core.ps1 -ZigPath 'C:\tools\zig\zig.exe'
```

This suite runs isolated fixtures and does not load code into LMU. Live observer
builds additionally require local installation/compatibility manifests and exact
matching game binaries. An empty checkout does not contain local capture data.

Format C/C++ with clang-format 21.1.8 and Python with Ruff 0.14.1:

```powershell
./scripts/Format-Code.ps1 -ClangFormat clang-format -Ruff ruff
```

## Current work

1. Complete synchronization coverage for object destruction and control changes.
2. Install the [full checkpoint/four-contact adapter](docs/VEHICLE_ADAPTER.md) only after
   the native lifetime contract is established. Its integrated fixtures now pass.
3. Validate the first full reset, physical stability, lap validity and hotkeys.

The UI is postponed. See [physics thread findings](docs/PHYSICS_THREAD_2026-10-13.md)
and [restore integration](docs/RESTORE_INTEGRATION_2026-10-13.md).
The [development archive](docs/README.md) preserves earlier results chronologically;
older paths and source hashes refer to the pre-reorganization working tree.
