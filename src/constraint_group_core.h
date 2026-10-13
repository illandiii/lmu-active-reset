#ifndef LMU_CONSTRAINT_GROUP_CORE_H
#define LMU_CONSTRAINT_GROUP_CORE_H
#include "ball_constraint_core.h"
#include "angular_constraint_core.h"
#define CONSTRAINT_GROUP_MAX 64
enum ConstraintKind {
    CONSTRAINT_BAR = 1,
    CONSTRAINT_BALL,
    CONSTRAINT_HINGE,
    CONSTRAINT_ORIENTATION
};
typedef struct ConstraintRecipe {
    size_t identity, body_indices[2];
    unsigned int kind;
    union {
        BarGeometryInput bar;
        BallGeometryInput ball;
        AngularGeometryInput angular;
    } geometry;
} ConstraintRecipe;
typedef struct ConstraintPrepared {
    size_t identity, body_indices[2], row_count;
    unsigned int kind;
    BarGeometryPrepared rows[3];
} ConstraintPrepared;
enum ConstraintGroupResult {
    CONSTRAINT_GROUP_OK,
    CONSTRAINT_GROUP_BAD_ARGUMENT,
    CONSTRAINT_GROUP_BAD_TOPOLOGY,
    CONSTRAINT_GROUP_UNKNOWN_KIND,
    CONSTRAINT_GROUP_INVALID_GEOMETRY
};
/* Owned values and integer indices only. All geometry is prepared before any
 * result is published. This does not validate live object identity/lifetime,
 * rebuild bias/limits/history or apply impulses. Identity is a unique local
 * slot, not a process address. No game writes are performed. */
int constraint_prepare_group(const RigidSavedBody *bodies, size_t body_count,
                             const ConstraintRecipe *recipes, size_t constraint_count,
                             ConstraintPrepared *output);
#endif
