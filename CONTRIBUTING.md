# Contributing to Sector

Sector combines a Windows desktop, telemetry overlays and a native physical-reset
runtime. Keep changes focused, readable and accompanied by evidence appropriate
to the component changed. Start with the [source map](docs/SOURCE_LAYOUT.md).

## Branches and commits

Develop and push completed work on `dev`. `prod` is the default branch and stays
on the approved release; promote it only after an explicit maintainer request.
Commit each completed part with a description of the behavior changed and the
checks performed. Do not commit generated artifacts or machine-specific data.

## Code conventions

- Write comments and docstrings in English. Explain ownership, synchronization
  and native constraints instead of restating the code.
- Format owned C/C++ with clang-format and Python with Ruff. Keep third-party
  sources and their license texts unchanged.
- Import Python modules through `sector.<area>.<module>` and resolve paths with
  `sector.paths`. Use `scripts/sector.py` for commands and subprocess entry points.
- Keep native headers with their implementation. Build scripts resolve unique
  source basenames through `scripts/dev/SourcePaths.ps1`.
- Keep privileged operations in Electron's main process and use the explicit
  preload API. Do not add another reset hotkey listener in the renderer.
- Show acknowledged native results. A keypress is not evidence of a saved or
  restored physical state.

## Checks by component

Run only the checks relevant to the change, then record their results.

| Component | Commands |
| --- | --- |
| Python | `python scripts/sector.py test`, `ruff check src/python tests/python scripts/sector.py`, `ruff format --check src/python tests/python scripts/sector.py` |
| Native reset or synchronization | `./scripts/test/Test-Core.ps1 -ZigPath <zig.exe>` and the affected host fixture |
| Desktop and overlays | In `src/ui/electron`: `npm run format:check`, `npm test`, `npm run build`, `npm run test:ui` |
| First-launch setup | `npm run test:onboarding` in the Electron directory |
| Portable release | [Release procedure](docs/RELEASE.md), including `npm run test:release` |
| Documentation | Verify relative links, commands, version references and the distinction between current behavior and historical evidence |

Preview tests use isolated profiles and do not attach to LMU. Native fixtures
operate on owned memory. Neither replaces a live-game test. Report the car,
track, build and observed result when documenting live evidence; preserve failures
and limitations rather than describing successful field writes as proof of stability.

## Refresh README screenshots

From `src/ui/electron`, run `npm run build`, then `node build/capture-readme.mjs`.
This requires FFmpeg on `PATH`. It captures the real desktop in isolated preview
and renders the production widget components with explicitly labelled demo data.
It never connects to LMU. Final PNGs and the six-second GIF go to `docs/media/`;
intermediate frames and the gallery build stay under ignored `work/readme-media/`.

Review the images before committing them. Keep test results and implementation
details in developer documentation rather than the user-facing README.

## Native runtime and repository hygiene

Never overwrite a DLL while it is resident in LMU, including after its hooks stop.
Retain historical observer layouts used to decode recorded captures. Removing a
launcher requires updating its callers and documentation; removing research code
requires checking replay dependencies first.

Do not commit game binaries, dumps, decompiled game code, local installation
paths, compatibility captures or credentials. Keep generated material in the
ignored directories listed in [source layout](docs/SOURCE_LAYOUT.md).

## License and attribution

New original project code and documentation use the repository's [MIT license](LICENSE).
Preserve dependency licenses and attribution when adding or updating assets.
Record them in [third-party notices](src/ui/electron/THIRD_PARTY_NOTICES.md) and
include required notices in the portable package.
