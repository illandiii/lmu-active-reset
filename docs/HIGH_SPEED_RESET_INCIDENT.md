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
