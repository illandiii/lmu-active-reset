# Impact reset investigation

The 17 October 2026 packaged 0.2.5 test failed. This page records the evidence
and the v19 correction without treating successful memory writes as proof of
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

Before adding them to reset, capture the healthy, damaged and native-repaired
values and establish ownership, component identity and derived dependencies.
No calls to these repair routines or writes to their fields have been added.

## Remaining work and next live check

Front bodywork/aero state is outside the current checkpoint. Suspension
deformation recovery does not imply repair of those components. Punctures and
detached assemblies also remain unsupported.

Use a new offline Practice without EAC and the new packaged build, after
quitting the previous Sector process. Start with an ordinary save and restore
on a healthy vehicle. Then save again, reproduce one impact with wheels still
attached, and request one restore after the car has settled. Record the native
and physical traces before and after that restore. Verify teleport, saved
velocity, suspension alignment, continued driving and absence of a crash.
Stop after a refusal or instability and preserve the same vehicle for capture.

Only after that check should a separate trial restore during an active impact.
The stopped-car step isolates suspension recovery from concurrent collision
handling; it is a test sequence, not a product speed restriction. Bodywork
restoration needs its own implementation and evidence before it can be claimed.
