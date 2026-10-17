# Source layout and entry points

Production code is split by responsibility. Native state/restore code does not
depend on the desktop renderer; Python owns supervision and Electron owns the
desktop and overlay windows.

```text
src/
  native/
    physics/       Rigid bodies, inertia and constraints
    checkpoints/   Subsystem snapshots and checkpoint banks
    restore/       Plans, transactions, adapters and contact reconstruction
    runtime/       Physics callbacks, synchronization, lifecycle and diagnostics
    observers/     Historical captures and versioned wire layouts
    telemetry/     Shared-memory reader
  python/sector/
    runtime/       Hotkey supervision, host commands and physical traces
    desktop/       UI status, feedback sounds and shared key definitions
    telemetry/     Context capture, admission observations and wire types
    observers/     Observer runners and shutdown audits
    analysis/      Capture summaries and trace analysis
    research/      Read-only probes, executable inspection and relocation studies
    replay/        Recorded-frame replay and fixture preparation
    paths.py       Workspace paths independent of the working directory
  ui/electron/
    main/          Privileged Electron controllers and isolated preload
    renderer/      React desktop, overlays, styles and local assets
    shared/        Shared models
    build/         Portable release packaging
    tests/         Controller tests and Electron smoke suites
tests/
  native/          Owned-memory fixtures and fixture bridges
  python/          Unit tests and source-layout checks
scripts/
  build/          Maintained native and UI builds
  launch/         Source-checkout GUI and console launchers
  test/           Test entry points
  dev/            Formatting, source resolution and research helpers
  sector.py       Python package launcher
tools/rea/         Offline REA JavaScript analysis
docs/archive/      Historical experiments and build-specific contracts
```

## Commands

Use the package launcher rather than running Python modules as loose files:

```powershell
python scripts/sector.py test
python scripts/sector.py runtime.start_reset_hotkeys --help
python scripts/sector.py analysis.analyze_physical_reset_trace logs/example.physical.jsonl
```

The launcher supplies `src/python` and the repository root to the process and
its children. An explicitly configured environment can instead use
`python -m sector.<area>.<module>` with `PYTHONPATH=src/python`.

| Task | Entry point |
| --- | --- |
| Start source UI | `./scripts/launch/Start-ResetGui.ps1` |
| Isolated UI preview | `./scripts/launch/Start-ResetGui.ps1 -Preview` |
| Console reset diagnostics | `./scripts/launch/Start-ResetHotkeys.ps1` |
| Native/desktop build | `./scripts/build/Build-ResetDesktop.ps1 -ZigPath <zig.exe>` |
| Native core checks | `./scripts/test/Test-Core.ps1 -ZigPath <zig.exe>` |
| C++/Python formatting | `./scripts/dev/Format-Code.ps1 -ClangFormat clang-format -Ruff ruff` |

The redundant root `.cmd` aliases have been removed. Portable users launch
`Sector.exe`; contributors use the maintained PowerShell entry points above.
See [contributing](../CONTRIBUTING.md) for per-component checks.

## Build rules and retained history

`scripts/dev/SourcePaths.ps1` indexes `src/native` and `tests/native`, rejects
duplicate filenames and fails for missing sources. Build lists use unique
basenames; headers use relative includes across areas. No forwarding files are
needed at obsolete flat source paths.

Historical observer classes and wire layouts remain because replay tools import
them. Their versioned field offsets are part of recorded-data decoding, not
permission to load an old module against a newer game. Earlier logs can retain
old filenames; historical reports are indexed in [the archive](archive/README.md).

## Generated and local files

| Directory | Local contents |
| --- | --- |
| `bin/` | Generated native executables and DLLs |
| `logs/` | Captures, diagnostics and test logs |
| `analysis/` | Local reverse-engineering output |
| `compatibility/` | Machine/build-specific manifests |
| `work/` | Toolchains, fixture material, preview profiles and screenshots |
| `releases/` | Portable outputs and archives |

These directories, dependency caches and the renderer build output are ignored.
Do not delete local evidence or compatibility inputs as generic cleanup: they
can be required to reproduce a native result or build.

Compiled binaries, runtime data (`.dat`, `.bin`, `.pak`, `.asar`), archives and
debug symbols are also ignored if generated outside those directories. The
source repository retains code, build/test scripts, configuration and lockfiles,
licenses, bundled UI fonts/artwork and documentation media. The unused original
logo reference sheet stays local; widgets import `logos-transparent.png`.

`scripts/build/` and `src/ui/electron/build/` contain maintained source scripts,
not compiled output, and must remain tracked. A portable release still needs its
runtime DLLs and data files: excluding them from Git does not remove them from
the release package. Distribute the complete portable folder to users.
