#include "mechanical_constraint_core.h"
#include <string.h>
int mechanical_constraint_prepare(const RigidSavedBody *saved, const RigidMassProperties *current,
                                  size_t count, const MechanicalLinks *links,
                                  const MechanicalDependencies *saved_deps,
                                  const MechanicalDependencies *current_deps,
                                  const ConstraintRecipe *recipes, size_t constraint_count,
                                  MechanicalConstraintPrepared *output) {
    if (!output || !count || count > RIGID_RESTORE_MAX_BODIES || !constraint_count ||
        constraint_count > CONSTRAINT_GROUP_MAX)
        return MECHANICAL_CONSTRAINT_BAD_ARGUMENT;
    MechanicalConstraintPrepared prepared = {0};
    if (mechanical_prepare(saved, current, count, links, saved_deps, current_deps, prepared.bodies,
                           &prepared.fuel))
        return MECHANICAL_CONSTRAINT_BODY_FAILURE;
    RigidSavedBody restored[RIGID_RESTORE_MAX_BODIES];
    for (size_t i = 0; i < count; i++)
        restored[i] = prepared.bodies[i].state;
    if (constraint_prepare_group(restored, count, recipes, constraint_count, prepared.constraints))
        return MECHANICAL_CONSTRAINT_GEOMETRY_FAILURE;
    prepared.body_count = count;
    prepared.constraint_count = constraint_count;
    for (size_t i = 0; i < constraint_count; i++)
        prepared.row_count += prepared.constraints[i].row_count;
    memcpy(output, &prepared, sizeof(prepared));
    return MECHANICAL_CONSTRAINT_OK;
}
