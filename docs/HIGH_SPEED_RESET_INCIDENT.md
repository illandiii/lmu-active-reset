# High-speed reset and launcher lifecycle

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
