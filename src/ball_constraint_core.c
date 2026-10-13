#include "ball_constraint_core.h"
#include <math.h>
#include <string.h>
static void rotate(const double *r, const double *v, double *o) {
    for (int j = 0; j < 3; j++)
        o[j] = r[j * 3] * v[0] + r[j * 3 + 1] * v[1] + r[j * 3 + 2] * v[2];
}
static void cross(const double *a, const double *b, double *o) {
    o[0] = a[1] * b[2] - a[2] * b[1];
    o[1] = a[2] * b[0] - a[0] * b[2];
    o[2] = a[0] * b[1] - a[1] * b[0];
}
static void symmetric(const double *m, const double *v, double *o) {
    o[0] = m[0] * v[0] + m[1] * v[1] + m[3] * v[2];
    o[1] = m[1] * v[0] + m[2] * v[1] + m[4] * v[2];
    o[2] = m[3] * v[0] + m[4] * v[1] + m[5] * v[2];
}
int ball_prepare_geometry(const RigidSavedBody bodies[2], const BallGeometryInput *input,
                          BarGeometryPrepared output[3]) {
    if (!bodies || !input || !output)
        return BAR_BAD_ARGUMENT;
    RigidMassProperties mass[2] = {bodies[0].mass, bodies[1].mass};
    RigidPreparedBody b[2];
    if (rigid_prepare_restore(bodies, mass, 2, b))
        return BAR_INVALID_BODY;
    double offsets[2][3];
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 3; j++)
            if (!isfinite(input->anchors_local[i][j]))
                return BAR_INVALID_GEOMETRY;
        rotate(bodies[i].orientation, input->anchors_local[i], offsets[i]);
    }
    BarGeometryPrepared prepared[3] = {0};
    for (int axis = 0; axis < 3; axis++) {
        BarGeometryPrepared *p = prepared + axis;
        double effective = input->softness[axis];
        if (!isfinite(effective) || effective < 0)
            return BAR_INVALID_GEOMETRY;
        p->jacobian[0][axis] = 1;
        p->jacobian[2][axis] = -1;
        cross(offsets[0], p->jacobian[0], p->jacobian[1]);
        cross(offsets[1], p->jacobian[2], p->jacobian[3]);
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 3; j++)
                p->weighted[2 * i][j] = mass[i].inverse_mass * p->jacobian[2 * i][j];
            symmetric(b[i].inverse_inertia_world, p->jacobian[2 * i + 1], p->weighted[2 * i + 1]);
        }
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 3; j++)
                effective += p->jacobian[i][j] * p->weighted[i][j];
        if (!isfinite(effective) || effective <= 0 || !isfinite(1.125 / effective))
            return BAR_INVALID_GEOMETRY;
        p->inverse_effective_mass = 1.125 / effective;
    }
    memcpy(output, prepared, sizeof(prepared));
    return BAR_OK;
}
