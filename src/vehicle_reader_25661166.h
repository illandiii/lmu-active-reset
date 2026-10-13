#ifndef LMU_VEHICLE_READER_25661166_H
#define LMU_VEHICLE_READER_25661166_H
#include "vehicle_snapshot_25661166.h"

// The caller owns a verified build and a lifetime pin for the entire read.
// Output buffers must be distinct. Failure clears the binding count; the frame
// is scratch storage and must not be published unless this returns true.
bool vehicle_read_25661166(uintptr_t module_base, uintptr_t physics, uint64_t epoch,
                           double simulation_time, SnapshotFrame *, RestoreBindings *);
uintptr_t vehicle_scoring_25661166(uintptr_t module_base, uintptr_t physics);
#endif
