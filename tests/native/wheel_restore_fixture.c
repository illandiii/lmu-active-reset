#include "../../src/native/physics/wheel_restore_core.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            printf("FAIL line %d\n", __LINE__);                                                    \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    WheelSavedSpin spin[4];
    RigidSavedBody hubs[4] = {0};
    RigidMassProperties mass[4];
    WheelPreparedSpin out[4], untouched[4];
    for (int i = 0; i < 4; i++) {
        spin[i] = (WheelSavedSpin){0, 2, 1, 0};
        hubs[i].mass = (RigidMassProperties){1, {1, 1, 1}};
        hubs[i].orientation[0] = hubs[i].orientation[4] = hubs[i].orientation[8] = 1;
        hubs[i].momentum_world[0] = 1;
        hubs[i].momentum_world[1] = 2;
        hubs[i].momentum_world[2] = 3;
        mass[i] = (RigidMassProperties){.1, {.5, .25, .125}};
    }
    spin[1].angle = acos(-1.) / 2;
    /* A hub rotated about Z makes the wheel spin about world Y. */
    hubs[2].orientation[0] = hubs[2].orientation[4] = 0;
    hubs[2].orientation[1] = -1;
    hubs[2].orientation[3] = 1;
    CHECK(wheel_prepare_spin(spin, hubs, mass, out) == RIGID_PREPARE_OK);
    CHECK(fabs(out[0].angular_velocity_world[0] - 3) < 1e-12);
    CHECK(fabs(out[0].angular_momentum_world[0] - 6) < 1e-12);
    CHECK(fabs(out[0].angular_momentum_world[1] - 8) < 1e-12);
    CHECK(fabs(out[0].angular_momentum_world[2] - 24) < 1e-12);
    CHECK(fabs(out[1].orientation[5] + 1) < 1e-12 && fabs(out[1].orientation[7] - 1) < 1e-12);
    CHECK(fabs(out[1].angular_momentum_world[1] - 16) < 1e-12);
    CHECK(fabs(out[1].angular_momentum_world[2] - 12) < 1e-12);
    CHECK(fabs(out[2].angular_velocity_world[0] - 1) < 1e-12 &&
          fabs(out[2].angular_velocity_world[1] - 4) < 1e-12);
    memset(out, 0xa5, sizeof(out));
    memcpy(untouched, out, sizeof(out));
    spin[3].deformed = 1;
    CHECK(wheel_prepare_spin(spin, hubs, mass, out) != RIGID_PREPARE_OK);
    CHECK(!memcmp(out, untouched, sizeof(out)));
    spin[3].deformed = 0;
    spin[3].attached = 0;
    CHECK(wheel_prepare_spin(spin, hubs, mass, out) != RIGID_PREPARE_OK);
    CHECK(!memcmp(out, untouched, sizeof(out)));
    spin[3].attached = 1;
    spin[3].angle = NAN;
    CHECK(wheel_prepare_spin(spin, hubs, mass, out) != RIGID_PREPARE_OK);
    CHECK(!memcmp(out, untouched, sizeof(out)));
    spin[3].angle = 0;
    mass[3].inverse_inertia_local[2] = 0;
    CHECK(wheel_prepare_spin(spin, hubs, mass, out) != RIGID_PREPARE_OK);
    CHECK(!memcmp(out, untouched, sizeof(out)));
    puts("PASS wheel spin: rotated parent, angle, omega, anisotropic momentum, all-four atomic "
         "preparation and unsupported-state rejection");
    return 0;
}
