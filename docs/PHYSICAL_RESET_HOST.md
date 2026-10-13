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
external heartbeat. Versions 1–5 required speed at most 0.5 m/s and limited
displacement to 0.25 m. Version 6 removes those two limits. Terminal failures remain rejected. A read-only
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
prologs and attempted zero physical field writes.

### First successful live reset

After starting a fresh Practice with terminal failure zero,
`20261013T204517144116Z_physical_reset_host_v4` completed the first actual reset.
The game remained PID 31512 and the physics thread was 9756. The first checkpoint
was refreshed because the unbraked vehicle had rolled beyond the initial 0.25 m
displacement bound. The subsequent save/prepare/restore commands were acknowledged
sequentially; no limit or reader guard was bypassed.

| Measurement | Result |
| --- | --- |
| Restore command | ID 5, control result 0, flow result 0 |
| Speed before restore | 0.0788066719 m/s |
| Fields written / verified after contacts | 10,077 / 10,077 |
| Mismatched fields / recovery writes | 0 / 0 |
| Native contact calls / completed mask | 4 / 15 |
| Completed saves / restores | 2 / 1 |
| Lifecycle generation | 1 |

Saved and post-restore body position matched exactly:
`[-186.16254928547082, -12.037670200296452, 359.92693603335323]`.
The pre-restore position was
`[-186.13655513664276, -12.037827192677504, 359.9308149324934]`.
These are physical body coordinates, not the telemetry reference point.

A later read/prepare (command 6) passed all full-reader checks and prepared the
same 10,077-field plan after 5,934 additional outer callbacks, with no further
physical writes. Telemetry continued advancing with finite positions, velocities
and angular velocities. The user changed controls during the subsequent capture;
this does not establish deterministic replay or long-term unattended stability.
The reported lap-invalidated flag stayed false, but no completed lap was tested.

Shutdown returned status 3/error 0, with all 11 original target prologs restored.
An independent audit confirmed all six resident project modules stopped and a
cumulative total of exactly 10,077 physical field write attempts. No hotkey loop
or automatic restore remained active after that test. Moving resets, terminal-damage restoration,
general lifecycle coverage and lap-validity behavior remain unqualified.

Raw captures, game dumps and decompiled game code remain local and are not committed.

## Hotkey connection

`Start-Reset.cmd` now starts a four-minute session using F6 to save and F7 to
prepare/restore. The controller requires a clean key release in the foreground
game window, ignores chords/modifiers and does not repeat held keys. Restore
requires an acknowledged successful save and a matching 10,077-field preparation;
focus/context loss or an acknowledgement older than two seconds cancels the
pending preparation. Lifecycle and lease checks remain mandatory. Ctrl+C stops
and drains the native host before terminating its own
read-only telemetry child. The game process is never terminated.

Six focused-controller tests pass for acknowledgement sequencing, held keys,
missing checkpoints, chords, invalid preparation, focus/context loss and timeout.
The v5 host passed the loaded 10,077-field fixture. Read-only live checks resolved
worker 9756 and queried the foreground/key state successfully. Actual physical
keyboard presses through this new frontend have not yet been exercised in LMU;
the earlier successful live reset used explicit commands.

## V6: user hotkey failure and unrestricted-distance model

The user's v5 run (`20261013T205124393195Z_physical_reset_host_v5`) confirmed real
hotkey saves, but exposed intermittent reader cause 90205 (mechanical body inertia
mismatch), an HTTP timeout and a missing final native acknowledgement. Its shutdown
summary showed 20,154 write attempts although the last displayed acknowledgement
preceded those writes. That count alone cannot establish whether the unseen command
recovered successfully; the historical missing result cannot be reconstructed from
that summary. All hooks did stop cleanly.

V6 changes:

- A mechanical body mismatch can defer the same request to a later outer boundary
  for up to 1.2 seconds, inside the existing 1.5-second request lifetime. Each retry
  runs the full unchanged reader. Persistent mismatches remain rejected without
  writes; no tolerance is relaxed and inconsistent state is never accepted.
- HTTP timeout/connection failures revoke the lease and write permission, cancel
  pending hotkey preparation and retry transport instead of exiting immediately.
  Successfully fetching data alone does not grant admission; normal context checks
  must pass again. A new key press is needed after cancellation.
- Shutdown collects any stable acknowledgement completed after the last regular
  poll, including forward/recovery counters, before unmapping shared memory.
- Speed and checkpoint-distance restrictions are removed. The launcher selects
  v6 only, so it cannot silently fall back to a limited old binary.

The loaded-host fixture now saves at 80 m/s, moves its modeled bodies 1,000 m,
changes current speed to 100 m/s and verifies the complete 10,077-field restoration
and four contact calls. It additionally exercises three transient mismatches and
a persistent mismatch, with the latter rejected after bounded retries and no new
writes. Three transport/report regression tests (timeouts at each HTTP stage,
connection failures, late recovery report and duplicate/incomplete reports) and
six hotkey tests pass. These are isolated tests; live moving/long-distance reset
validation still requires reopening LMU. In particular they do not prove that
native track/contact caches work for every distant checkpoint or airborne state.

## V7: correct the inertia model, remove mechanical retries

The root cause of the reproducible 90205 rejection is the integrated reader's
legacy call to `mechanical_prepare` using current tire-sector sums. Native code
F1222A/F12242/F12253 subtracts persistent C20/C28/C30 caches, and sectors can advance
separately. The checkpoint bank already handled these different phases; the reader
incorrectly rejected the frame before the bank could validate it.

The reader now calls `checkpoint_validate_cached_mechanical`, sharing the existing
cached-mass normalization and persistent-inertia validator with checkpoint storage.
Original captured sectors and caches remain intact. The existing 32-epsilon
tolerances, wheel inertia and other mechanical checks are unchanged. V6's 1.2-second
mechanical retry block has been removed: a genuine invalid frame is rejected on its
first attempt. Transport recovery remains separate from physical-state validity.

Replay of the 12 unmodified real frames in
`20261013T201355027985Z_snapshot_observer_v31.frames.jsonl` reproduces seven legacy
mechanical-status-5 rejections and five successes. The corrected validator accepts
all 12 and rejects all 192 deliberately corrupted mass/inertia-cache cases. Run:

```powershell
python src/test_cached_reader_recording.py logs/20261013T201355027985Z_snapshot_observer_v31.frames.jsonl
```

The loaded v7 host fixture also reproduces the phase mismatch, verifies immediate
acceptance with the correct cache model, immediate rejection of invalid caches
without retries, and the full moving/1,000 m restore. The launcher selects v7 only.
This fixes the demonstrated model error offline; a fresh live hotkey run is still
needed to verify the updated reader in the current game session.

## V8: discover the worker from native callbacks

The launcher no longer samples a critical section's owner to infer a physics
thread. That sampling could see too few acquisitions or other legitimate owners
and incorrectly report `Physics worker not stable` before loading the host.

V8 starts with commands disabled and callbacks forwarding normally. It identifies
the player Dynamics callback only when nested one level inside the outer boundary
and running on a thread that actually owns the worker lock. It then initializes
the adapter and fence with that observed thread before enabling commands. The
launcher supplies owner thread zero for automatic discovery; explicit nonzero
thread IDs must match the native observation. The discovery phase writes no
vehicle state. If no qualifying callback arrives within two seconds, it disables
the hooks and reports failure instead of guessing an owner.

The loaded-host fixture passes with no externally provided thread ID, followed by
the complete 10,077-field restore. A second fixture suppresses the player callback
while outer callbacks continue and confirms refusal, zero writes and original
outer instructions restored. The launcher now selects only v8. Actual user launch
against LMU remains the final live confirmation of this startup change.
