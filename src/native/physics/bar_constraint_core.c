#include "bar_constraint_core.h"
#include <math.h>
static void rotate(const double *r, const double *v, double *o) {
    for (int j = 0; j < 3; j++)
        o[j] = r[3 * j] * v[0] + r[3 * j + 1] * v[1] + r[3 * j + 2] * v[2];
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
int bar_prepare_geometry(const RigidSavedBody bodies[2], const BarGeometryInput *input,
                         BarGeometryPrepared *output) {
    if (!bodies || !input || !output)
        return BAR_BAD_ARGUMENT;
    RigidPreparedBody b[2];
    RigidMassProperties m[2] = {bodies[0].mass, bodies[1].mass};
    if (rigid_prepare_restore(bodies, m, 2, b))
        return BAR_INVALID_BODY;
    if (!isfinite(input->softness) || input->softness < 0)
        return BAR_INVALID_GEOMETRY;
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 3; j++)
            if (!isfinite(input->anchors_local[i][j]))
                return BAR_INVALID_GEOMETRY;
    BarGeometryPrepared p = {0};
    double offsets[2][3], delta[3];
    for (int i = 0; i < 2; i++)
        rotate(bodies[i].orientation, input->anchors_local[i], offsets[i]);
    for (int j = 0; j < 3; j++)
        delta[j] = ((offsets[0][j] + bodies[0].position_world[j]) - bodies[1].position_world[j]) -
                   offsets[1][j];
    p.length = sqrt(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
    if (!isfinite(p.length) || p.length <= 0)
        return BAR_INVALID_GEOMETRY;
    for (int j = 0; j < 3; j++) {
        p.jacobian[0][j] = delta[j] / p.length;
        p.jacobian[2][j] = -p.jacobian[0][j];
    }
    cross(offsets[0], p.jacobian[0], p.jacobian[1]);
    cross(offsets[1], p.jacobian[2], p.jacobian[3]);
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 3; j++)
            p.weighted[2 * i][j] = bodies[i].mass.inverse_mass * p.jacobian[2 * i][j];
        symmetric(b[i].inverse_inertia_world, p.jacobian[2 * i + 1], p.weighted[2 * i + 1]);
    }
    double effective = input->softness;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 3; j++)
            effective += p.jacobian[i][j] * p.weighted[i][j];
    if (!isfinite(effective) || effective <= 0 || !isfinite(1 / effective))
        return BAR_INVALID_GEOMETRY;
    /* 14200 native 3EF779 reads 1.125 from RVA 1812AC0, a solver relaxation
     * factor. The cached coefficient is not merely the inverse diagonal. */
    p.inverse_effective_mass = 1.125 / effective;
    *output = p;
    return BAR_OK;
}
int bar_prepare_warm_start(const BarGeometryPrepared *g, const BarWarmState *before, double lower,
                           double upper, BarWarmState *output) {
    if (!g || !before || !output)
        return BAR_BAD_ARGUMENT;
    if (!isfinite(lower) || !isfinite(upper) || lower > upper || !isfinite(before->impulse))
        return BAR_INVALID_GEOMETRY;
    BarWarmState p = *before;
    p.impulse = fmin(upper, fmax(lower, before->impulse));
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 3; j++) {
            if (!isfinite(before->velocity[i][j]) || !isfinite(before->omega[i][j]) ||
                !isfinite(g->weighted[2 * i][j]) || !isfinite(g->weighted[2 * i + 1][j]))
                return BAR_INVALID_BODY;
            p.velocity[i][j] += p.impulse * g->weighted[2 * i][j];
            p.omega[i][j] += p.impulse * g->weighted[2 * i + 1][j];
            if (!isfinite(p.velocity[i][j]) || !isfinite(p.omega[i][j]))
                return BAR_INVALID_BODY;
        }
    *output = p;
    return BAR_OK;
}
