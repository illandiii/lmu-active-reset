#ifndef LMU_CHECKPOINT_VEHICLE_PARTIAL_H
#define LMU_CHECKPOINT_VEHICLE_PARTIAL_H
#include "checkpoint_persistent_bank.h"
/* Verified native field locations, deliberately offset-named where semantics
 * are incomplete. No claim of complete engine or thermal coverage. */
typedef struct PowertrainPartialState {
    uint64_t engine_type_rva;
    double engine_1f8, engine_250, engine_260, engine_280;
    double hdv_48a0, hdv_48a8, hdv_48b0, hdv_48b8, hdv_48e8, hdv_48f0, hdv_48f8;
} PowertrainPartialState;
typedef struct VehiclePartialBank {
    PersistentCheckpointBank mechanical;
    PowertrainPartialState powertrain;
} VehiclePartialBank;
typedef struct VehiclePartialPreparation {
    CheckpointPreparation mechanical;
    MechanicalDependencies dependencies;
    TirePersistentState tires[4];
    PowertrainPartialState powertrain;
} VehiclePartialPreparation;
int checkpoint_save_vehicle_partial(VehiclePartialBank *, const PartialCheckpoint *,
                                    const MechanicalLinks *, const MechanicalDependencies *,
                                    const TirePersistentState[4], const PowertrainPartialState *);
int checkpoint_prepare_vehicle_partial(const VehiclePartialBank *, const CheckpointIdentity *,
                                       const RigidMassProperties *, size_t,
                                       const ConstraintRecipe *, size_t,
                                       const MechanicalDependencies *, const TirePersistentState[4],
                                       const PowertrainPartialState *, VehiclePartialPreparation *);
#endif
