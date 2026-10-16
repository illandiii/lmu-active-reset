#ifndef LMU_CHECKPOINT_PERSISTENT_BANK_H
#define LMU_CHECKPOINT_PERSISTENT_BANK_H
#include "checkpoint_bank.h"
#include "../physics/tire_persistent_core.h"
typedef struct PersistentCheckpointBank {
    CheckpointBank base;
    MechanicalDependencies original_dependencies;
    TirePersistentState tires[4];
} PersistentCheckpointBank;
int checkpoint_save_persistent(PersistentCheckpointBank *, const PartialCheckpoint *,
                               const MechanicalLinks *, const MechanicalDependencies *,
                               const TirePersistentState[4]);
int checkpoint_prepare_persistent(const PersistentCheckpointBank *, const CheckpointIdentity *,
                                  const RigidMassProperties *, size_t, const ConstraintRecipe *,
                                  size_t, const MechanicalDependencies *,
                                  const TirePersistentState[4], CheckpointPreparation *);
#endif
