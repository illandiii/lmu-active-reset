#include "wheel_restore_core.h"
#include <math.h>

static RigidPrepareResult prepare_spin(const WheelSavedSpin saved[4],
                                       const WheelDeformation deformation[4],
                                       const RigidSavedBody hubs[4],
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
        if (saved[i].attached != 1 || saved[i].deformed > 1 || (saved[i].deformed && !deformation))
            return RIGID_PREPARE_BAD_ARGUMENT;
        if (!isfinite(saved[i].angle) || !isfinite(saved[i].angular_rate))
            return RIGID_PREPARE_INVALID_VALUE;
        prepared[i].spin = saved[i];
        double rotated[9];
        const double *r = hubs[i].orientation;
        if (deformation) {
            const double *v = deformation[i].rotation;
            if (!isfinite(v[0]) || !isfinite(v[1]) || !isfinite(v[2]) ||
                !isfinite(deformation[i].accumulated) || deformation[i].accumulated < 0)
                return RIGID_PREPARE_INVALID_VALUE;
            const double length = hypot(hypot(v[0], v[1]), v[2]);
            if (!isfinite(length) || (!saved[i].deformed && length != 0))
                return RIGID_PREPARE_INVALID_VALUE;
            if (saved[i].deformed && length > 0) {
                const double x = v[0] / length, y = v[1] / length, z = v[2] / length;
                const double c = cos(length), s = sin(length), t = 1 - c;
                const double rotation[9] = {c + x * x * t,     x * y * t - z * s, x * z * t + y * s,
                                            x * y * t + z * s, c + y * y * t,     y * z * t - x * s,
                                            x * z * t - y * s, y * z * t + x * s, c + z * z * t};
                /* Native EE4010 applies the local deformation before spin.
                 * Hub angular velocity remains in world coordinates. */
                for (int row = 0; row < 3; ++row)
                    for (int col = 0; col < 3; ++col) {
                        rotated[row * 3 + col] = 0;
                        for (int k = 0; k < 3; ++k)
                            rotated[row * 3 + col] += r[row * 3 + k] * rotation[k * 3 + col];
                    }
                r = rotated;
            }
        }
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

RigidPrepareResult wheel_prepare_spin(const WheelSavedSpin saved[4], const RigidSavedBody hubs[4],
                                      const RigidMassProperties mass[4], WheelPreparedSpin out[4]) {
    return prepare_spin(saved, NULL, hubs, mass, out);
}

RigidPrepareResult wheel_prepare_deformed_spin(const WheelSavedSpin saved[4],
                                               const WheelDeformation deformation[4],
                                               const RigidSavedBody hubs[4],
                                               const RigidMassProperties mass[4],
                                               WheelPreparedSpin out[4]) {
    if (!deformation)
        return RIGID_PREPARE_BAD_ARGUMENT;
    return prepare_spin(saved, deformation, hubs, mass, out);
}

RigidPrepareResult wheel_validate_deformed_state(const WheelSavedSpin spin[4],
                                                 const WheelDeformation deformation[4],
                                                 const RigidSavedBody hubs[4],
                                                 const RigidSavedBody wheels[4]) {
    if (!spin || !deformation || !hubs || !wheels)
        return RIGID_PREPARE_BAD_ARGUMENT;
    RigidMassProperties mass[4];
    RigidPreparedBody observed[4];
    WheelPreparedSpin prepared[4];
    for (int i = 0; i < 4; ++i)
        mass[i] = wheels[i].mass;
    RigidPrepareResult result = rigid_prepare_restore(wheels, mass, 4, observed);
    if (result != RIGID_PREPARE_OK)
        return result;
    result = wheel_prepare_deformed_spin(spin, deformation, hubs, mass, prepared);
    if (result != RIGID_PREPARE_OK)
        return result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 9; ++j)
            if (fabs(prepared[i].orientation[j] - wheels[i].orientation[j]) > 1e-8)
                return RIGID_PREPARE_INVALID_ROTATION;
        for (int j = 0; j < 3; ++j)
            if (fabs(prepared[i].angular_velocity_world[j] -
                     observed[i].angular_velocity_world[j]) > 1e-8)
                return RIGID_PREPARE_INVALID_VALUE;
    }
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
