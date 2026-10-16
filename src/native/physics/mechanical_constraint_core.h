#ifndef LMU_MECHANICAL_CONSTRAINT_CORE_H
#define LMU_MECHANICAL_CONSTRAINT_CORE_H
#include "mechanical_prepare_core.h"
#include "constraint_group_core.h"
typedef struct MechanicalConstraintPrepared {
    size_t body_count, constraint_count, row_count;
    RigidPreparedBody bodies[RIGID_RESTORE_MAX_BODIES];
    FuelPrepared fuel;
    ConstraintPrepared constraints[CONSTRAINT_GROUP_MAX];
} MechanicalConstraintPrepared;
enum MechanicalConstraintResult {
    MECHANICAL_CONSTRAINT_OK,
    MECHANICAL_CONSTRAINT_BAD_ARGUMENT,
    MECHANICAL_CONSTRAINT_BODY_FAILURE,
    MECHANICAL_CONSTRAINT_GEOMETRY_FAILURE
};
/* Combines mass/inertia restoration and constraint geometry as one numerical
 * preparation. Failure leaves output byte-for-byte unchanged. This is not an
 * atomic game transaction: subsystem coverage and a live adapter remain absent. */
int mechanical_constraint_prepare(const RigidSavedBody *saved, const RigidMassProperties *current,
                                  size_t body_count, const MechanicalLinks *links,
                                  const MechanicalDependencies *saved_deps,
                                  const MechanicalDependencies *current_deps,
                                  const ConstraintRecipe *recipes, size_t constraint_count,
                                  MechanicalConstraintPrepared *output);
#endif
