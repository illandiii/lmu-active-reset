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
| Desktop/widgets | `npm run test:ui` | Real Electron interactions in isolated preview, including all eleven widgets |
| SDK/widget integration | `npm run test:telemetry` | Compiled native reader's synthetic SDK frame through production wheel and stint models |
| Onboarding | `npm run test:onboarding` | Automatic/manual selection, invalid/canceled paths, tutorial choices and persistence |
| Startup | `npm run test:startup` | SVG drawing, automatic exit, click/key skip, reduced motion and no replay on background restore or reload |
| Portable package | `npm run test:release` | Actual EXE in preview, version, bundled Python imports, UI and no active reset listener |

The 0.1.10 startup update passed 41 Node unit tests, desktop, Home, overlay,
onboarding and startup smoke suites, formatting, renderer build and packaged EXE
smoke checks. The unchanged native SDK reader last passed its self-test and
serialization fixture with 0.1.9. These are recorded development results, not an
automatically updated CI status.

Deployment-map fixtures cover LMH and LMDh model recognition, both SDK encodings,
range boundaries, valid recommendation steps, unknown vehicles and clearing
learning after a vehicle change. Electron verifies the compact map row without
step indicators. This update has not yet been validated in a live LMU session.

New fixtures cover old-layout migration, per-wheel brake-lock detection, tyre
temperatures and replacement, measured fuel/energy consumption, lap reserves,
missing data and reset acknowledgements. Flat spot fixtures exercise sustained
locks, sensitivity, rotation, load, off-road/airborne wheels, short spikes,
duplicate timestamps, missing frames and history reset. Additional fixtures cover
speed/slip-angle/pedal-based lock forecasting, player placement in every class,
per-car resources, hybrid power/state, clean-lap learning and map recommendations.
Electron checks all eleven widget windows, visibility switches, shrinking geometry,
adjustable flat spot sensitivity and a Hypercar player inside the Hypercar section.
Screenshots use sample data. Flat spot and lock-prediction thresholds still require
live-game calibration; these tests do not establish real damage-detection accuracy.

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
