# Sector 0.1.2 for Windows x64

The portable release starts with `Sector.exe`. Keep its adjacent resources and DLLs.
It includes Electron, embedded CPython 3.13.12, the overlay reader, telemetry diagnostic
and revision 15 reset host. It does not require a developer checkout or installed
Node/Python. This build is unsigned.

## Build

1. Build the native components with `scripts/build/Build-ResetDesktop.ps1` and the
   locally verified LMU SDK/profile, or use their already verified local outputs.
2. Download `https://www.python.org/ftp/python/3.13.12/python-3.13.12-embed-amd64.zip`
   into `work/release-tools/`. The release script checks its pinned SHA-256.
3. In `src/ui/electron`, run `npm ci`, `node node_modules/electron/install.js`, `npm run release`,
   then `npm run test:release`.
   The output is `releases/Sector-0.1.2-win-x64`. Existing outputs are never overwritten.

Packaging follows Electron's [manual distribution layout](https://www.electronjs.org/docs/latest/tutorial/application-distribution)
and uses Python's [embedded distribution](https://docs.python.org/3/using/windows.html#the-embeddable-package).
The executable has the Sector icon and file/product version 0.1.2. Native binary
hashes are checked against the shipped compatibility profiles. SDK headers, game
binaries, captures, developer tools and machine-specific installation paths are excluded.

At first launch, the runtime is copied to a version-and-content-specific directory
under the user's application data. It is never overwritten while a native module
could be loaded. Logs and IPC files stay there; preferences retain their existing
`LOCALAPPDATA/LMUActiveReset` location. The LMU install is resolved from the requested
process and still must pass every pinned file/UI hash and offline Practice check.

## Validation

`npm run test:release` launches the actual EXE in isolated preview, verifies version,
UI, overlays, bundled Python imports and absence of a reset listener. It does not
exercise a live reset. The source UI, Python and native fixture suites remain required.

## Repository layout

- `src/native`: native restore, telemetry and observer components.
- `src/python/sector`: runtime, desktop, telemetry, observers, replay and research.
- `src/ui/electron`: Electron main/preload, renderer, shared model and release builder.
- `scripts/build`: maintained native/UI builds.
- `scripts/test`: test entry points.
- `scripts/launch`: source-checkout launchers.
- `scripts/dev`: formatting, inventory and research helpers.
- `tests`: native and Python fixtures; Electron tests live beside the UI.

Thirty superseded snapshot observer build scripts were removed. Historical observer
sources and replay layouts remain because audits and recorded captures still use them.
Old documentation references to removed scripts describe historical experiments;
the original scripts can be recovered from Git history.

`dev` contains development releases. `prod` is the default branch and is promoted
only on explicit user request; creating 0.1.2 does not promote it to production.
