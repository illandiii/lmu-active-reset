# High-speed reset and launcher lifecycle

Latest result, 16 October: the v14 live trace identifies a new collision sweep
across the teleport, followed by native collision correction. V15 contains a
targeted repair, tested on owned memory but awaiting a live confirmation. The
earlier sections below retain the evidence available at each investigation stage.

## Confirmed incident

Local recording `20261013T210817261273Z_physical_reset_host_v8` contains three
successful reported restores. Command 8 restored from 56.427771 m/s to a checkpoint
captured at 48.278566 m/s. All 10,077 fields matched before forwarding the original
update, four native contact calls returned successfully, and no compensation ran.

The body position before forwarding matched the saved position exactly:
`[-34.3666028075, -14.8895678940, -110.1202970746]`.
Telemetry at elapsed 58.952188 s then reported world height 2.731146574, 16.730891944 m
above the previous telemetry sample, with angular rates approximately
`[-7.806, -0.923, -12.413]` rad/s. Recorded world height later peaked at 111.6228333.
Thus field equality is not sufficient evidence of physical stability.

The current contact validator checks finite geometry, positive contact depths,
normal lengths, surface references, flags and unchanged tire-body position.
It does not prove the post-teleport contact/solver impulse history is coherent or
measure the first resumed physics step. The fixture substitutes a contact model;
its high-speed success did not exercise this native instability.

The evidence narrows the fault to state outside the verified plan and/or the first
resumed update, but does not identify the responsible field or routine. Stale
contact/solver history is a hypothesis, not an established root cause. No arbitrary
cache clearing or extra reset has been added. A native first-step capture of contact
depths, normals, velocity and solver impulses is needed to distinguish these paths.
LMU was closed during this investigation; the high-speed defect remains unresolved.

## V9 launcher changes

The launcher waits in menus/monitor until offline Practice and player control are
observed. Existing identity and anti-cheat checks still run before loading code.
The console-close handler revokes admission and requests native stop, waiting up
to three seconds for shutdown. Abrupt termination still expires the native lease
and watchdog. Hotkeys are owned by the console process and cannot keep listening
after it exits.

Each launch loads an identical hashed v9 binary under a unique UUID filename and
uses a mapping derived from that filename. Reopening audits stopped instances and
all original hook targets before starting a new one. It waits briefly for a prior
console's shutdown; active or quarantined modules are not bypassed. Checkpoints are
not shared between launches. Old module storage remains until the game exits, so
each restart retains another module allocation rather than hot-unloading code.

Tests passed: menu/monitor-to-driving wait, interruptible wait, native discovery,
missing-callback shutdown, and two successive independent host instances in the
same fixture process with a full restore and clean stop in each. Console-window
close and reopening against live LMU remain to be confirmed by a user session.

## V10 physical trace

The capture adds a separate append-only shared buffer, limited to 512 records.
No disk I/O occurs on the physics thread. The external runner drains records into
`*.physical.jsonl`. Each record contains the chassis and four tire-body positions,
linear velocities, orientation matrices and angular momentum; tire contact counts,
minimum/maximum depth and the normal of the deepest contact are also recorded.
Validity bits distinguish unavailable body/contact reads. Raw solver impulses are
not included in this capture.

Phases: 1 before SAVE, 2 before RESTORE, 3 after restore/contact reconstruction but
before resuming the original update, 4 after the outer update, 5 before Dynamics,
6 after Dynamics. Phases 5/6 cover the first 20 player Dynamics calls after reset.
Ticks and request IDs associate samples with the command. The fixture verifies
that phase 3 contains saved velocity (80 m/s) and phases 5/6 are captured; clean
shutdown and two-instance restart tests also pass. The launcher Path import needed
when leaving its wait loop is restored.

## Live reproduction, 14 October 2026

Local capture `20261014T213818947304Z_physical_reset_host_v10` recorded SAVE 1,
PREPARE 2 and RESTORE 3. Saved speed was 46.634505 m/s (167.88 km/h); restore was
requested at 57.681654 m/s (207.65 km/h). All 10,077 fields matched before native
execution resumed, four contact calls completed, and no recovery ran.

The physical trace contains 33 records with all nine validity bits set. At phase 3,
all 18 captured components of each of the five bodies exactly equal phase 1.
In particular the saved velocity is restored; current velocity retention does not
explain this reproduction. Initial maximum contact depths remain near the saved
values: 0.0230, 0.0235, 0.0294 and 0.0295 metres.

Four Dynamics calls return without the large launch. Between exit of call four
(phase 6, remaining step 17) and entry of call five (phase 5, step 16), the chassis
moves approximately 10.89 metres and its orientation changes. Its velocity becomes
`[-4.1083, -28.7502, -36.2084]` m/s while its speed magnitude remains 46.416569 m/s.
One common rigid transformation, fitted from the root orientation and position,
also reproduces the four tire positions, all five velocities and angular momenta
to numerical precision (maximum residuals below 7e-14 m, 6e-13 m/s and 2e-13
angular-momentum units respectively). This is evidence of a common pose/vector
transformation outside the captured Dynamics interval, preceding the subsequent
divergence. It does not identify the writer, caller, or stale input responsible.

The following Dynamics calls diverge. Chassis vertical velocity reaches +156.58
m/s, then becomes strongly negative; a recorded contact depth reaches 116.95 m.
The last record is an entry into Dynamics without its matching exit. LMU's own
trace independently logs the initial 241.94 m teleport, a second 11.02 m teleport
(different exported position reference), and `Crash submitted to bugsplat`.
LMU was no longer running when inspected. No crash exception/stack was obtained;
the terminating instruction is not established by these records.

The old summary's `Host quarantine or hook shutdown failure` is not evidence of
quarantine: host status stayed at 2 with error 0 when the game exited. The runner
now queries its original process handle and records process exit separately,
including the exit code and null hook-restoration results when the process is gone.
It drains the shared physical trace after exit. A successful write acknowledgement
now explicitly says physical stability has not yet been verified.

`analyze_physical_reset_trace.py` reproduces the saved/applied comparison, per-call
changes, between-call changes and common rigid-transform residuals. Its tests cover
an exact common rotation, an independent impulse, invalid bodies and incomplete
call pairs. The physical reset defect is still unresolved; no guessed cache write,
extra reset, speed clamp or automatic retry has been added.

## V11 diagnostic boundaries

The same existing hooks now also capture before/after every outer update (phases
7/8) and before/after player scoring (9/10) during the 20-step post-reset window.
Sampling still requires the physics thread, its worker lock and matching roots.
No additional hook targets or physics writes were introduced. PREPARE no longer
changes the request ID of an ongoing physical trace. These boundaries are intended
to locate the observed between-call transformation more precisely in a future
capture; they have not yet been exercised in live LMU.

Validation: 16 Python tests passed, including exit-code detection on an owned
terminated process. The v11 native fixture passed full restore,
all added trace phases, absent-player refusal and two-instance restart. These
fixtures do not model LMU's transform/collision pipeline or establish that its
high-speed instability is fixed.

## Offline follow-up: candidate collision correction path

The hash-verified live-code image contains a concrete path capable of the common
rigid transform seen in the trace:

* `CC8210` calls `FEFC60` when the native step counter is divisible by eight.
* `FEFC60` is identified by its native profiling string as
  `CollObjectManager::PostInputDyn`; it calls `F4EAC0` on vehicle collision entries.
* `F4EAC0` checks vehicle-relative byte `16CFA`, compares collision-body position
  at `16D40` with exported position at `15A0C`, constructs a pose correction and
  calls `ECFFC0` at `F4EF63`.
* `ECFFC0` calls `ED02C0`, which transforms the body-vector entries and four tire
  bodies. Its chained unwind fragment at `ED03E0` contains the body loop.
* `F4E7B0` prepares the separate collision body at vehicle-relative `16D00` from
  exported vehicle state and clears `16CFA`; `FF0150` contains writes setting that
  flag. These are separate from the tire contact nodes inspected during restore.

The instruction evidence is retained locally under
`work/reset-incident-disassembly/`, including direct-call references and decoded
field references. The frame's numerical evidence is consistent with this path,
but does not record its execution or establish which collision data became stale.
Do not clear the flag or replay a collision update as a speculative fix: its
ownership, timing and effect on pending collision pairs must be established.

REA/Ghidra analysis of `F47B30`, `F59B50`, `CC7D90`, `F48F20`, and `F49FC0` completed
successfully in `analysis/rea-25661166/reset-transform-20261014-configured/`.
`F59B50` reads the HDV pose and updates exported vehicle values, helping distinguish
that output path from collision feedback. Raw game code and decompilation remain
local and are not distributed with the repository.

## V12 collision instrumentation and offline findings (15 October)

REA/Ghidra completed the collision-path batch under the local directory
`analysis/rea-25661166/collision-sync-20261014/`. Its decompilation agrees with
the instruction evidence above. `F4EAC0` applies a pose correction and writes
six force/torque feedback values at HDV `4BC8..4BF0`. `FF0150` first resets that
feedback for player vehicles, prepares collision shapes through `F4DB30`, builds
collision pairs, marks affected vehicles and runs collision resolution.

`F4DB30` retains shape geometry from the old collision proxy before calling
`F4E7B0` to prepare the new proxy and geometry. Consequently, changing physical
bodies alone does not establish coherent previous/current collision geometry.
This is a concrete missing dependency, not yet proof that it caused the incident.

The native teleport path `F73900` contains the following relevant sequence:

1. Call `F702E0`, then `F4E7B0` with the vehicle pointer.
2. Call `FEDFA0` with the vehicle in RDX. This walks the global collision-pair
   vector at RVA `3B8AC40/3B8AC48`, stride `398`, and removes pairs whose endpoint
   is a type-6 entry referring to that vehicle. Both endpoints are checked.
3. Clear vehicle byte `16CF9`; for player control, call `ED00D0` to move the
   physical state, then set vehicle byte `18F4D`.

This is evidence that native teleportation invalidates pending collision work.
It is not a drop-in reset API: it uses exported pose, has other state changes,
and the selective pair removal changes a shared vector. Before integrating any
of it, establish the correct projection of restored state to exported pose,
the enclosing ownership of every relevant caller, and handling of collision
history/feedback. Simply clearing `16CFA` would hide a pending correction without
repairing these dependencies. No such clearing or native teleport call was added.

V12 adds two observational hooks at `F4EAC0` (phases 11/12) and `F4E7B0`
(13/14). They forward each original call exactly once. Only the player's existing
physics thread samples, with the worker lock held and roots matching. During the
20-step post-reset window, records now include the separate collision body,
exported pose/velocities, aggregate HDV pose, pending correction byte and six
feedback values. The buffer remains bounded at 512 records. These hooks add no
state repair, collision suppression or game-state writes.

The trace ABI is 1368 bytes for v12; v10/v11 remain 952 bytes. Readability bits
9 through 13 respectively identify collision body, exported state, correction
byte, HDV pose and feedback. A set bit means the read succeeded, not that the
values are finite or physically valid. The Python decoder selects by host
revision. The analyzer keeps incomplete calls and non-finite-body evidence,
and only compares readable, finite collision groups. Proxy/exported disagreement
can reflect normal advancement and is not by itself a failure classification.

Validation: the native v12 fixture passed the 10,077-field restore, absent-player
refusal and two-instance restart. An injected 11 m displacement inside the
fixture's correction callback was correctly bracketed by phases 11/12; original
call counts were unchanged. Twenty Python tests passed, including trace layouts,
world/local velocity discrimination, missing data and crash-truncated callbacks.
All 13 target signatures match the previously captured code; the generated DLL
hash matches its local profile. These tests do not simulate native LMU collisions.

V12 has not yet run in LMU. The deferred live procedure is in
[TEST_RESET_COLLISIONI.md](TEST_RESET_COLLISIONI.md). The live defect remains open.

### Native teleport analysis completed

The second REA batch completed successfully in the local directory
`analysis/rea-25661166/native-teleport-20261015/`, covering `FEDFA0`, `F73900`,
`3D0DE0` and `CD2930`. It confirms the pair-removal loop and teleport ordering.
The pair vector's resize helper `3D0DE0` performs element cleanup before moving
the end pointer; this is not equivalent to editing the vector length or copying
raw pair bytes. `FEDFA0` itself does not acquire a critical section, so its native
call sites must supply the required exclusion. Direct-call references were found
in the teleport path and at `F4442E`; this does not exclude indirect calls.

`F73900` also converts its requested position using vehicle geometry and can adjust
height/suspension against the track. Passing the saved root-body position directly
would therefore not preserve the checkpoint reference point. `F702E0` refreshes
a spatial lookup using exported position; its cached pointer at vehicle `17B28`
is explicitly invalidated by the teleport path before the call. The full teleport
routine must not be substituted for snapshot restoration without modeling those
differences.

`CD2930` calls collision generation `FF0150` on native counter modulo eight equal
to zero. Along with the earlier `CC8210`/`FEFC60` result, this shows that collision
work has its own cadence, separate from each individual Dynamics callback. Its
enclosing lock context remains to be proven before changing shared collision data.

The next trace distinguishes two mechanisms that need different ordering fixes:

* A pending correction survives across restore and changes bodies before the next
  collision-proxy preparation. Compare the saved, pre-restore and applied collision
  states, then phases 11/12 against the next 13/14.
* A new collision sweep spans the old and restored locations because previous
  collision geometry was not rebased. Look for a preparation across that gap and
  a later correction, rather than assuming the original pending flag caused it.

The current data cannot select between these mechanisms. Neither is repaired by
overwriting only linear velocity: the captured velocity was already restored
correctly before the common pose transformation. The pending implementation work
is coherent aggregate/exported pose synchronization, collision history rebasing
and native invalidation of obsolete pairs at a proven boundary. No unverified
candidate for those writes has been enabled in v12.

## Continuous-session trace fix (v14, 16 October)

After integration of the desktop/v13 branch, the native fixed trace buffer would
still stop permanently at 512 records. That meant later resets in a continuous
session could be applied without physical diagnostic samples. V14 replaces this
with a fixed-size ring while retaining the 1368-byte v12 record payload.

The original count/payload region is followed by 512 aligned 64-bit publication
stamps. The sole physics-thread writer marks a slot busy with the negative next
sequence, copies its payload, publishes its positive sequence, then advances the
global count. Readers check the same stamp before and after copying and emit only
matching completed records. They never write into the mapping or stall physics.
Old layouts remain selected for hosts through v13.

Losses are explicit sequence ranges, including an old slot destroyed by a reuse
interrupted by a game crash before the global count advances. That interrupted
slot does not hide the other completed records. Analysis discards pending
before/after comparisons across gaps and keeps the gaps in the report.

The native ring fixture and host fixture passed on Windows, and forty Python tests
passed. The installed local default is v14. This fixes diagnostic exhaustion;
it does not modify collision state or claim to fix the physical instability.

## Captured collision sweep and v15 repair (16 October)

Recording `20261016T142736982514Z_physical_reset_host_v14` contains 790 physical
records, no gaps, two saves and eight restores. Every restore verified 10,077
fields; the five traced bodies initially matched the saved pose, velocity and
angular momentum. The first seven restores show no large jump within their
20-step diagnostic windows. This is not a claim of longer-term stability.

The user reported the final instability while steering after leaving the track.
Request 18 restored a checkpoint saved at 206.86 km/h from 203.68 km/h, about
708 metres away. This also shows that greater current speed is not necessary
for the defect. At sequences 690/691, the physical root returns to
`[-236.956884, 2.078725, 336.180637]`, while the separate collision body stays at
`[-61.626900, 5.903471, 1021.373657]`. Its pending flag is zero on both sides.

At sequences 701/702, `F4E7B0` finally moves the collision body to the restored
exported pose. `F4DB30` has already calculated previous shape geometry from the
old body, so its sweep spans the two locations. By sequence 703 a new correction
is pending. Between sequences 704/705, inside `F4EAC0`, the root jumps
115.539209 metres and its velocity vector changes by 75.436843 m/s. Collision
feedback reaches approximately 56.94 million on its first axis. This incident
is a newly generated collision, not an old pending flag surviving the reset.
The trace also has an earlier first-Dynamics transient; eliminating the measured
collision jump alone cannot establish that all resumed dynamics are correct.

V15 adds seven typed fields to the existing transaction: exported pose/velocities,
exported collision mass, HDV aggregate position, rotation, world velocity, world
angular velocity and collision feedback. The first six come from the checkpoint;
feedback from obsolete collision pairs is reset to zero. The exported binding is
an explicit additional slot, with an exact field whitelist. It contains no saved
pointers. Finite values, proper rotations and positive collision mass are required
before replacing a checkpoint. Numeric caches participate in the same preflight,
byte readback and compensation as the existing physical/scoring fields.

After four tire-contact rebuilds succeed, the existing admitted physics boundary
still owns W and C8 and pins the lifecycle generation. At that point the host uses
the native teleport path's cache-maintenance sequence: invalidate spatial lookup
`17B28`, call `F702E0`, prepare the collision body with `F4E7B0`, remove this vehicle's
obsolete pairs with `FEDFA0`, then reset the overlap-cache flag `16CF9`. Pair-vector
cleanup remains native; there is no manual vector truncation. The full teleport
routine is not called because it adjusts reference position/suspension and
transforms bodies whose checkpoint state has already been restored. Native code
prologs are checked against the verified local capture before activation.

The same rebuild callback runs after compensation, using the original numeric
projection restored by the transaction. Failure to recover keeps the existing
quarantine behavior. Additional locks are released before normal physics resumes.
Trace phases 15/16 bracket this cache reconstruction, before phase 3. Exact-byte
verification still occurs inside the transaction; the later host report allows
only a 1e-6 component difference in the nine exported rotation floats normalized
by the native routine. Other fields retain exact comparison.

Local validation covers a 1,000-metre reset with changed heading and 80/-100 m/s
saved/current velocity, removal of a pending correction, unchanged physical pose,
saved collision mass and velocity, no long collision sweep, all 10,084 fields,
four contacts, absent-callback rejection and two consecutive host instances.
Adapter cases cover success, contact compensation, failed recovery quarantine and
collision-rebuild failure followed by compensation, including rejected replacement
of a checkpoint by a singular rotation. The core Windows suites and Python tests
also pass. These fixtures model native collision helpers and do not replace a
live test. At this stage no v15 live reset had yet been recorded.

### First v15 attempt: launcher blocked before restore

Recording `20261016T150448520010Z_physical_reset_host_v15` contains three successful
saves and fifteen successful preparations, but no restore command and zero physical
field writes. The user correctly observed no teleport. Both launcher preparation
gates still required the former 10,077-field plan; the v15 host returned 10,084.
The native host then detached cleanly on a vehicle lifecycle change, with no
process crash recorded. This attempt did not exercise the collision correction.

The launcher now selects the expected field count from the admitted revision and
uses one predicate for both hotkey advancement and the recent-preparation lease.
Tests exercise v14 and v15 save/prepare/restore sequences, reject the other plan
size and reject unknown revisions. A mismatched preparation now reports an explicit
reason instead of silently abandoning F7. No fallback or reduced field validation
was introduced. The next recording exercised the corrected launcher.

### Seven successful v15 restores and intentional shutdown

Recording `20261016T151235731244Z_physical_reset_host_v15` contains 696 physical
records without gaps or nonfinite body values. One checkpoint saved at 178.67 km/h
was restored seven times, from current speeds of 191.37–242.61 km/h and distances
of 67.11–797.36 metres. Every restore verified all 10,084 fields with zero
mismatches, four rebuilt contacts and no compensation. All five traced bodies
matched their saved position, orientation, velocity and angular momentum exactly
at the immediate post-restore boundary.

All seven collision rebuilds left the restored physical root unchanged. The
17 subsequent recorded collision-correction calls also left its position,
velocity, orientation and angular momentum unchanged. Across 140 Dynamics steps,
the largest root position increment was 0.124446 metres and the largest velocity
vector increment was 0.063205 m/s. No root change occurred between consecutive
Dynamics calls. These detailed windows cover 20 steps (about 50 ms) per restore;
they establish absence of the previous immediate collision jump in these trials,
not universal stability under every condition.

The user confirmed successful teleports, including while cornering. The listener
was then stopped deliberately by the operator after collecting enough evidence.
Native shutdown completed with status 3, error 0, no failure, LMU still running,
and original bytes verified at all 15 target addresses. The supervisor incorrectly
reported this intentional Ctrl+C as an unexpected disconnection because the host
returned the generic `stopped` outcome. Intentional console, desktop and command
stops now use `user_stopped`, which exits supervision without an error or automatic
reconnection. Unexpected shutdowns remain errors, and failed native cleanup still
raises. A regression test checks that intentional shutdown admits only one host.
