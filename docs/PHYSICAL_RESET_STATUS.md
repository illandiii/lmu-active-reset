# Physical reset: current status

**Current desktop: 0.2.1. Native host: v16 for the verified Steam 25661166 /
app build 14200 compatibility profile.** Matching the display version alone is
insufficient: runtime admission also checks the pinned identities and hashes.

## Implemented behavior

F6 captures a checkpoint; F7 prepares and restores the saved physical state at
the admitted native callback boundary. Physics/scoring coordination, object
lifetime checks, contact reconstruction and collision-state rebuilding are part
of the restore sequence. The v16 plan verifies **10,085 fields** and rebuilds
**four tire contacts** before simulation continues.

The saved pose and velocity are restored, rather than combining the saved
position with the velocity at the moment of F7. There are no speed, distance,
session-duration or checkpoint-age caps. Compatibility, lifecycle, finite-state
checks remain active. Checkpoints are tied to the vehicle
generation and can be invalidated by reconstruction, even at the same address.

## Recovery after an impact

The 17 October runtime stopped at startup with `Vehicle terminal failure 4`.
The reader also rejected any nonzero failure latch before restore preparation.
V16 keeps the connection available and admits the damaged current frame for
restore preparation. Saving still requires a healthy frame.

The shared failure latch at `game_vehicle+0x1cf9c` (scoring `+0xd4`) is now
restored with the engine, transmission, rigid bodies and collision state in the
same transaction. It participates in expected-value preflight, readback and
rollback. A failed restore puts back the pre-restore failure value too.
This is recovery to an existing healthy checkpoint, not a new checkpoint
created after the accident.

The adapter fixture covers failure code 4, successful restore, contact failure,
collision rebuild failure and compensation. The host fixture covers the complete
10,085-field path and restart. **V16 has not yet been tested after an actual
impact in LMU.** Detached wheels, changed topology and invalid mechanical
geometry still cannot be restored by this path; recreating missing assemblies
is not implemented.

## Collision correction

The earlier high-speed launch was traced to a collision sweep spanning the old
and checkpoint positions. V15 restores aggregate/exported kinematics and uses
the native collision helpers to rebuild geometry and clear obsolete collision
pairs before physics resumes. This reconstruction is also used during transaction
compensation. A field-equality check alone is not a stability test.

See the [collision investigation](HIGH_SPEED_RESET_INCIDENT.md) for the chronological
evidence, the initially blocked v15 launcher and the subsequent successful trials.

## Live evidence

The 16 October 2026 recording contains seven restores from one checkpoint saved
at **178.67 km/h**. Current speeds ranged from **191.37 to 242.61 km/h**, and
distances from **67.11 to 797.36 m**. Every restore verified all 10,084 fields,
rebuilt four contacts and required no compensation. The user confirmed successful
teleports, including in turns.

The capture contains 696 physical records with no gaps or nonfinite body values.
The detailed windows cover 20 Dynamics steps per restore, 140 in total, without
the previous immediate collision jump. The listener was deliberately stopped
after collection, and native cleanup completed with LMU still running.

## Remaining qualification

- Longer sessions with repeated resets, broader vehicle/track coverage and lifecycle transitions.
- Lap validity, timing records and damage-related behavior under live resets.
- The complete packaged desktop/background workflow during extended live play.
- Live overlay behavior on the user's monitor configuration.

Desktop previews and owned-memory fixtures establish different properties from
these live checks. Use the [manual test procedure](TEST_RESET_COLLISIONI.md) to
extend the evidence and the [test index](TEST_RESULTS.md) for automated coverage.
Earlier contracts and observer-only stages remain in the [research archive](archive/README.md).
