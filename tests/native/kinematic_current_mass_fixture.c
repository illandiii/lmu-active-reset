#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../../src/native/physics/rigid_restore_core.h"
int main(void) {
    RigidSavedBody saved[2] = {0};
    RigidMassProperties current[2];
    RigidPreparedBody out[2], untouched[2];
    for (int n = 0; n < 2; n++) {
        saved[n].mass.inverse_mass = .01;
        double old_inv[3] = {.5, .25, .125};
        memcpy(saved[n].mass.inverse_inertia_local, old_inv, sizeof(old_inv));
        double r[9] = {0, -1, 0, 1, 0, 0, 0, 0, 1};
        memcpy(saved[n].orientation, r, sizeof(r));
        double momentum[3] = {8, 6, 32};
        memcpy(saved[n].momentum_world, momentum, sizeof(momentum));
        saved[n].position_world[0] = 123;
        saved[n].velocity_world[2] = 17;
        current[n].inverse_mass = .0125;
        double new_inv[3] = {1. / 3, 1. / 5, 1. / 7};
        memcpy(current[n].inverse_inertia_local, new_inv, sizeof(new_inv));
    }
    if (rigid_prepare_restore(saved, current, 2, out) != RIGID_PREPARE_MASS_CHANGED)
        return 1;
    if (rigid_prepare_kinematics_current_mass(saved, current, 2, out) != RIGID_PREPARE_OK)
        return 2;
    const double expected_l[3] = {10, 9, 28}, expected_w[3] = {2, 3, 4};
    for (int n = 0; n < 2; n++) {
        if (out[n].state.mass.inverse_mass != current[n].inverse_mass ||
            out[n].state.position_world[0] != 123 || out[n].state.velocity_world[2] != 17)
            return 3;
        for (int k = 0; k < 3; k++)
            if (fabs(out[n].state.momentum_world[k] - expected_l[k]) > 1e-12 ||
                fabs(out[n].angular_velocity_world[k] - expected_w[k]) > 1e-12)
                return 4;
    }
    memset(out, 0xa5, sizeof(out));
    memcpy(untouched, out, sizeof(out));
    current[1].inverse_mass = 0;
    if (rigid_prepare_kinematics_current_mass(saved, current, 2, out) !=
            RIGID_PREPARE_INVALID_VALUE ||
        memcmp(out, untouched, sizeof(out)))
        return 5;
    current[1].inverse_mass = .0125;
    current[1].inverse_inertia_local[2] = 1e-320;
    if (rigid_prepare_kinematics_current_mass(saved, current, 2, out) !=
            RIGID_PREPARE_INVALID_VALUE ||
        memcmp(out, untouched, sizeof(out)))
        return 6;
    current[1].inverse_inertia_local[2] = 1. / 7;
    saved[1].orientation[0] = 2;
    if (rigid_prepare_kinematics_current_mass(saved, current, 2, out) !=
            RIGID_PREPARE_INVALID_ROTATION ||
        memcmp(out, untouched, sizeof(out)))
        return 7;
    puts("PASS: explicit current-mass kinematics preserve v/omega with analytical changed "
         "momentum; strict mode still rejects; late-body failures leave all output untouched.");
    return 0;
}
