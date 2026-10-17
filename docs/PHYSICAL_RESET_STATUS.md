# Physical reset: current status

**Development desktop: 0.2.7. Native host: v20 for the verified Steam 25661166 /
app build 14200 compatibility profile.** Matching the display version alone is
insufficient: runtime admission also checks the pinned identities and hashes.
The 0.2.5 impact trial crashed LMU. V19 passed the subsequent ordinary and
settled-impact trial on the Oreca 07. V20 adds aerodynamic damage recovery;
it has passed local tests but has not yet been validated in LMU.

## Implemented behavior

F6 captures a checkpoint; F7 prepares and restores the saved physical state at
the admitted native callback boundary. Physics/scoring coordination, object
lifetime checks, contact reconstruction and collision-state rebuilding are part
of the restore sequence. V17 validates every field of the vehicle-specific plan
and rebuilds **four tire contacts** before simulation continues. The field count
depends on the car's body/constraint topology and engine integration channels.

The saved pose and velocity are restored, rather than combining the saved
position with the velocity at the moment of F7. There are no speed, distance,
session-duration or checkpoint-age caps. Compatibility, lifecycle, finite-state
checks remain active. Checkpoints are tied to the vehicle
generation and can be invalidated by reconstruction, even at the same address.

## Reconnecting after an app update

The 0.2.2 launch logs recorded `Resident identity mismatch` before a host was
started. The audit incorrectly required a resident DLL to live in the current
release directory and did not register hosts v16/v17. Version 0.2.3 resolves each
resident module against its original build. Earlier managed runtime directories
must match their content-addressed release manifest, profile and DLL hashes.
All resident modules must still acknowledge a clean stop, and their original
hook targets must be restored before a new host can connect. Unknown, modified
or still-active modules are refused. This does not reuse a previous checkpoint.

## Preparation without teleporting

The 17 October `20261017T140149268846Z` recording from desktop 0.2.0 contains
a successful save followed by nine successful native PREPARE replies with
10,122 fields, but no RESTORE commands or physical writes. The launcher expected
10,084 fields from a different vehicle. V16 had retained the same fixed-count
assumption with one extra failure-latch field.

V17 performs a read-only native preflight after preparing the complete typed
plan. It reports the number of verified fields only after all bindings, writable
regions, expected values and owner context pass. Python requires that verification
to match the actual plan count, rather than comparing against another car's count.
RESTORE still builds and validates a fresh plan on the physics thread before
writing. Request identity, focus/context and timeout checks remain in place.

Regression coverage includes the incident's differing plan counts, missing or
partial verification, stale requests, host restart and the complete native
save/prepare/restore path. The packaged 0.2.4 live test below confirms V17 admission
and repeated restores on the tested car and circuit.

## Recovery after an impact

The 17 October 0.2.4 capture identified a second refusal beyond the failure
latch: host v17 rejected either wheel flag at offsets `+0x301/+0x302`.
Report 16 in `20261017T144553439928Z_physical_reset_host_v17` returned cause
115 with no plan and no writes. The read-only follow-up observed attached wheels
with deformation flag 1, followed by a rear-right puncture and then detachment.
Those are distinct conditions, not an identity mismatch.

Version 0.2.5 / v18 admits **attached, deformed wheels** only after validating
their orientation and world angular velocity against the native axis-angle
deformation and wheel spin. It restores the saved deformation flag, rotation
vector and accumulated suspension damage together with the existing saved
constraint axes, rigid state and failure latch. All eight added fields are
included in expected-value preflight, readback and compensation. The legacy
snapshot ABI stays unchanged, and saving still requires a healthy checkpoint.

Static analysis of the locally captured build identifies `EE4010` as the
deformed wheel projection, `ECEF30` as the impact deformation update, and
`ECF1F0/EEB650` as the suspension repair/update paths. The local regression uses
the measured deformation vector and verifies both successful restore and
rollback. **The live impact test of 0.2.5 failed:** after the second restore,
the front-left tire diverged and LMU crashed in a tire table lookup. The user
also confirmed that the front bodywork remained damaged.

V19 additionally saves both PureHinge reference axes at `+0x50/+0x68`.
Native impact handling changes the body-B axis at `+0x68`; restoring the
row basis alone leaves that damaged reference active during the next bias
update. These axes now participate in the same validated transaction and
compensation as wheel deformation. Local tests pass with 10,097 fields for
the recorded fixture. The 0.2.6 live trial restored the suspension and teleported
without a reported crash, but left the front visibly damaged. Restoring during
an ongoing collision is still unverified. See the [impact report](IMPACT_RESET_INCIDENT.md).

V20 includes seven aerodynamic coefficients and ten dependent caches, with
finite/coherence checks and unchanged setup dependencies. All 17 fields take
part in preflight, readback and rollback. The fixture now restores 10,114 fields.
The change follows measured damaged/repaired values and native repair analysis.
It does not yet repair the visible bodywork mesh.

Detachment remains unsupported: native `EEB800` also changes the active
constraint set. Merely clearing its flag would leave a broken suspension.
V18 reports the detached corner explicitly and preserves the existing
checkpoint. Visible bodywork repair, puncture repair and missing-assembly
reconstruction are not implemented. Aerodynamic recovery still needs a live v20 test.

### Earlier failure-latch correction

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
collision rebuild failure and compensation. V18 extends that fixture with
deformed wheel state, its derived caches and the 10,093-field plan for the
recorded assembly. Native host restart and missing-player refusal also pass.

## Collision correction

The earlier high-speed launch was traced to a collision sweep spanning the old
and checkpoint positions. V15 restores aggregate/exported kinematics and uses
the native collision helpers to rebuild geometry and clear obsolete collision
pairs before physics resumes. This reconstruction is also used during transaction
compensation. A field-equality check alone is not a stability test.

See the [collision investigation](HIGH_SPEED_RESET_INCIDENT.md) for the chronological
evidence, the initially blocked v15 launcher and the subsequent successful trials.

## Live evidence

On 17 October 2026, packaged Sector 0.2.4 with host v17 connected to offline
Practice on Monza Curva Grande Circuit with the BMW M Hybrid V8. Recording
`20261017T144326039314Z_physical_reset_host_v17` contains two successful saves
and eight successful restores. Each restore verified all 10,123 plan fields,
rebuilt four contacts and required no compensation. All commands returned success;
the supervisor error log remained empty. The user confirmed that the test worked.

The 800-record physical trace has no gaps or nonfinite body values. At application,
all five traced bodies matched the saved position, velocity, orientation and
angular momentum exactly. This qualifies these observed resets; impact recovery,
other vehicle assemblies and longer sessions still need separate testing.

After changing to the Oreca 07 in the same LMU process, recording
`20261017T144553439928Z_physical_reset_host_v17` shows successful re-admission
with two cleanly stopped v17 modules already resident. Its first five restores
all succeeded, verified 10,085 fields and rebuilt four contacts without
compensation. This also exercises the vehicle-dependent plan count and resident
module audit in the packaged app, rather than only in fixtures.

The same session supplied 37 identified cars across Hypercar, LMP2 and GT3. The
production logo resolver recognized every car, including team-only scoring names.
The user confirmed the combined F6/F7 and logo test. The diagnostic SDK reader
stops independently of Sector's listener and does not send reset commands.

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
