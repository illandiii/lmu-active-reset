#ifndef LMU_CHECKPOINT_BANK_H
#define LMU_CHECKPOINT_BANK_H
#include <stdint.h>
#include "../physics/constraint_warm_group.h"
#include "../physics/mechanical_prepare_core.h"
/* Tokens must be supplied by a live adapter from observed object lifetimes and
 * configuration. This library does not discover or authenticate those tokens. */
typedef struct CheckpointIdentity {
    uint64_t process_epoch, session_epoch, vehicle_epoch, configuration_epoch;
    unsigned char executable_sha256[32];
} CheckpointIdentity;
typedef struct PartialCheckpoint {
    CheckpointIdentity identity;
    uint64_t sequence;
    double simulation_time;
    size_t body_count, constraint_count;
    RigidSavedBody bodies[RIGID_RESTORE_MAX_BODIES];
    ConstraintRecipe recipes[CONSTRAINT_GROUP_MAX];
    ConstraintMemory memory[CONSTRAINT_GROUP_MAX];
} PartialCheckpoint;
typedef struct CheckpointBank {
    uint64_t occupied;
    PartialCheckpoint saved;
    uint64_t has_mechanical_dependencies;
    MechanicalLinks links;
    MechanicalDependencies dependencies;
} CheckpointBank;
typedef struct CheckpointPreparation {
    uint64_t sequence;
    size_t body_count, constraint_count;
    RigidPreparedBody bodies[RIGID_RESTORE_MAX_BODIES];
    ConstraintPrepared constraints[CONSTRAINT_GROUP_MAX];
    ConstraintMemory memory[CONSTRAINT_GROUP_MAX];
    /* Always zero: absent subsystems and a write adapter cannot be bypassed by
     * claiming more coverage in a caller-provided flag. */
    uint64_t full_restore_supported;
} CheckpointPreparation;
enum CheckpointResult {
    CHECKPOINT_OK,
    CHECKPOINT_BAD_ARGUMENT,
    CHECKPOINT_INVALID_STATE,
    CHECKPOINT_EMPTY,
    CHECKPOINT_IDENTITY_CHANGED,
    CHECKPOINT_TOPOLOGY_CHANGED,
    CHECKPOINT_MASS_CHANGED
};
/* Single-owner RAM slot; not a concurrent atomic store. Save only a coherent
 * Dynamics-entry capture. No pointers into LMU, file I/O or game writes. */
int checkpoint_save(CheckpointBank *bank, const PartialCheckpoint *capture);
int checkpoint_save_mechanical(CheckpointBank *bank, const PartialCheckpoint *capture,
                               const MechanicalLinks *links,
                               const MechanicalDependencies *dependencies);
void checkpoint_clear(CheckpointBank *bank);
/* Reconstructs the saved bodies/geometry, copies impulse memory WITHOUT applying
 * it to the saved velocities, and refuses unexplained mass/inertia changes.
 * Full vehicle restoration remains unsupported. Failure never publishes output. */
int checkpoint_prepare(const CheckpointBank *bank, const CheckpointIdentity *current_identity,
                       const RigidMassProperties *current_mass, size_t body_count,
                       const ConstraintRecipe *current_topology, size_t constraint_count,
                       CheckpointPreparation *output);
int checkpoint_prepare_mechanical(const CheckpointBank *bank,
                                  const CheckpointIdentity *current_identity,
                                  const RigidMassProperties *current_mass, size_t body_count,
                                  const ConstraintRecipe *current_topology, size_t constraint_count,
                                  const MechanicalDependencies *current_dependencies,
                                  CheckpointPreparation *output);
#endif
