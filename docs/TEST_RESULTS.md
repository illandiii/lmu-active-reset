# Verification and evidence

This page separates automated checks from live-game observations. Historical
counts and build-specific experiments are preserved in the [research archive](archive/README.md).

## Paused startup: 0.2.8

The first 0.2.7 connection attempt returned native status/error `4/112` before
any save, restore or physical write. LMU reported `MultiStintState=PAUSED`
while `inControlOfVehicle=true` and `inMonitor=false`; SDK simulation time
remained unchanged. The native owner-discovery timeout expired, and the
shutdown summary confirmed every original hook target was restored.

The launcher now requires `MultiStintState=DRIVING` before loading the host.
Its two-sample admission restarts when pause or missing state interrupts the
sequence. The regression reproduces that sequence, and all 110 Python tests
pass. Native v20 is unchanged. Aerodynamic recovery is still awaiting a live
test; this failed startup is not a failed restore or an aero test result.

## Impact recovery: 0.2.6 live result and 0.2.7 candidate

The Oreca 07 / Monza 0.2.6 trial passed ordinary and settled-impact reset:
the user confirmed teleport, stable simulation and repaired suspension. The
front bodywork remained visibly damaged. Report 15 verified 10,097 writes and
four contacts; the measured hinge axes returned to their saved directions.
This does not yet verify reset during a continuing collision.

The native repair comparison found seven aerodynamic coefficients left damaged
by F7. Their repaired values and ten dependent caches are now included in v20.
The local fixture verifies all 10,114 fields on success and compensation after
contact/collision faults. It rejects damaged replacement saves, inconsistent
caches, nonfinite configuration and changed setup while preserving the previous
checkpoint. Loaded-host save/restore, missing-player refusal and restart pass.
All 109 Python tests, 46 desktop model tests and the isolated packaged 0.2.7
executable smoke test pass.
The 0.2.7 aerodynamic change has not yet run in LMU; visual mesh repair remains
unimplemented. The native-repair recording crossed a vehicle lifetime change
and cannot establish the graphical repair sequence on an unchanged generation.

The 0.2.4 impact capture reproduced a refusal at the wheel damage flags. SDK
samples distinguished attached deformation, puncture and wheel detachment.
The capture was read-only; the user was still running 0.2.4 during that test.

The 0.2.5 / v18 update passes:

- Analytical wheel deformation tests: independent quarter-turn rotation and
  world angular velocity expectations, invalid rotation data and detached-state
  refusal. Healthy checkpoint validation remains strict.
- Complete adapter restore with the measured front-right deformation vector,
  damage flag, saved constraint state and failure latch. Contact/rebuild faults
  verify compensation of all 10,093 fields; failed compensation quarantines.
- Loaded native host save/restore, missing-player refusal and clean restart.
- 109 Python tests and 46 desktop model tests.
- Packaged 0.2.5 executable startup, embedded Python imports, UI and overlays
  in an isolated preview smoke test.

**The packaged 0.2.5 impact test failed.** Two restore commands on the Alpine
A424 wrote and verified all 10,131 planned fields, but the second, requested
during an impact, was followed by a game crash (`0xC0000005`). The user saw
suspension recovery while the front bodywork remained damaged. The physical
trace shows front-left tire divergence beginning inside the first resumed
Dynamics call. The dump records a tire lookup fault at executable RVA
`EF394D`; it does not by itself establish which missing state caused divergence.

Offline analysis then found a concrete omission: native suspension damage
updates PureHinge's body-B reference axis at `+0x68`, while the restore plan
only restored the angular row basis at `+0xA8/+0x150`. Version 0.2.6 / v19
includes the two reference axes at `+0x50/+0x68` in capture, preflight, restore,
readback and compensation. The snapshot file ABI is unchanged.

The adapter fixture now changes the body-B axis independently of the restored
row basis, verifies its corrected alignment, and checks compensation after
contact/collision failures. The recorded fixture has four hinges and 10,097
planned fields. Zero-length and nonfinite axes reject a replacement save while
preserving the previous checkpoint. The subsequent live result is recorded above.
Visible bodywork, detached suspension and puncture repair remain incomplete.

V19 also passes loaded-host save/restore, missing-player admission refusal and
two successive clean host lifetimes. All 109 Python tests, 46 desktop model
tests and the isolated packaged 0.2.6 smoke test pass.

See [impact investigation](IMPACT_RESET_INCIDENT.md) for evidence and the next
test procedure. Do not use 0.2.5 to repeat the impact test.

## Live check: 17 October 2026

Packaged 0.2.4 and native v17 connected in offline Practice without the earlier
resident-identity errors. The BMW M Hybrid V8 at Monza Curva Grande completed
two saves and eight restores, with every command successful. Each restore
verified 10,123 fields and rebuilt four contacts without compensation. The
800-record trace contains no gaps or nonfinite body values; saved and applied
pose, velocity and angular momentum match on all five traced bodies.

Changing to an Oreca 07 in the same game process also passed re-admission with
stopped v17 modules resident. The first five P2 restores succeeded with 10,085
verified fields, four contacts and no compensation. This checks the two different
vehicle plan sizes and live resident-module reuse checks.

SDK data contained 37 cars, including eleven P2 and fifteen GT3 entries. Every
entry resolved to a manufacturer logo through the production model. The user
confirmed the combined save/restore and logo test. This is one live session,
not qualification of all cars, impact recovery or long-session logo continuity.
See [physical reset status](PHYSICAL_RESET_STATUS.md) for the recording reference.

## Automated coverage

The 0.2.4 update passed 46 Node tests and the native SDK serialization check.
Regressions cover cached per-car identity during missing telemetry, invalidation
on car/session changes and removal of Brake lock from existing saved layouts.
Tyre flat-spot coverage remains active. The desktop and ten-widget Electron
smoke checks passed, along with Home and the packaged 0.2.4 EXE. Documentation
screenshots were regenerated for the ten remaining widgets. The live check above
adds in-session evidence; longer-session logo continuity remains to be qualified.

The latest reported unknown-v17 error came from the 0.2.2 runtime logs, not the
0.2.3 supervisor. The 0.2.3 runtime had no reset logs. Launching a new executable
while an older instance owns the desktop lock previously reopened that older
instance silently; 0.2.4 adds an explicit notice to quit it before changing versions.

The 0.2.3 update passed 108 Python tests and 49 Node tests. New regressions cover
current and previous-release resident identity, changed DLLs/profiles/manifests,
unrecognized paths, active or failed modules, v17 IPC and restored hook targets.
Logo tests cover team-only names, per-car models, vehicle filenames and missing
opponent identity. The native SDK fixture also checks stale and duplicate model
associations. Native SDK serialization, Electron desktop/overlay/Home suites and
the packaged 0.2.3 EXE smoke test passed. The mixed-class leaderboard screenshot
also confirms visible Oreca, Ligier and McLaren logos with team-only names.
Live reset reconnection and in-session logos still need validation.

The 0.2.2 update passed 48 Node tests, 100 Python tests and the native host's
save/prepare/restore, callback-refusal and restart fixtures. Native PREPARE now
performs complete read-only preflight and reports its verified field count.
Python regressions cover different vehicle plan sizes, partial or absent
verification, failure replies, stale identity and duplicate acknowledgements.
Desktop, overlay and Home smoke suites passed with the new pedal-limit model.
The packaged 0.2.2 EXE passed isolated startup, bundled Python imports, UI and
overlay smoke checks.
Live V17 recovery and threshold accuracy still require in-game validation.

| Area | Entry point | What it verifies |
| --- | --- | --- |
| Native core | `scripts/test/Test-Core.ps1` | State mathematics, restore plans, transaction/rollback and coordination fixtures |
| Native host | `scripts/build/Build-PhysicalResetHost25661166.ps1` | Native plan verification, contacts, collision reconstruction, callback refusal and restart cases |
| Python | `python scripts/sector.py test` | Supervision, acknowledgements, key behavior, lifecycle, analysis and source layout |
| Desktop/controller | `npm test` in `src/ui/electron` | Settings, discovery, runtime ownership and overlay models |
| Desktop/widgets | `npm run test:ui` | Real Electron interactions in isolated preview, including all ten widgets |
| SDK/widget integration | `npm run test:telemetry` | Compiled native reader's synthetic SDK frame through production wheel and stint models |
| Onboarding | `npm run test:onboarding` | Automatic/manual selection, invalid/canceled paths, tutorial choices and persistence |
| Startup | `npm run test:startup` | SVG drawing, automatic exit, click/key skip, reduced motion and no replay on background restore or reload |
| Portable package | `npm run test:release` | Actual EXE in preview, version, bundled Python imports, UI and no active reset listener |

The 0.2.1 update passed 46 Node unit tests, 98 Python tests, desktop, overlay,
Home and packaged EXE smoke checks, formatting and the renderer build. Native adapter
fixtures verify restoration of failure code 4 and preservation of that code
during compensation. Host fixtures verify the full plan, missing callbacks and
restart. The restore-flow fixture also rejects stale/unknown failure values,
duplicate latch fields and attempts to restore a nonzero failure. Live impact
recovery remains unqualified. Overlay smoke checks cover geometry synchronization
during a drag, class badges and manufacturer logos for all four classes.

The 0.2.0 Home update passed desktop, Home, overlay, onboarding and packaged EXE
smoke checks. Startup-specific coverage last passed with 0.1.10.
The unchanged native SDK reader last passed its self-test and
serialization fixture with 0.1.9. These are recorded development results, not an
automatically updated CI status.

Desktop reset fixtures cover single-use requests, duplicate/busy rejection,
host identity, expiry, shutdown and native acknowledgement correlation. Home
checks verify that overlay toggles preserve other buttons, clicks do not open
key capture, and every release is accessible in Help > Changelog. The new
desktop click path has not yet been exercised in a live LMU session.

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
