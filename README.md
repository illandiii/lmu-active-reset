# LMU Active Reset

Experimental checkpoint and physical-state restoration for Le Mans Ultimate,
developed for offline Practice on Windows x64.

**Known issue: a high-speed reset can launch the car into the air.** A live v8
recording confirms the instability after the native update resumes, despite
successful field readback. The cause is still under investigation.

**The first near-stationary physical reset succeeded in offline Practice.** The
experimental host restored and verified all 10,077 planned fields, rebuilt four
native tire contacts and resumed simulation without transaction recovery. Its
11 hooks stopped cleanly. This is a bounded initial experiment, not qualification
for moving resets, damage recovery or lap validity. Focused save/restore hotkeys
are connected to this host. See the [host evidence](docs/PHYSICAL_RESET_HOST.md).

## Try the local build

Run `Start-Reset.cmd` to open the English, dark-only desktop app. It automatically
detects LMU and waits for driving in offline Practice without EAC. There are no
start/stop buttons or session countdown. Release both keys before using them:

- **F6** saves the checkpoint.
- **F7** prepares and restores it after successful native acknowledgement.
- Change either key (F1–F24) in the GUI; settings persist across launches.
- Distinct sounds confirm save, restore and rejection; preview or mute them in the GUI.
- Closing the window requests orderly native shutdown.

A checkpoint has no time-based expiry. The v13 host has no session-duration cap;
vehicle reconstruction, including a reset to the pits, invalidates its checkpoint.
After a clean detach, the app waits for driving and connects a fresh host automatically.
Pausing driving or moving focus alone does not discard a checkpoint. Existing
identity/lifecycle checks can also invalidate it when the underlying vehicle changes.

The host has no speed or checkpoint-distance limit. Moving and long-distance
restores pass isolated model tests; live validation of that expanded behavior is
still pending. Identity, lifecycle, coherent physical state and valid contact
reconstruction remain required. The keys are observed, not swallowed: select unused game bindings.
For different keys run `scripts/Start-ResetHotkeys.ps1 -SaveKey F9 -RestoreKey F10`.
The launcher discovers the process and worker, chooses an unused locally built
v13 host under a unique instance filename, and never replaces a resident DLL or
falls back to older builds. Stopped modules remain resident until LMU exits.
Generated binaries and the local Python runtime are required; a fresh
clone must be built before this launcher can be used. On the configured Windows
machine, close LMU and run `scripts/Build-ResetDesktop.ps1 -ZigPath C:\tools\zig\zig.exe`
to rebuild v13 and the continuous telemetry reader. See
[desktop setup and verification](docs/DESKTOP_GUI.md).

The v12 diagnostic build records collision preparation/correction around the
restore. The next live test is documented in Italian in
[the collision test procedure](docs/TEST_RESET_COLLISIONI.md). This instrumentation
has passed local fixtures; it has not yet established or fixed the live cause.

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

The desktop UI is implemented; native Windows visual and live validation remain pending.
See [physics thread findings](docs/PHYSICS_THREAD_2026-10-13.md)
and [restore integration](docs/RESTORE_INTEGRATION_2026-10-13.md).
The [development archive](docs/README.md) preserves earlier results chronologically;
older paths and source hashes refer to the pre-reorganization working tree.
