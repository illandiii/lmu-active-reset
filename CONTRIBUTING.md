# Development workflow

Keep each completed, verified part in a separate commit. Record the relevant
tests and remaining limitations before committing. Do not describe fixture
results as evidence that a full reset has succeeded in LMU.

Run `scripts/Test-Core.ps1` after changing restore or callback coordination.
Format owned sources using `scripts/Format-Code.ps1`; leave third-party code
unchanged. Preserve historical observer layouts used by replay tools.

Write code comments and docstrings in English. Explain ownership, synchronization
and non-obvious native constraints; avoid comments that merely repeat a statement.
Keep UI presentation separate from native admission and reset sequencing. Desktop
and future overlay views must display acknowledged state rather than infer success
from a keypress.

The Electron app lives in `src/ui/electron`. Run `npm run format:check`,
`npm test`, `npm run build` and `npm run test:ui` there after desktop changes.
UI smoke tests use an isolated preview profile and never attach to LMU.
Keep privileged operations in `main/`; the renderer only receives the explicit
preload API. Do not introduce another hotkey listener in Electron.

Check Python changes with `ruff check src/python tests/python scripts/sector.py`,
`ruff format --check src/python tests/python scripts/sector.py` and
`python scripts/sector.py test`. Import cleanup must preserve
wire types exported to other capture and replay modules.

Use explicit `sector.<area>.<module>` imports. Read repository paths from
`sector.paths`, and invoke tools through `scripts/sector.py`. Keep native headers
next to their implementation and use relative includes across areas. Build lists
resolve unique filenames through `scripts/SourcePaths.ps1`; new source files do
not need compatibility copies in the former flat `src` directory.

Keep generated captures, proprietary binaries/disassembly, local installation
paths and credentials out of commits. Never overwrite a DLL while it is resident
in a running game, even when its hooks have stopped.
