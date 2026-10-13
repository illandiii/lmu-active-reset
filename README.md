# LMU Active Reset

Experimental checkpoint and physical-state restoration for Le Mans Ultimate,
developed for offline Practice on Windows x64.

**The first near-stationary physical reset succeeded in offline Practice.** The
experimental host restored and verified all 10,077 planned fields, rebuilt four
native tire contacts and resumed simulation without transaction recovery. Its
11 hooks stopped cleanly. This is a bounded initial experiment, not qualification
for moving resets, damage recovery or lap validity. Focused save/restore hotkeys
are connected to this host. See the [host evidence](docs/PHYSICAL_RESET_HOST.md).

## Try the local build

Open a healthy vehicle in offline Practice without EAC and enter driving mode.
Run `Start-Reset.cmd`, then return
focus to LMU and release both function keys before using them:

- **F6** saves the checkpoint.
- **F7** prepares and restores it after successful native acknowledgement.
- **Ctrl+C in the console** stops the tool; it also stops automatically after four minutes.

A system sound and console message report save/restore success or rejection.
The v8 host has no speed or checkpoint-distance limit. Moving and long-distance
restores pass isolated model tests; live validation of that expanded behavior is
still pending. Identity, lifecycle, coherent physical state and valid contact
reconstruction remain required. The keys are observed, not swallowed: select unused game bindings.
For different keys run `scripts/Start-ResetHotkeys.ps1 -SaveKey F9 -RestoreKey F10`.
The launcher discovers the process and worker, chooses an unused locally built
v8 host, and never replaces a resident DLL or falls back to older builds. Restart LMU if no revision is
available. Generated binaries and the local Python runtime are required; a fresh
clone must be built before this launcher can be used.

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

1. Validate the focused hotkey workflow with user input in LMU.
2. Qualify lifecycle transitions and physical stability beyond the initial experiment.
3. Validate moving resets, lap validity and hotkeys.

The UI is postponed. See [physics thread findings](docs/PHYSICS_THREAD_2026-10-13.md)
and [restore integration](docs/RESTORE_INTEGRATION_2026-10-13.md).
The [development archive](docs/README.md) preserves earlier results chronologically;
older paths and source hashes refer to the pre-reorganization working tree.
