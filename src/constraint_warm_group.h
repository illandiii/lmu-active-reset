#ifndef LMU_CONSTRAINT_WARM_GROUP_H
#define LMU_CONSTRAINT_WARM_GROUP_H
#include "constraint_group_core.h"
typedef struct ConstraintRowMemory {
    double impulse, lower, upper;
} ConstraintRowMemory;
typedef struct ConstraintMemory {
    size_t identity, body_indices[2], row_count;
    unsigned int kind;
    ConstraintRowMemory rows[3];
} ConstraintMemory;
typedef struct ConstraintVelocity {
    double linear[3], angular[3];
} ConstraintVelocity;
typedef struct ConstraintWarmPrepared {
    size_t body_count, constraint_count;
    ConstraintVelocity bodies[RIGID_RESTORE_MAX_BODIES];
    ConstraintMemory memory[CONSTRAINT_GROUP_MAX];
} ConstraintWarmPrepared;
enum ConstraintWarmResult {
    CONSTRAINT_WARM_OK,
    CONSTRAINT_WARM_BAD_ARGUMENT,
    CONSTRAINT_WARM_TOPOLOGY_MISMATCH,
    CONSTRAINT_WARM_INVALID_VALUE
};
/* Pure numerical warm-start, in the supplied constraint/row order. Bodies
 * shared by different constraints accumulate updates. Bounds must already
 * belong to the preparation phase; this does not recreate limits or bias.
 * Call only on velocities BEFORE warm-start, never on post-solver velocities
 * from a checkpoint (that would apply the cached impulses twice).
 * Does not change input, momentum, or the game. Entire output unchanged on error. */
int constraint_warm_prepare(const ConstraintVelocity *before, size_t body_count,
                            const ConstraintPrepared *geometry, const ConstraintMemory *memory,
                            size_t count, ConstraintWarmPrepared *output);
#endif
