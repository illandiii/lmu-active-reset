#include "constraint_group_core.h"
#include <string.h>
int constraint_prepare_group(const RigidSavedBody *bodies, size_t body_count,
                             const ConstraintRecipe *recipes, size_t count,
                             ConstraintPrepared *output) {
    if (!bodies || !recipes || !output || !body_count || body_count > RIGID_RESTORE_MAX_BODIES ||
        !count || count > CONSTRAINT_GROUP_MAX)
        return CONSTRAINT_GROUP_BAD_ARGUMENT;
    ConstraintPrepared result[CONSTRAINT_GROUP_MAX] = {0};
    for (size_t i = 0; i < count; i++) {
        const ConstraintRecipe *r = recipes + i;
        ConstraintPrepared *p = result + i;
        if (r->body_indices[0] >= body_count || r->body_indices[1] >= body_count ||
            r->body_indices[0] == r->body_indices[1])
            return CONSTRAINT_GROUP_BAD_TOPOLOGY;
        for (size_t j = 0; j < i; j++)
            if (recipes[j].identity == r->identity)
                return CONSTRAINT_GROUP_BAD_TOPOLOGY;
        RigidSavedBody pair[2] = {bodies[r->body_indices[0]], bodies[r->body_indices[1]]};
        int status;
        p->identity = r->identity;
        p->body_indices[0] = r->body_indices[0];
        p->body_indices[1] = r->body_indices[1];
        p->kind = r->kind;
        switch (r->kind) {
        case CONSTRAINT_BAR:
            p->row_count = 1;
            status = bar_prepare_geometry(pair, &r->geometry.bar, p->rows);
            break;
        case CONSTRAINT_BALL:
            p->row_count = 3;
            status = ball_prepare_geometry(pair, &r->geometry.ball, p->rows);
            break;
        case CONSTRAINT_HINGE:
            p->row_count = 2;
            status = angular_prepare_geometry(pair, &r->geometry.angular, 2, p->rows);
            break;
        case CONSTRAINT_ORIENTATION:
            p->row_count = 3;
            status = angular_prepare_geometry(pair, &r->geometry.angular, 3, p->rows);
            break;
        default:
            return CONSTRAINT_GROUP_UNKNOWN_KIND;
        }
        if (status)
            return CONSTRAINT_GROUP_INVALID_GEOMETRY;
    }
    memcpy(output, result, count * sizeof(*output));
    return CONSTRAINT_GROUP_OK;
}
