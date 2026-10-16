#include "angular_constraint_core.h"
#include <math.h>
#include <string.h>
static void symmetric(const double *m, const double *v, double *o) {
    o[0] = m[0] * v[0] + m[1] * v[1] + m[3] * v[2];
    o[1] = m[1] * v[0] + m[2] * v[1] + m[4] * v[2];
    o[2] = m[3] * v[0] + m[4] * v[1] + m[5] * v[2];
}
int angular_prepare_geometry(const RigidSavedBody bodies[2], const AngularGeometryInput *input,
                             size_t count, BarGeometryPrepared output[3]) {
    if (!bodies || !input || !output || count < 1 || count > 3)
        return BAR_BAD_ARGUMENT;
    RigidMassProperties mass[2] = {bodies[0].mass, bodies[1].mass};
    RigidPreparedBody b[2];
    if (rigid_prepare_restore(bodies, mass, 2, b))
        return BAR_INVALID_BODY;
    BarGeometryPrepared result[3] = {0};
    for (size_t axis = 0; axis < count; axis++) {
        BarGeometryPrepared *p = result + axis;
        double effective = input->softness[axis];
        if (!isfinite(effective) || effective < 0)
            return BAR_INVALID_GEOMETRY;
        for (int j = 0; j < 3; j++)
            if (!isfinite(input->axes_local[axis][j]))
                return BAR_INVALID_GEOMETRY;
        for (int j = 0; j < 3; j++) {
            const double *r = bodies[0].orientation + j * 3;
            const double *a = input->axes_local[axis];
            p->jacobian[1][j] = r[0] * a[0] + r[1] * a[1] + r[2] * a[2];
            p->jacobian[3][j] = -p->jacobian[1][j];
        }
        symmetric(b[0].inverse_inertia_world, p->jacobian[1], p->weighted[1]);
        symmetric(b[1].inverse_inertia_world, p->jacobian[3], p->weighted[3]);
        for (int j = 0; j < 3; j++)
            effective += (p->weighted[1][j] - p->weighted[3][j]) * p->jacobian[1][j];
        if (!isfinite(effective) || effective <= 0 || !isfinite(1.125 / effective))
            return BAR_INVALID_GEOMETRY;
        p->inverse_effective_mass = 1.125 / effective;
    }
    memcpy(output, result, sizeof(result));
    return BAR_OK;
}
