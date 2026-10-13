# Experimental physical reset host

The host connects the full vehicle/scoring adapter to the outer physics callback
on Steam build 25661166 (app build 14200). It is a bounded offline experiment,
not a qualified moving-reset or damage-recovery feature.

## Admission and lifetime

Commands require the observed physics thread, an existing worker-lock ownership,
nonblocking control-lock acquisition through the asynchronous handoff, matching
object roots and a pinned lifecycle generation. Seven native hooks cover the
registered clear, reconstruction, destruction, construction and control entries.
Any such event invalidates this host permanently, including reconstruction at
the same address. This conservative prototype also invalidates on registered
events involving other vehicles; it does not silently re-arm in a new session.

The REA analysis of captured runtime code confirmed the native signatures used
by these hooks. The generation guard is additional exclusion for registered
entries, not proof that all possible game lifetime paths have been discovered.

## Commands and bounds

Build with `scripts/Build-PhysicalResetHost25661166.ps1`, specifying `-ZigPath`,
`-FrameDirectory` for the recorded-frame fixture and an unused `-Revision`.
Never overwrite a DLL resident in LMU, even after its hooks have stopped.

The Python runner accepts a PID, observed owner thread, revision, bounded duration
and JSON request file. Offline launch confirmation is required; restoring also
requires `--allow-writes`. Publish command files atomically with increasing IDs:

| Action | Meaning |
| --- | --- |
| 1 | Capture a checkpoint; no physical writes |
| 2 | Prepare the complete typed plan; no physical writes |
| 3 | Restore after a successful preparation within 60 seconds |

`{"stop": true}` requests clean shutdown. The native lease expires without an
external heartbeat. Save/restore require speed at most 0.5 m/s; this first restore
also limits displacement to 0.25 m. Terminal failures remain rejected. A read-only
runner precheck rejects existing failures before loading another host; the pinned
reader repeats the authoritative check inside the callback.

The transaction restores 10,077 fields including scoring, rebuilds all four tire
contacts, and reads back every planned field before the native update resumes.
Contact reconstruction uses checkpoint angular inputs on forward restore and
current angular inputs during compensation. It requires matching step parameters.
Additional acquired locks are released before forwarding the original update.
Unrecoverable transaction failures retain the simulation fence until process exit.

## Verification on 2026-10-13

All 11 `Test-Core.ps1` suites passed, including the new lifecycle suite. Actual
Win32 threads exercise exclusion, nonblocking admission, same-address invalidation,
nested mutation and generation saturation. The seven-hook fixture verifies native
arguments/returns and detach behavior.

The loaded-host fixture captures, prepares and restores 10,077 fields, verifies all
of them after four modeled contact calls, checks write opt-in, lock release before
the original update, lifecycle invalidation and clean stop. Its reader and native
contact routine are fixture substitutes; these results do not establish a live reset.

Live boundary evidence is documented in `OBJECT_LIFETIME.md`. The physical host
acquired the native scopes but rejected capture:

| Local log prefix | Result |
| --- | --- |
| `20261013T203334678682Z_physical_reset_host_v1` | Reader rejected both capture attempts |
| `20261013T204057478140Z_physical_reset_host_v2` | Diagnostic cause 90001: terminal failure |
| `20261013T204237595691Z_physical_reset_host_v3` | Terminal failure persisted after pit return |

An independent read confirmed `game_vehicle+1CF9C == 1` and the vehicle root matched
the player. All three runs stopped with status 3/error 0, restored all 11 target
prologs and attempted zero physical field writes. The first live reset still
requires a healthy vehicle in a fresh Practice.

Raw captures, game dumps and decompiled game code remain local and are not committed.
