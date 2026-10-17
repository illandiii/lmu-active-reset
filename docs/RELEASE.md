# Building a Windows release

The current development version is **0.2.8**. A portable package starts with
`Sector.exe`; keep its adjacent DLLs and resources together. It bundles Electron,
CPython 3.13.12, the telemetry reader/diagnostic and revision 20 reset host.
The package is unsigned and does not require installed Node.js or Python.

## Prerequisites

| Requirement | Purpose |
| --- | --- |
| Windows x64 and PowerShell 7 | Native builds, launcher and packaging |
| Node.js 22.12+ with npm | Electron dependencies, UI build and release script |
| Python 3.11+ for source tooling | Python tests, captures and replay |
| Zig 0.16.0 | Native component builds |
| Locally verified LMU SDK, profiles and recorded fixture inputs | Exact native layout and compatibility verification |
| Pinned embedded Python archive | Bundled portable runtime |

The repository does not distribute game binaries, proprietary SDK headers,
decompiled code or local compatibility manifests. A fresh clone can build a UI
preview after installing its dependencies, but cannot reproduce the native release
without these separately prepared prerequisites. A matching app version string
does not replace the pinned compatibility checks.

## Build procedure

1. Close Sector and LMU before rebuilding native outputs. Never replace a DLL
   still resident in the game.
2. On the configured development machine, run:

   ```powershell
   ./scripts/build/Build-ResetDesktop.ps1 -ZigPath 'C:\tools\zig\zig.exe'
   ```

   This builds the desktop backend, overlay reader and renderer and runs their
   configured checks. The recorded host fixture inputs must be available locally.
3. Place the official [CPython 3.13.12 embedded x64 archive](https://www.python.org/ftp/python/3.13.12/python-3.13.12-embed-amd64.zip)
   at `work/release-tools/python-3.13.12-embed-amd64.zip`. The packager verifies its
   pinned SHA-256.
4. From `src/ui/electron`, run:

   ```powershell
   npm ci
   node node_modules/electron/install.js
   npm run release
   npm run test:release
   ```

The output is `releases/Sector-0.2.8-win-x64/`. The builder refuses an existing
destination; preserve or deliberately remove a previous output before rebuilding.
Generated releases are ignored by Git. Building or committing a version does not
publish a GitHub release or promote `prod`.

## Package contents and validation

Packaging uses Electron's [manual distribution layout](https://www.electronjs.org/docs/latest/tutorial/application-distribution)
and Python's [embedded distribution](https://docs.python.org/3/using/windows.html#the-embeddable-package).
The EXE receives the Sector icon and package version. Native hashes must match
the included sanitized compatibility profiles. SDK headers, captures and local
installation paths are excluded.

The package includes `LICENSE-Sector.txt` alongside Electron's original `LICENSE`
and Chromium notices. Dependency and font notices are retained under
`resources/app/licenses/`, with `THIRD_PARTY_NOTICES.md` in the application payload.
The root [MIT license](../LICENSE) applies to original Sector code/documentation;
it does not replace third-party terms.

`npm run test:release` launches the actual EXE in isolated preview and checks
the version, UI, overlays, bundled Python imports and absence of a reset listener.
It does not perform a live reset. For first-run changes, also run
`npm run test:onboarding`; `node tests/onboarding-smoke.mjs --packaged` repeats
onboarding checks against the built EXE. See [test coverage](TEST_RESULTS.md).

## Runtime files and user preferences

On packaged startup, runtime resources are copied into Electron's user-data
directory under `runtime/<version>-<content-id>/`. Native logs and IPC files live
inside that writable workspace. Content-specific directories prevent replacement
of modules that might still be loaded.

Preferences remain in `%LOCALAPPDATA%/LMUActiveReset/`: `settings.json`,
`overlays.json` and `onboarding.json`. Installation selection and onboarding
completion survive a runtime upgrade. First-run detection and manual selection
are described in the [desktop guide](DESKTOP_GUI.md).

## Branch policy

Commit completed development parts to `dev`. `prod` is the default branch and
changes only on explicit approval to promote a release. Version changes must keep
the Electron package/lockfile and current user documentation consistent.
