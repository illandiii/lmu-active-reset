# Source organization

Sector separates production native code, Python orchestration, UI and tests.
The reorganization changes paths and imports, not native layouts or reset policy.

```text
src/
  native/
    physics/       Rigid bodies, inertia and constraints
    checkpoints/   Saved subsystem state and checkpoint banks
    restore/       Plans, transactions, vehicle adapters and contact rebuilds
    runtime/       Host callbacks, synchronization, lifecycle and diagnostics
    observers/     Versioned historical capture modules and wire layouts
    telemetry/     Shared-memory telemetry reader
  python/sector/
    runtime/       Hotkey supervision, native host commands and physical traces
    desktop/       UI snapshots, feedback sounds and shared key definitions
    telemetry/     Context capture, admission observations and wire types
    observers/     Historical observer runners and clean-shutdown audits
    analysis/      Capture summaries and physical trace analysis
    research/      Read-only probes, executable inspection and relocation studies
    replay/        Recorded-frame replay and fixture preparation tools
    paths.py       Repository paths independent of the working directory
  ui/electron/     Electron main/preload, React renderer and desktop tests
tests/
  native/          Isolated native fixtures and Python fixture bridges
  python/          Python unit and source-layout tests
tools/rea/         Offline JavaScript REA analysis
scripts/           Launchers, PowerShell desktop controller, builds and checks
```

## Running Python

Use the repository launcher rather than executing package files directly:

```powershell
python scripts/sector.py test
python scripts/sector.py runtime.start_reset_hotkeys --help
python scripts/sector.py analysis.analyze_physical_reset_trace logs/example.physical.jsonl
python scripts/sector.py replay.test_vehicle_partial_checkpoint work/example.frames.jsonl --verify-plan
```

The launcher inserts `src/python` and the repository root into the module path
and passes the same paths to subprocesses. A configured environment can instead
set `PYTHONPATH=src/python` and use `python -m sector.<area>.<module>`.
The desktop and console launchers use the common launcher automatically.
Settings remain in their existing user directory; local captures and binaries
remain in `logs`, `work`, `bin` and `compatibility`.

## Native builds

`SourcePaths.ps1` indexes only `src/native` and `tests/native`, rejects duplicate
filenames and fails if a requested source is missing. Build source lists remain
readable basename lists. Headers use relative paths across source areas, so the
compiler does not depend on an artificial flat include directory. Historical
observer classes, IPC layouts and field offsets are preserved for replay.

The repository retains no forwarding copies at old source paths. Documentation
links and runnable examples use the new layout. Earlier log messages and local
capture metadata can still contain historical filenames.

## Migration verification

The reorganized tree passes 95 Python tests, the Windows WPF interaction suite,
all `Test-Core.ps1` native suites and the complete v15 host fixtures (10,084 fields,
four contacts, missing-callback refusal and two-instance restart). The telemetry
reader compiles from its new directory and passes all 24 self-tests. Replay of
two recorded frames passes its 58 negative checks. The 50 historical Python
observer class definitions are unchanged apart from their enclosing imports.
No live-game reset was needed for this path-only migration.
