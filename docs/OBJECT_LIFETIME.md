# Object lifetime and control changes

This investigation separates pointer identity from object lifetime. The game
writer remains disabled until the complete host-side lifetime contract is met.

## Verified paths and additional findings

The ordinary session-exit path returns from `CD0BD0` only after acquiring worker
critical section `3B8A968` and clearing both worker callbacks. The subsequent
`CCFC60` teardown therefore follows the drain of that ordinary worker path.

Control change `F73DE0` can allocate, replace or destroy the HDV physics object.
The direct-reference scan found 36 call/jump sites. The separate HDV destructor
`EE3320` has six direct callers, so observing only `F73DE0` would miss other paths.
`EE32D0` calls the destructor, clears the object and reconstructs it at the same
address. A stable pointer is consequently insufficient as a lifetime token.
No direct callers of `EE32D0` were found; this does not establish that it is
unreachable, and it has not been observed executing in this game session.

Two control-change paths use a second critical section, `1A5A5C8`:

| Function | Acquire | Control-change call | Release |
| --- | --- | --- | --- |
| `B24240` | `B24284` | `B247AA` | `B24ED1` |
| `CC0490` | `CC0771` | `CC07B7` | `CC0857` |

These are instruction-level findings from the hash-verified code capture. They
are evidence for the listed paths, not a claim that all mutations use this lock.

REA completed analysis of `CDA340`, `CDB4F0`, `CDB5A0`, `CD81F0` and `EE3320`.
The first three lead to control selection in `CD81F0`. An indirect callback
slot at `17C9D68` points to `CDB4F0`; that pointer was also checked against the
running build. Its table starts at `17C9D58`, referenced by `CDB630`. This is why
the absence of a direct call cannot be used to dismiss a lifecycle path.

## Implemented protection

`native_lifetime_pin` requires the caller to already own the worker lock and
uses `TryEnterCriticalSection` for the distinct control lock. Contention refuses
the operation without waiting. This avoids introducing an inverse wait against
a main thread holding the control lock while waiting for the worker to finish.
The scope releases only its own recursion and checks ownership of both locks.

The full vehicle adapter now requires both locks during capture, preparation,
memory access and contacts. It additionally requires the host's pinned-context
validator; acquiring these two locks alone is not treated as proof of universal
coverage. The host must hold the scope around the entire boundary dispatch.

A fixture with two actual threads verifies contention, recursion balance and
ownership loss. The full adapter's three fault scenarios pass with the scope in
place. A separate case changes the lifetime epoch while keeping pointers and
external identity unchanged: the saved slot is invalidated and cannot be revived
by restoring the old epoch value.

## Still required before enabling a game writer

Close synchronization coverage for the remaining destructor/control callers,
including indirect dispatch, and supply an authoritative epoch for in-place
reconstruction. A usable admission window is also still required.

The boundary v3 observer tested nonblocking admission in the actual game during
the natural Dynamics callback inside the outer boundary. Across 4,764 callbacks,
14 persisted samples were stable; every attempt found the control lock owned by
thread 23068 while physics ran on thread 20020. All attempts were refused without
waiting. Thus this probe validates refusal under contention, **not successful
live lifetime pinning**. The main loop releases `1A5A5C8` at `CD0FE6`, exchanges
with `1A5A5F0`, then reacquires it at `CD100D`; the useful handoff window and its
relationship to mutations still need to be established.

The observer stopped, restored Dynamics/ScoreUpdate/OuterUpdate and left seven
resident but inactive modules. The shutdown audit also verifies Contacts. No
physical writes or full reset occurred. Local evidence: the
`20261013T194408625195Z_boundary_observer_v3` capture and
`logs/20261013_dual_lock_shutdown_audit.json`.

Local raw evidence lives under `analysis/rea-25661166/control-lifetime-20261013/`.
It is excluded from Git. Reusable direct-reference and absolute-pointer scanners
are included in `src/`; absolute data candidates can be checked using read-only
`--pid` validation against the matching executable hash.
