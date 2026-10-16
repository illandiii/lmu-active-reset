# Full checkpoint adapter

The full checkpoint pipeline is now connected to the native boundary dispatcher
through `vehicle_restore_adapter_25661166`. It is not installed in LMU yet.

`vehicle_reader_25661166` reads the typed vehicle layout previously validated by
the v31 observer. `vehicle_checkpoint_25661166` saves and prepares all currently
mapped subsystems, then appends the three scoring fields. The payload is separate
from observer IPC. The adapter resolves fresh bindings, validates all four contact
samples and supplies the real contact-batch implementation to `RestoreFlowOps`.

The host must supply a verified build, the physics thread, the already-owned
native worker critical section, the separate control critical section and a lifetime pin that covers the entire callback,
including capture and preparation. A pointer comparison or heartbeat is not a
lifetime pin. This remaining host contract prevents enabling a game writer now.
The [object lifetime investigation](OBJECT_LIFETIME.md) describes the implemented
nonblocking dual-lock scope and the coverage still open.

The adapter owns the saved payload and preallocated preparation workspace. It
publishes a new save only after every subsystem validates; a failed replacement
preserves the previous checkpoint. It rejects changed identities/epochs, missing
locks, expired contact samples, changed topology and unsupported terminal damage.
There are no allocations in capture or preparation. Contact samples must belong
to the same thread, epoch, world and four current tires, and be at most 100 ms old.

## Validation

The fixture uses recorded numerical states, fresh local allocations for all
2,700 binding slots and a modeled native contact refresh. It runs the real
boundary, plan, Win32 memory backend and contact-node adapter. Three separate
processes passed:

Admission now uses the asynchronous gate with an actual competing control
thread. That thread yields its control lock through the gate and cannot resume
until the complete capture/restore/compensation sequence releases the acquired
pin. This connects the new synchronization mechanism to the full adapter fixture.

- Success: 10,077 fields committed and all four tires rebuilt.
- Contact failure: all 10,077 fields compensated and four original contacts rebuilt.
- Repeated contact failure: current and later original callbacks suppressed;
  checkpoint disabled and fence detach refused.

Additional checks cover failed-save preservation, stale contact time/epoch,
changed topology/session, absent native lock/pin, next-lap restrictions and an
unrelated scoring byte. An epoch change at identical addresses invalidates the
checkpoint permanently, including if the previous epoch is subsequently supplied.
Successful and compensated paths compare every mapped
field against the expected final bytes.

```powershell
python scripts/sector.py replay.prepare_vehicle_adapter_fixture <recording.frames.jsonl> work/vehicle-adapter-fixture
./scripts/Build-VehicleRestoreAdapter.ps1 -ZigPath <zig.exe> -FrameDirectory work/vehicle-adapter-fixture
```

Omit `FrameDirectory` to compile both production and fixture targets without local
recordings. The resulting production DLL has no injection or hook-install entry.
The fixture reader substitution is compiled only with `LMU_VEHICLE_ADAPTER_FIXTURE`.

These results do not validate a live moving reset, live contact argument selection
after a rewind, complete vehicle-state coverage or lap/record behavior in LMU.
