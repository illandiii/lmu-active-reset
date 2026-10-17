# Bodywork checkpoint and synchronization

Sector 0.2.9 introduces native host v21. It integrates CPU bodywork buffers with
the existing physical reset transaction for the verified Steam 25661166 profile.
Automated tests pass; visible repair in LMU still requires the packaged live test.

## What is captured and restored

A healthy save captures each graphics component's CPU vertex data, mesh topology
and allocation generation. Restore validates the same model and original vertex
buffers, journals the current bytes, restores the saved bytes, clears damage
events and severity, and marks the component for the game's normal render upload.
Only the damage bit in submesh flags is cleared. Other flags are preserved.

The graphics root is the player owner plus `0x38`, separate from its collision
subobject at plus `0x8`. Native repair copies the component's original vertices
and requests an upload. Sector does not call that repair function on an assumed
thread or treat a cleared damage flag alone as a complete repair.

The damaged Oreca capture contains four CPU meshes. Its 6,408 differing bytes
are at vertex attribute offset 27; this capture does not demonstrate changed
vertex positions. The local replay reproduces and restores those bytes. It does
not establish that LMU subsequently uploads them or renders an intact car.

## Thread and lifetime contract

Physics, scoring and player lifetime are pinned by the existing host. The new
graphics gate additionally observes player model loading, clearing, construction,
destruction, asynchronous part publication, impact updates and native repair.
Ten target prologs are verified before hooks are installed.

Native operations register activity but release the gate while executing. A
loader can therefore wait for another thread's publication job without keeping
the gate locked. Reset takes a nonblocking pin only while no operation is active,
then takes every component critical section with `TryEnterCriticalSection`.
The pin and component locks remain held through physical writes, graphics writes,
contact/collision rebuilding and finalization. New native operations wait for
that brief pin. Reconstruction changes the generation even if addresses repeat.

All graphics spans are preflighted before the first graphics write, checked after
writing, and journaled for compensation. A physical contact or collision failure
also restores the pre-reset graphics bytes. A failed recovery quarantines the
host, suppresses further simulation callbacks and requires an LMU restart.
No automatic retry or partial graphics fallback is used.

## Limits

- Only healthy, fully loaded checkpoints are accepted.
- CPU buffers are supported; the native GPU-mapped buffer flag is rejected.
- Bounds are 32 components, 256 meshes, 4,096 submeshes per mesh and 32 MiB of
  vertex data. Malformed layouts, overlapping spans and changed topology fail
  before writes.
- Punctures and detached assemblies remain unsupported.
- Restoration during a continuing impact and other vehicle models need separate
  live qualification. There is no speed or distance cap in this implementation.

## Source map

| File | Responsibility |
| --- | --- |
| `src/native/restore/bodywork_checkpoint.*` | Graphics capture, validation, journal and rollback |
| `src/native/runtime/graphics_lifetime.*` | Graphics activity tracking and generation pins |
| `src/native/runtime/graphics_lifetime_hooks.cpp` | Typed hooks around native graphics operations |
| `src/native/restore/vehicle_restore_adapter.*` | Combined physical and graphics transaction |
| `src/native/runtime/physical_reset_host.*` | Physics boundary, command dispatch and reports |
| `src/native/runtime/reset_hook_targets.*` | Verified native hook target signatures |
| `src/python/sector/runtime/reset_host_runner.py` | Host admission, commands and acknowledgements |

Descriptive filenames replace numeric suffixes in the active runtime. Actual
Steam build IDs, compatibility hashes and host revisions remain explicit in
profiles and DLL names. Historical observer versions retain their identifiers
because they describe different recording layouts.

## Local verification

`scripts/build/Build-BodyworkRestore.ps1` exercises captured-buffer replay,
partial and silently corrupted writes at every journal span, rollback failure,
pin loss, topology changes, lock contention and asynchronous publication.
`Build-VehicleRestoreAdapter.ps1` checks combined success and contact/collision
compensation. `Build-PhysicalResetHost.ps1` loads the actual host into an owned
fixture process, exercises save/restore, rejects a stale graphics generation
without physical writes, permits a fresh save and tests clean host restart.
`Build-ActiveResetBoundary.ps1` verifies suppression of simulation after a
finalization failure. None of these fixtures opens LMU.

## Packaged live test

1. Quit the previous Sector process completely and restart LMU without EAC.
2. Open Sector 0.2.9. Start a fresh Offline Practice and enter DRIVE with a healthy car.
3. Press F6 once and wait for `Checkpoint saved`. Move a short distance and press
   F7 once. Confirm teleport and stable driving before testing damage.
4. Keep the healthy checkpoint. Produce one front or rear impact without losing
   a wheel. After the impact ends, press F7 once.
5. Check the front and rear visually, suspension, teleport and continued driving.
   Record the result and remain on the same vehicle for inspection.

Stop after a refusal or instability and preserve the exact error message.
The host acknowledgement now logs component/mesh counts, vertex bytes, graphics
result, attempted writes, verified writes and recovery writes alongside the
existing physical report. Causes `92003` and `92007` distinguish an update in
progress from a reconstructed model that requires a new checkpoint.
