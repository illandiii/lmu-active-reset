# Sector

[![Star this repo](https://img.shields.io/badge/Star_this_repo-181717?style=for-the-badge&logo=github&logoColor=white)](https://github.com/sector-lmu/Sector)
[![GitHub stars](https://img.shields.io/github/stars/sector-lmu/Sector?style=for-the-badge&label=Stars&color=73B7FF)](https://github.com/sector-lmu/Sector/stargazers)
[![Ko-fi](https://img.shields.io/badge/Ko--fi-F16061?style=for-the-badge&logo=ko-fi&logoColor=white)](https://ko-fi.com/illandiii)
[![License: MIT](https://img.shields.io/badge/License-MIT-73B7FF?style=for-the-badge)](LICENSE)

A Windows companion for **Le Mans Ultimate**, with physical checkpoints and
customizable telemetry overlays. Current development version: **0.1.6**.

## 🏁 Active Reset

Save a checkpoint, keep driving, then restore the saved vehicle state: position,
orientation, velocity and the physical subsystems covered by the reset host.
Restoring a checkpoint saved at 80 km/h restores that saved velocity, even if
you request the reset at 200 km/h.

| Default key | Action |
| --- | --- |
| **F6** | Save a checkpoint |
| **F7** | Restore the checkpoint |

Click either shortcut in **Active Reset** and press a new key to reassign it.
Confirmation sounds are configurable on the same page. Sector waits for driving
in offline Practice without EAC; checkpoints and results follow native
acknowledgements. Keys remain available to LMU, so choose unused game bindings.

There is no speed, distance or checkpoint-age cap. Compatibility, vehicle identity,
lifecycle and physical-state checks still apply. Rebuilding the vehicle can
invalidate a checkpoint; a failed save does not silently replace a valid one.

## 📊 Telemetry overlays

Seven widgets: **Leaderboard · Relative · Weather · Delta · Inputs · Laps · Session**.

- Toggle widgets, columns and sections, including clutch input and manufacturer logos.
- Set position, size, opacity and supported widget-specific options.
- Use **Edit layout** to drag and resize widgets in real time, then **Lock layout**.
- Choose a monitor in **Modifica → Impostazioni → Overlay**.

Locked overlays pass mouse input through to the game. They appear when LMU is in
the foreground with a loaded session and fresh telemetry. Use windowed or
borderless mode. Missing telemetry is shown as unavailable; editor samples are
never substituted into the live HUD. See [overlay data and limitations](docs/OVERLAYS.md).

## Get started

With a built portable package, open **`Sector.exe`** and keep the complete extracted
folder together. Node.js and Python are bundled; users do not need to install them.

1. Confirm the detected LMU folder or select it manually on first launch.
2. Choose the optional tutorial, available again through **Aiuto → Guida**.
3. For Active Reset, start LMU without EAC, load offline Practice and enter **GUIDA**.
4. Wait for the connection, focus LMU and release both shortcut keys before using them.

Closing the window asks whether to run in the background, quit Sector or cancel.
The tray or a second launch reopens the existing app. Background mode keeps the
listener active; quitting requests cooperative native cleanup.

Portable outputs are local under `releases/Sector-0.1.6-win-x64/` and excluded from
Git. A source download is not a ready-to-run portable package.
See [build and packaging](docs/RELEASE.md) for prerequisites.

## Verification status

The v15 host completed **seven live moving resets** on 16 October 2026, verifying
10,084 fields and rebuilding four contacts each time. Current speeds reached
242.61 km/h and checkpoint distances reached 797.36 m. The user confirmed
teleports including while cornering. The recorded first 20 physics steps per
reset showed no recurrence of the immediate collision jump.

This evidence does not qualify every car, track, damage state or long session.
Desktop and overlay automation uses isolated previews; complete live overlay
validation remains pending. See [current reset status](docs/PHYSICAL_RESET_STATUS.md),
[test coverage](docs/TEST_RESULTS.md) and the [collision investigation](docs/HIGH_SPEED_RESET_INCIDENT.md).

## Development

Work is committed to **`dev`**. **`prod`** is the default branch and receives only
explicitly approved promotions. The documentation on each branch describes that
branch's state.

For a UI-only preview on Windows with Node.js 22.12+ and npm:

```powershell
./scripts/launch/Start-ResetGui.ps1 -Preview
```

The source launcher installs locked dependencies and builds the renderer when
needed. First setup needs internet access. Preview does not attach to LMU.
The real reset backend additionally requires locally verified native binaries,
compatibility profiles and Python. Those prerequisites are not part of a public clone.

| Path | Purpose |
| --- | --- |
| `src/native/` | Physics, checkpoints, transactions, runtime and telemetry |
| `src/python/sector/` | Supervision, diagnostics, capture analysis and replay |
| `src/ui/electron/` | Desktop, overlays, onboarding and release builder |
| `tests/` | Native fixtures and Python tests |
| `scripts/` | Maintained launch, build, test and development commands |
| `docs/` | Current guides and an indexed research archive |
| `assets/` | Sector logo and Windows icon |
| `third_party/` | Vendored dependencies with their original notices |

See [contributing](CONTRIBUTING.md), [source layout](docs/SOURCE_LAYOUT.md) and the
[documentation index](docs/README.md). Captures, generated binaries, proprietary
game files, local compatibility manifests and toolchains stay outside Git.

## Support and license

If Sector is useful to you, give the repository a star. You can also support its
development through a **[voluntary donation on Ko-fi](https://ko-fi.com/illandiii)**.

Sector's original code and documentation are licensed under [MIT](LICENSE).
Dependencies and third-party assets retain their own terms; manufacturer marks
belong to their respective owners. See [third-party notices](src/ui/electron/THIRD_PARTY_NOTICES.md).
Sector is an independent project and is not affiliated with Le Mans Ultimate.
