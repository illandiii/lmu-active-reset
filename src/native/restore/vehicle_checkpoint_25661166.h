#ifndef LMU_VEHICLE_CHECKPOINT_25661166_H
#define LMU_VEHICLE_CHECKPOINT_25661166_H
#include "../observers/vehicle_snapshot_25661166.h"
extern "C" {
#include "restore_joint_25661166.h"
}

// Allocate once outside the physics callback. No allocation or game writes are
// performed while validating a capture or preparing a restore plan.
struct VehicleCheckpointWorkspace;
VehicleCheckpointWorkspace *vehicle_checkpoint_workspace_create();
void vehicle_checkpoint_workspace_destroy(VehicleCheckpointWorkspace *);
int vehicle_checkpoint_validate(VehicleCheckpointWorkspace *, const SnapshotFrame *);
int vehicle_checkpoint_prepare(VehicleCheckpointWorkspace *, const SnapshotFrame *saved,
                               const SnapshotFrame *current, const RestoreBindings *,
                               const RestoreBackend *read_backend, const LapGuardFlags *,
                               RestorePlan *output,
                               const WheelDeformation *current_deformation = nullptr);
#endif
