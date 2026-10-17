# Verification and evidence

This page separates automated checks from live-game observations. Historical
counts and build-specific experiments are preserved in the [research archive](archive/README.md).

## Automated coverage

| Area | Entry point | What it verifies |
| --- | --- | --- |
| Native core | `scripts/test/Test-Core.ps1` | State mathematics, restore plans, transaction/rollback and coordination fixtures |
| Native host | `scripts/build/Build-PhysicalResetHost25661166.ps1` | Revision-specific host fixture, including v15 field count, contacts, collision reconstruction and restart cases |
| Python | `python scripts/sector.py test` | Supervision, acknowledgements, key behavior, lifecycle, analysis and source layout |
| Desktop/controller | `npm test` in `src/ui/electron` | Settings, discovery, runtime ownership and overlay models |
| Desktop/widgets | `npm run test:ui` | Real Electron interactions in isolated preview, including all seven widgets |
| Onboarding | `npm run test:onboarding` | Automatic/manual selection, invalid/canceled paths, tutorial choices and persistence |
| Portable package | `npm run test:release` | Actual EXE in preview, version, bundled Python imports, UI and no active reset listener |

The latest 0.1.6 desktop verification passed 20 Node unit tests, the desktop and
overlay smoke suites, formatting, renderer build and packaged EXE smoke checks.
The overlay suite checks resizing before mouse release, fixed Edit/Lock width,
display persistence and conditional accent controls. These are recorded results
for that development stage, not an automatically updated CI status.

The native v15 fixtures include a 1,000-metre displacement with changed heading
and saved/current velocities of 80/-100 m/s, all 10,084 fields, four contacts,
missing-callback refusal and consecutive host instances. They model native
collision helpers on owned memory; they cannot prove game behavior by themselves.

## Live reset evidence

On 16 October 2026, seven v15 restores succeeded in the captured LMU session.
All fields verified, four contacts were rebuilt per reset and no compensation
ran. The current speed reached 242.61 km/h, with distances up to 797.36 metres
from the saved point. The user confirmed successful teleports, including turns.

The 20 recorded Dynamics steps after each restore showed no recurrence of the
immediate collision jump. This is a short post-restore observation window, not
proof of universal or long-session stability. See [current status](PHYSICAL_RESET_STATUS.md)
and the full [collision investigation](HIGH_SPEED_RESET_INCIDENT.md).

## What still needs live testing

Broader cars/tracks, repeated long-session restores, lifecycle changes, lap
validity and damage behavior remain to be qualified. The complete desktop and
overlay workflow also needs in-game testing; sample widgets and preview gating
checks are not evidence of a working HUD on every display configuration.

Follow the [manual test procedure](TEST_RESET_COLLISIONI.md). Retain rejected
commands, failures and trace gaps in reports. Native field equality, a success
sound and an unchanged process exit code each answer different questions; none
alone establishes stable vehicle motion.
