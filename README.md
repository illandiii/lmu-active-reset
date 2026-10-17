# Sector

A sim-racing companion for Le Mans Ultimate, starting with checkpoint and
physical-state restoration in offline Practice on Windows x64. Seven configurable telemetry overlays are available in the Overlay tab.

**V15 fixes the collision sweep that launched the car after moving resets.**
Seven live restores on 16 October verified all 10,084 fields, with current speeds
up to 242.6 km/h and distances up to 797 metres from the checkpoint. The user
confirmed successful teleports, including while cornering. The detailed traces
show no collision-induced jump in the first 20 physics steps after each restore;
broader stability and lifecycle qualification remain ongoing.

**The first near-stationary physical reset succeeded in offline Practice.** The
experimental host restored and verified all 10,077 planned fields, rebuilt four
native tire contacts and resumed simulation without transaction recovery. Its
11 hooks stopped cleanly. This is a bounded initial experiment, not qualification
for moving resets, damage recovery or lap validity. Focused save/restore hotkeys
are connected to this host. See the [host evidence](docs/PHYSICAL_RESET_HOST.md).

## Try the local build

Run `Start-Sector.cmd` to open the English, dark-only Electron app (`Start-Reset.cmd`
remains an alias). It automatically
detects LMU and waits for driving in offline Practice without EAC. There are no
start/stop buttons or session countdown. Release both keys before using them:

- **F6** saves the checkpoint.
- **F7** prepares and restores it after successful native acknowledgement.
- Select a shortcut and press its new key; settings persist across launches.
  Letters, digits, function keys, navigation and numpad keys are supported.
  Escape cancels capture; modifiers and combinations are not assigned.
- Distinct sounds confirm save, restore and rejection; preview or mute them in the GUI.
- The checkpoint card shows availability; shortcuts and confirmation audio sit below it.
- Use **Guide** beside the page title for a brief tour with arrows and popups.
- **Overlay** contains seven widgets with per-widget position, size, opacity and visibility.
  Use **Edit layout** to drag them, then **Lock layout** to return input to LMU.
  See [overlay setup and supported data](docs/OVERLAYS.md).
- The gear button opens app settings, including a persistent Reduce motion option.
- Connection status and the latest result remain visible in the bottom status bar.
- Closing the window offers **Run in background**, **Quit Sector** or **Cancel**
  in a popup inside the app.
- In the background, shortcuts remain active. Reopen from the tray or launcher.
- Quit waits for native cleanup; a second launcher reveals the existing app.

The top tabs separate **Active Reset** from the **Overlay** placeholder. Scrolling
uses the app's dark theme. Overlay functionality will be added in a later version.
The launcher builds the local renderer and installs its locked dependencies on
first use. Node.js 22.12+ and npm are required, with internet access for initial
setup. Later launches use local assets. Existing shortcuts and sound preferences
are retained. For a UI-only preview, run `scripts/launch/Start-ResetGui.ps1 -Preview`.

A checkpoint has no time-based expiry. The v15 host has no session-duration cap;
vehicle reconstruction, including a reset to the pits, invalidates its checkpoint.
After a clean detach, the app waits for driving and connects a fresh host automatically.
Pausing driving or moving focus alone does not discard a checkpoint. Existing
identity/lifecycle checks can also invalidate it when the underlying vehicle changes.

The host has no speed or checkpoint-distance limit. Moving and long-distance
restores pass isolated model tests and the seven live v15 trials described above.
Identity, lifecycle, coherent physical state and valid contact
reconstruction remain required. The keys are observed, not swallowed: select unused game bindings.
For different keys run `scripts/launch/Start-ResetHotkeys.ps1 -SaveKey F9 -RestoreKey F10`.
The launcher discovers the process and worker, chooses an unused locally built
v15 host under a unique instance filename, and never replaces a resident DLL or
falls back to older builds. Stopped modules remain resident until LMU exits.
Generated binaries and the local Python runtime are required; a fresh
clone must be built before this launcher can be used. On the configured Windows
machine, close LMU and run `scripts/build/Build-ResetDesktop.ps1 -ZigPath C:\tools\zig\zig.exe`
to rebuild v15 and the continuous telemetry reader. See
[desktop setup and verification](docs/DESKTOP_GUI.md).

The current host records collision preparation/correction around the restore,
using the diagnostics introduced in v12 and a continuous ring in v14.
Further live tests are documented in Italian in
[the collision test procedure](docs/TEST_RESET_COLLISIONI.md). The 16 October capture identified a collision sweep across the pre-reset and saved
locations. V15 restores the aggregate/exported kinematics and rebuilds collision
state before physics resumes. Local regression and compensation tests pass;
the subsequent live results are recorded in the
[collision incident history](docs/HIGH_SPEED_RESET_INCIDENT.md).

## Repository layout

| Directory | Contents |
| --- | --- |
| `src/native/physics/` | Physical state, constraint and inertia calculations |
| `src/native/checkpoints/` | Saved state banks and subsystem checkpoints |
| `src/native/restore/` | Restore plans, transactions, vehicle adapters and contacts |
| `src/native/runtime/` | Physics-thread host, synchronization, lifecycle and hooks |
| `src/native/observers/` | Historical capture modules and their wire layouts |
| `src/native/telemetry/` | Native telemetry reader |
| `src/python/sector/` | Python package, split into runtime, desktop, telemetry, observers, analysis, research and replay |
| `src/ui/electron/` | Electron main process, isolated preload, React renderer and desktop tests |
| `tests/native/`, `tests/python/` | Native fixtures/bridges and Python unit tests |
| `tools/rea/` | Offline REA JavaScript integration |
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
./scripts/test/Test-Core.ps1 -ZigPath 'C:\tools\zig\zig.exe'
python scripts/sector.py test
```

This suite runs isolated fixtures and does not load code into LMU. Live observer
builds additionally require local installation/compatibility manifests and exact
matching game binaries. An empty checkout does not contain local capture data.

Run Python tools through the package launcher, for example
`python scripts/sector.py analysis.analyze_physical_reset_trace <capture.physical.jsonl>`.
The launcher resolves repository paths independently of the working directory;
it also supplies the package path to child capture/replay processes. See
[source organization](docs/SOURCE_LAYOUT.md) for entry points and build rules.

Format C/C++ with clang-format 21.1.8 and Python with Ruff 0.14.1:

```powershell
./scripts/dev/Format-Code.ps1 -ClangFormat clang-format -Ruff ruff
```

## Current work

1. Qualify lifecycle transitions and physical stability across more cars and tracks.
2. Validate lap validity and longer sessions with repeated moving resets.
3. Validate the complete desktop workflow during longer live-game sessions.

The Electron desktop UI passes Windows layout, status and lifecycle smoke tests. The seven
live v15 reset trials used the same engine through the hotkey launcher; the new
tray workflow has been tested independently of LMU. See [desktop architecture,
design sources and tests](docs/DESKTOP_GUI.md).
See [physics thread findings](docs/PHYSICS_THREAD_2026-10-13.md)
and [restore integration](docs/RESTORE_INTEGRATION_2026-10-13.md).
The [development archive](docs/README.md) preserves earlier results chronologically;
older paths and source hashes refer to the pre-reorganization working tree.

## Windows development release 0.1.6

Open `releases/Sector-0.1.6-win-x64/Sector.exe`. Keep the complete folder together;
Node.js and Python do not need to be installed. See [release instructions](docs/RELEASE.md)
for the repeatable build and packaged-app checks.
