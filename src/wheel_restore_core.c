#include "wheel_restore_core.h"
#include <math.h>

RigidPrepareResult wheel_prepare_spin(const WheelSavedSpin saved[4], const RigidSavedBody hubs[4],
                                      const RigidMassProperties wheel_mass[4],
                                      WheelPreparedSpin output[4]) {
    if (!saved || !hubs || !wheel_mass || !output)
        return RIGID_PREPARE_BAD_ARGUMENT;
    RigidMassProperties hub_mass[4];
    RigidPreparedBody parents[4], bodies[4];
    RigidSavedBody wheels[4];
    WheelPreparedSpin prepared[4];
    for (int i = 0; i < 4; i++)
        hub_mass[i] = hubs[i].mass;
    RigidPrepareResult status = rigid_prepare_restore(hubs, hub_mass, 4, parents);
    if (status != RIGID_PREPARE_OK)
        return status;
    for (int i = 0; i < 4; i++) {
        if (saved[i].attached != 1 || saved[i].deformed != 0)
            return RIGID_PREPARE_BAD_ARGUMENT;
        if (!isfinite(saved[i].angle) || !isfinite(saved[i].angular_rate))
            return RIGID_PREPARE_INVALID_VALUE;
        prepared[i].spin = saved[i];
        const double *r = hubs[i].orientation;
        double s = sin(saved[i].angle), c = cos(saved[i].angle);
        for (int j = 0; j < 3; j++) {
            prepared[i].orientation[3 * j] = r[3 * j];
            prepared[i].orientation[3 * j + 1] = c * r[3 * j + 1] + s * r[3 * j + 2];
            prepared[i].orientation[3 * j + 2] = c * r[3 * j + 2] - s * r[3 * j + 1];
            prepared[i].angular_velocity_world[j] =
                parents[i].angular_velocity_world[j] + saved[i].angular_rate * r[3 * j];
            if (!isfinite(prepared[i].angular_velocity_world[j]))
                return RIGID_PREPARE_INVALID_VALUE;
        }
        wheels[i] = (RigidSavedBody){0};
        wheels[i].mass = wheel_mass[i];
        for (int j = 0; j < 9; j++)
            wheels[i].orientation[j] = prepared[i].orientation[j];
        double local_momentum[3];
        for (int k = 0; k < 3; k++) {
            double d = wheel_mass[i].inverse_inertia_local[k];
            if (!isfinite(d) || d <= 0)
                return RIGID_PREPARE_INVALID_VALUE;
            double omega = 0;
            for (int j = 0; j < 3; j++)
                omega += prepared[i].orientation[3 * j + k] * prepared[i].angular_velocity_world[j];
            local_momentum[k] = omega / d;
            if (!isfinite(local_momentum[k]))
                return RIGID_PREPARE_INVALID_VALUE;
        }
        for (int j = 0; j < 3; j++) {
            double l = 0;
            for (int k = 0; k < 3; k++)
                l += prepared[i].orientation[3 * j + k] * local_momentum[k];
            wheels[i].momentum_world[j] = l;
            prepared[i].angular_momentum_world[j] = l;
        }
    }
    status = rigid_prepare_restore(wheels, wheel_mass, 4, bodies);
    if (status != RIGID_PREPARE_OK)
        return status;
    for (int i = 0; i < 4; i++)
        for (int k = 0; k < 6; k++)
            prepared[i].inverse_inertia_world[k] = bodies[i].inverse_inertia_world[k];
    for (int i = 0; i < 4; i++)
        output[i] = prepared[i];
    return RIGID_PREPARE_OK;
}

RigidPrepareResult wheel_prepare_consistent_checkpoint(const WheelSavedSpin saved[4],
                                                       const RigidSavedBody hubs[4],
                                                       const RigidSavedBody wheels[4],
                                                       WheelPreparedSpin output[4]) {
    if (!saved || !hubs || !wheels || !output)
        return RIGID_PREPARE_BAD_ARGUMENT;
    RigidMassProperties mass[4];
    RigidPreparedBody observed[4];
    WheelPreparedSpin prepared[4];
    for (int i = 0; i < 4; i++)
        mass[i] = wheels[i].mass;
    RigidPrepareResult status = rigid_prepare_restore(wheels, mass, 4, observed);
    if (status != RIGID_PREPARE_OK)
        return status;
    status = wheel_prepare_spin(saved, hubs, mass, prepared);
    if (status != RIGID_PREPARE_OK)
        return status;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 9; j++)
            if (fabs(prepared[i].orientation[j] - wheels[i].orientation[j]) > 1e-8)
                return RIGID_PREPARE_INVALID_ROTATION;
        for (int j = 0; j < 3; j++)
            if (fabs(prepared[i].angular_velocity_world[j] -
                     observed[i].angular_velocity_world[j]) > 1e-8)
                return RIGID_PREPARE_INVALID_VALUE;
    }
    for (int i = 0; i < 4; i++)
        output[i] = prepared[i];
    return RIGID_PREPARE_OK;
}
