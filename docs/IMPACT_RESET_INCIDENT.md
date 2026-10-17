# Impact reset investigation

The 17 October 2026 packaged 0.2.5 test failed. This page records the evidence
and the v19/v20/v21 changes without treating successful memory writes as proof of
physical stability. Raw dumps, captured game code and telemetry remain local
and excluded from Git.

## Observed failure

The Alpine A424 at Monza Curva Grande saved a healthy checkpoint. Native log
`20261017T150827679163Z_physical_reset_host_v18` contains restores 4 and 6,
each with 10,131 verified writes and four rebuilt contacts. Restore 4 resumed
normally in the captured 20-step window. Before restore 6, the game recorded
front-left and rear-left suspension deformation. All four wheels remained
attached and the SDK reported no punctures.

Restore 6 reproduced the saved pose, velocity, orientation and momentum on
all five traced bodies. The deformation flags returned to zero. However,
the front-left tire's velocity started diverging inside the first Dynamics
call and oscillated with increasing magnitude over the next 20 calls. The
captured collision correction calls did not alter those body states.

The game subsequently logged an unexpected additional displacement and
submitted a BugSplat crash. The minidump reports a read access violation
at executable RVA `EF394D`, called from the tire routine containing `EF50C6`.
The failing instruction indexes a table with an invalid large index, and
the exception stack contains nonfinite floating-point values. This is
consistent with numerical divergence reaching a lookup; it does not identify
the initial cause by itself.

The user observed suspension recovery but no repair of the front bodywork,
then a game crash when restoring during the impact.

## Identified missing suspension state

Analysis of the hash-verified local code capture shows:

- `ECEF30` updates wheel deformation and calls `ECEC70`.
- `ECEC70` projects wheel bodies, finds affected angular joints and invokes
  their vector setter with parameter 3.
- For PureHinge, that setter is `3F4D90`. Parameter 3 normalizes and writes
  the body-B reference axis at `+0x68`; the getter `3F52E0` reads it there.
- The native joint update `3F57C0` reads that reference separately from the solver
  row basis at `+0xA8/+0x150`.

The v18 restore plan restored the row basis and solver histories but omitted
the physical reference axes at `+0x50/+0x68`. Thus restoring a healthy pose
could leave the impact-modified alignment driving the next constraint update.
This is a concrete code omission. Its contribution to the live crash still
requires validation of the corrected build.

V19 captures both reference axes under the same physics/control locks as the
checkpoint. It checks finite, unit-length axes and restores all six doubles
as one typed transaction field per PureHinge. Preflight, readback and rollback
cover that field. No native damage setter, retry, speed limit or guessed cache
clear is used. Existing snapshot files retain their ABI.

The fixture changes the body-B axis by a quarter turn independently of the
solver rows. It verifies the restored alignment and checks that contact or
collision failure restores the original damaged axis instead. Invalid axes
cannot replace a valid checkpoint. This fixture models memory transactions;
it does not run LMU's native solver.

## Bodywork investigation

The native repair path at `ECF380` restores additional vehicle parameters and
refreshes derived values through `ED8130`, `ED7DE0` and `ED8070`. The vehicle
service path containing `F5E63E` performs those updates separately from wheel
repair. Component reattachment at `F6DE50` also changes global object lists
and recursively handles related components. Those operations cannot be
replaced by simply clearing a damage byte.

The 0.2.6 Oreca recording confirms seven coefficients remain altered after
successful F7. After the user's native repair, all seven returned exactly to
the initial baseline and the user reported an intact front. The recording also
contains a transition out of GUIDA and a new host lifetime. The physics address
was reused. This is evidence of the repaired values, not proof of an uninterrupted
pit-service operation on the same vehicle generation.

V20 captures those seven coefficients and ten dependent caches under the existing
physics/control locks. Its sidecar validates finite values and the dependencies
computed by ED8130, ED8070 and ED7DE0. All 33 read-only setup inputs must still
match the checkpoint before restoration. The 17 writes use the normal expected
values, preflight, readback and compensation. Saving damaged aerodynamic values
cannot replace a healthy checkpoint. No native repair function is called, and
the snapshot ABI is unchanged.

| Primary field | Repaired value | Damaged value after F7 with v19 |
| --- | ---: | ---: |
| Physics +39D8 | -0.45014228 | -0.43189216455 |
| Physics +39F0 | -0.1380634 | -0.12378070095 |
| Physics +3A08 | -0.5527169 | -0.54522818285 |
| Physics +3A20 | 0.1959759 | 0.20183663516 |
| Physics +3A38 | 0.0996095 | 0.16069502279 |
| Physics +3A48 | 0.36494141 | 0.41274747132 |
| Physics +40D8 | -0.759812 | -0.72195782680 |

These are build-specific observations, not universal vehicle constants.
The test fixture uses this measured difference and verifies rollback as well
as forward restoration. The subsequent v20 capture below confirms the tested
aerodynamic restore in LMU.

## Live v19 result

In `20261017T153328903460Z_physical_reset_host_v19`, restore report 15 wrote
10,097 fields and rebuilt all four contacts. The front-left, rear-left and
rear-right body-B hinge axes returned to their saved directions and all wheel
deformation flags cleared. The user confirmed teleport and continued stability,
with the front bodywork still damaged. The trace contains no nonfinite body
values. This validates the tested settled-impact recovery only; restoring
during a continuing impact still needs a separate trial.

The component flags at collision-vehicle +18B40 did not change in the damaged
capture. The collision subobject starts at game-owner +8; using the owner's
address directly mislabels these fields. The initial capture's component data
had this offset error and is excluded from the conclusion. The corrected
capture identifies visual deformation separately from a detached-component flag.

## Live v20 result and v21 bodywork integration

The 0.2.8 recording `20261017T160130779569Z_physical_reset_host_v20` contains
ordinary and impact restores with 10,114 verified fields and four contacts.
Both damaged restores returned all 17 aerodynamic fields to their saved bytes.
The user still observed damaged front and rear bodywork.

The graphics comparison found four CPU meshes and 6,408 changed attribute bytes.
Native repair restores these buffers and requests a render upload under each
component's critical section. V21 implements that path as a journaled sidecar
to the physical transaction, with a separate lifetime gate for model loading,
asynchronous publication, impact and repair updates. The original vertex data,
topology and generation must still match the healthy checkpoint.

Local replay, write-fault recovery, thread synchronization and loaded-host tests
pass. Visible bodywork repair in the game remains unverified. Punctures and
detached assemblies remain unsupported. The next test is an ordinary F6/F7,
then one settled front or rear impact with wheels still attached, using 0.2.9.
See [Bodywork restore](BODYWORK_RESTORE.md) for synchronization details, limits
and the exact procedure. Restoring during an active impact needs separate
qualification after the settled-impact test.
