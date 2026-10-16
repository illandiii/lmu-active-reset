#include "rigid_restore_core.h"
#include <math.h>

static int finite_values(const double *values, size_t count) {
    for (size_t i = 0; i < count; i++)
        if (!isfinite(values[i]))
            return 0;
    return 1;
}
static int valid_mass(const RigidMassProperties *m) {
    if (!isfinite(m->inverse_mass) || m->inverse_mass <= 0)
        return 0;
    for (int i = 0; i < 3; i++)
        if (!isfinite(m->inverse_inertia_local[i]) || m->inverse_inertia_local[i] <= 0)
            return 0;
    return 1;
}
static int valid_rotation(const double *r) {
    /* Numerical input validation tolerance, not a measured LMU reset tolerance. */
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            double dot = 0;
            for (int k = 0; k < 3; k++)
                dot += r[3 * i + k] * r[3 * j + k];
            if (!isfinite(dot) || fabs(dot - (i == j)) > 1e-8)
                return 0;
        }
    double det = r[0] * (r[4] * r[8] - r[5] * r[7]) - r[1] * (r[3] * r[8] - r[5] * r[6]) +
                 r[2] * (r[3] * r[7] - r[4] * r[6]);
    return isfinite(det) && fabs(det - 1) < 1e-8;
}

RigidPrepareResult rigid_prepare_restore(const RigidSavedBody *saved,
                                         const RigidMassProperties *current_mass, size_t count,
                                         RigidPreparedBody *output) {
    if (!saved || !current_mass || !output || count == 0 || count > RIGID_RESTORE_MAX_BODIES)
        return RIGID_PREPARE_BAD_ARGUMENT;
    RigidPreparedBody prepared[RIGID_RESTORE_MAX_BODIES];
    for (size_t n = 0; n < count; n++) {
        const RigidSavedBody *s = &saved[n];
        const RigidMassProperties *mass = &current_mass[n];
        if (!valid_mass(&s->mass) || !valid_mass(mass) || !finite_values(s->position_world, 3) ||
            !finite_values(s->velocity_world, 3) || !finite_values(s->orientation, 9) ||
            !finite_values(s->momentum_world, 3))
            return RIGID_PREPARE_INVALID_VALUE;
        if (!valid_rotation(s->orientation))
            return RIGID_PREPARE_INVALID_ROTATION;
        if (s->mass.inverse_mass != mass->inverse_mass)
            return RIGID_PREPARE_MASS_CHANGED;
        for (int k = 0; k < 3; k++)
            if (s->mass.inverse_inertia_local[k] != mass->inverse_inertia_local[k])
                return RIGID_PREPARE_MASS_CHANGED;
        prepared[n].state = *s;
        const double *r = s->orientation;
        const int pairs[6][2] = {{0, 0}, {0, 1}, {1, 1}, {0, 2}, {1, 2}, {2, 2}};
        for (int p = 0; p < 6; p++) {
            double value = 0;
            for (int k = 0; k < 3; k++)
                value += r[3 * pairs[p][0] + k] * mass->inverse_inertia_local[k] *
                         r[3 * pairs[p][1] + k];
            if (!isfinite(value))
                return RIGID_PREPARE_INVALID_VALUE;
            prepared[n].inverse_inertia_world[p] = value;
        }
        const double *a = prepared[n].inverse_inertia_world;
        const double *l = s->momentum_world;
        double *w = prepared[n].angular_velocity_world;
        w[0] = a[0] * l[0] + a[1] * l[1] + a[3] * l[2];
        w[1] = a[1] * l[0] + a[2] * l[1] + a[4] * l[2];
        w[2] = a[3] * l[0] + a[4] * l[1] + a[5] * l[2];
        if (!finite_values(w, 3))
            return RIGID_PREPARE_INVALID_VALUE;
    }
    /* Only caller-owned typed payloads are assigned. No arbitrary object copy. */
    for (size_t n = 0; n < count; n++)
        output[n] = prepared[n];
    return RIGID_PREPARE_OK;
}

RigidPrepareResult rigid_prepare_kinematics_current_mass(const RigidSavedBody *saved,
                                                         const RigidMassProperties *current_mass,
                                                         size_t count, RigidPreparedBody *output) {
    if (!saved || !current_mass || !output || count == 0 || count > RIGID_RESTORE_MAX_BODIES)
        return RIGID_PREPARE_BAD_ARGUMENT;
    RigidMassProperties previous_mass[RIGID_RESTORE_MAX_BODIES];
    RigidPreparedBody previous[RIGID_RESTORE_MAX_BODIES];
    RigidSavedBody adjusted[RIGID_RESTORE_MAX_BODIES];
    for (size_t n = 0; n < count; n++)
        previous_mass[n] = saved[n].mass;
    RigidPrepareResult status = rigid_prepare_restore(saved, previous_mass, count, previous);
    if (status != RIGID_PREPARE_OK)
        return status;
    for (size_t n = 0; n < count; n++) {
        if (!valid_mass(&current_mass[n]))
            return RIGID_PREPARE_INVALID_VALUE;
        adjusted[n] = saved[n];
        adjusted[n].mass = current_mass[n];
        const double *r = saved[n].orientation;
        const double *omega = previous[n].angular_velocity_world;
        double local_momentum[3];
        for (int k = 0; k < 3; k++) {
            double local_omega = 0;
            for (int j = 0; j < 3; j++)
                local_omega += r[j * 3 + k] * omega[j];
            local_momentum[k] = local_omega / current_mass[n].inverse_inertia_local[k];
            if (!isfinite(local_momentum[k]))
                return RIGID_PREPARE_INVALID_VALUE;
        }
        for (int j = 0; j < 3; j++) {
            double momentum = 0;
            for (int k = 0; k < 3; k++)
                momentum += r[j * 3 + k] * local_momentum[k];
            if (!isfinite(momentum))
                return RIGID_PREPARE_INVALID_VALUE;
            adjusted[n].momentum_world[j] = momentum;
        }
    }
    return rigid_prepare_restore(adjusted, current_mass, count, output);
}
