/* Standalone owned-memory tests. Never opens or modifies the LMU process. */
#include "rigid_restore_core.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define REQUIRE(x)                                                                                 \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static int near(double a, double b, double tolerance) {
    return isfinite(a) && isfinite(b) &&
           fabs(a - b) <= tolerance * fmax(1.0, fmax(fabs(a), fabs(b)));
}
static RigidSavedBody base_body(void) {
    RigidSavedBody b = {0};
    b.mass.inverse_mass = 0.01;
    b.mass.inverse_inertia_local[0] = 0.5;
    b.mass.inverse_inertia_local[1] = 0.25;
    b.mass.inverse_inertia_local[2] = 0.125;
    b.orientation[0] = b.orientation[4] = b.orientation[8] = 1;
    b.position_world[0] = 100;
    b.position_world[1] = 20;
    b.position_world[2] = -30;
    b.velocity_world[0] = 3;
    b.velocity_world[1] = -2;
    b.velocity_world[2] = 1;
    return b;
}
static void rotate(const double *r, const double *local, double *world) {
    for (int i = 0; i < 3; i++)
        world[i] = r[3 * i] * local[0] + r[3 * i + 1] * local[1] + r[3 * i + 2] * local[2];
}
static int model_tests(void) {
    RigidSavedBody b = base_body();
    RigidPreparedBody out;
    /* Anisotropic inertia under a known 45-degree rotation about z. */
    double c = sqrt(0.5);
    b.orientation[0] = c;
    b.orientation[1] = -c;
    b.orientation[3] = c;
    b.orientation[4] = c;
    b.momentum_world[0] = 2;
    b.momentum_world[1] = 4;
    b.momentum_world[2] = 8;
    REQUIRE(rigid_prepare_restore(&b, &b.mass, 1, &out) == RIGID_PREPARE_OK);
    const double tensor[6] = {0.375, 0.125, 0.375, 0, 0, 0.125};
    for (int i = 0; i < 6; i++)
        REQUIRE(near(out.inverse_inertia_world[i], tensor[i], 1e-12));
    REQUIRE(near(out.angular_velocity_world[0], 1.25, 1e-12));
    REQUIRE(near(out.angular_velocity_world[1], 1.75, 1e-12));
    REQUIRE(near(out.angular_velocity_world[2], 1, 1e-12));

    /* Arbitrary rotation axes: construct angular momentum from local inertia
     * and known angular velocity, then require the inverse operation to recover
     * that velocity and rotational energy in world space. */
    for (int n = 1; n <= 512; n++) {
        b = base_body();
        double angle = n * 0.079, co = cos(angle), si = sin(angle), t = 1 - co;
        double x = sin(n * 0.11), y = cos(n * 0.17), z = sin(n * 0.23) + 0.5;
        double length = sqrt(x * x + y * y + z * z);
        x /= length;
        y /= length;
        z /= length;
        const double r[9] = {t * x * x + co,     t * x * y - si * z, t * x * z + si * y,
                             t * x * y + si * z, t * y * y + co,     t * y * z - si * x,
                             t * x * z - si * y, t * y * z + si * x, t * z * z + co};
        for (int i = 0; i < 9; i++)
            b.orientation[i] = r[i];
        double local_w[3] = {sin(n * 0.7) * 25, cos(n * 0.3) * 10, sin(n * 0.2) * 5};
        double local_l[3], expected[3], energy = 0;
        for (int i = 0; i < 3; i++) {
            local_l[i] = local_w[i] / b.mass.inverse_inertia_local[i];
            energy += local_l[i] * local_w[i];
        }
        rotate(r, local_l, b.momentum_world);
        rotate(r, local_w, expected);
        REQUIRE(rigid_prepare_restore(&b, &b.mass, 1, &out) == RIGID_PREPARE_OK);
        double world_energy = 0;
        for (int i = 0; i < 3; i++) {
            REQUIRE(near(out.angular_velocity_world[i], expected[i], 1e-11));
            REQUIRE(out.state.position_world[i] == b.position_world[i]);
            REQUIRE(out.state.velocity_world[i] == b.velocity_world[i]);
            world_energy += b.momentum_world[i] * out.angular_velocity_world[i];
        }
        REQUIRE(near(world_energy, energy, 1e-11));
    }
    RigidSavedBody saved[RIGID_RESTORE_MAX_BODIES];
    RigidMassProperties current[RIGID_RESTORE_MAX_BODIES];
    RigidPreparedBody batch[RIGID_RESTORE_MAX_BODIES], untouched[RIGID_RESTORE_MAX_BODIES];
    for (int i = 0; i < RIGID_RESTORE_MAX_BODIES; i++) {
        saved[i] = base_body();
        current[i] = saved[i].mass;
    }
    REQUIRE(rigid_prepare_restore(saved, current, RIGID_RESTORE_MAX_BODIES, batch) ==
            RIGID_PREPARE_OK);
    memset(batch, 0xa5, sizeof(batch));
    memcpy(untouched, batch, sizeof(batch));
    const int last = RIGID_RESTORE_MAX_BODIES - 1;
    for (int failure = 0; failure < 8; failure++) {
        saved[last] = base_body();
        current[last] = saved[last].mass;
        RigidPrepareResult expected = RIGID_PREPARE_INVALID_VALUE;
        switch (failure) {
        case 0:
            saved[last].position_world[1] = NAN;
            break;
        case 1:
            saved[last].momentum_world[2] = INFINITY;
            break;
        case 2:
            saved[last].orientation[0] = -1;
            expected = RIGID_PREPARE_INVALID_ROTATION;
            break;
        case 3:
            saved[last].orientation[0] = 1.001;
            expected = RIGID_PREPARE_INVALID_ROTATION;
            break;
        case 4:
            current[last].inverse_mass *= 2;
            expected = RIGID_PREPARE_MASS_CHANGED;
            break;
        case 5:
            current[last].inverse_inertia_local[2] *= 2;
            expected = RIGID_PREPARE_MASS_CHANGED;
            break;
        case 6:
            saved[last].mass.inverse_mass = 0;
            break;
        case 7:
            saved[last].mass.inverse_inertia_local[0] = DBL_MAX;
            current[last] = saved[last].mass;
            saved[last].momentum_world[0] = 2;
            break;
        }
        REQUIRE(rigid_prepare_restore(saved, current, RIGID_RESTORE_MAX_BODIES, batch) == expected);
        REQUIRE(memcmp(batch, untouched, sizeof(batch)) == 0);
    }
    REQUIRE(rigid_prepare_restore(saved, current, 0, batch) == RIGID_PREPARE_BAD_ARGUMENT);
    REQUIRE(rigid_prepare_restore(saved, current, RIGID_RESTORE_MAX_BODIES + 1, batch) ==
            RIGID_PREPARE_BAD_ARGUMENT);
    REQUIRE(rigid_prepare_restore(NULL, current, 1, batch) == RIGID_PREPARE_BAD_ARGUMENT);
    REQUIRE(rigid_prepare_restore(saved, NULL, 1, batch) == RIGID_PREPARE_BAD_ARGUMENT);
    REQUIRE(rigid_prepare_restore(saved, current, 1, NULL) == RIGID_PREPARE_BAD_ARGUMENT);
    REQUIRE(memcmp(batch, untouched, sizeof(batch)) == 0);
    puts("PASS analytic tensor; 512 arbitrary rotations and energy invariance; 32-body batch; 8 "
         "late failures without partial output; invalid arguments. MODEL ONLY.");
    return 0;
}
static int read_values(FILE *f, double *values, int count) {
    for (int i = 0; i < count; i++)
        if (fscanf(f, "%lf", &values[i]) != 1 || !isfinite(values[i]))
            return 0;
    return 1;
}
static int recorded_test(const char *path) {
    FILE *f = fopen(path, "r");
    REQUIRE(f != NULL);
    int count = 0;
    REQUIRE(fscanf(f, "%d", &count) == 1 && count > 0 && count <= RIGID_RESTORE_MAX_BODIES);
    RigidSavedBody saved[RIGID_RESTORE_MAX_BODIES];
    RigidMassProperties mass[RIGID_RESTORE_MAX_BODIES];
    RigidPreparedBody prepared[RIGID_RESTORE_MAX_BODIES];
    double expected[RIGID_RESTORE_MAX_BODIES][9];
    for (int i = 0; i < count; i++) {
        RigidSavedBody *s = &saved[i];
        REQUIRE(read_values(f, &s->mass.inverse_mass, 1));
        REQUIRE(read_values(f, s->mass.inverse_inertia_local, 3));
        REQUIRE(read_values(f, s->position_world, 3));
        REQUIRE(read_values(f, s->velocity_world, 3));
        REQUIRE(read_values(f, s->orientation, 9));
        REQUIRE(read_values(f, s->momentum_world, 3));
        REQUIRE(read_values(f, expected[i], 9));
        mass[i] = s->mass;
    }
    char extra;
    REQUIRE(fscanf(f, " %c", &extra) == EOF);
    fclose(f);
    REQUIRE(rigid_prepare_restore(saved, mass, (size_t)count, prepared) == RIGID_PREPARE_OK);
    double max_tensor = 0, max_omega = 0;
    for (int i = 0; i < count; i++) {
        for (int k = 0; k < 6; k++) {
            REQUIRE(near(prepared[i].inverse_inertia_world[k], expected[i][k], 1e-10));
            max_tensor =
                fmax(max_tensor, fabs(prepared[i].inverse_inertia_world[k] - expected[i][k]));
        }
        for (int k = 0; k < 3; k++) {
            REQUIRE(near(prepared[i].angular_velocity_world[k], expected[i][6 + k], 1e-10));
            max_omega =
                fmax(max_omega, fabs(prepared[i].angular_velocity_world[k] - expected[i][6 + k]));
        }
    }
    printf("PASS %d recorded bodies; inverse-inertia max error %.17g; angular-velocity max error "
           "%.17g. ASYNCHRONOUS MONITOR DATA, NOT LIVE RESTORE.\n",
           count, max_tensor, max_omega);
    return 0;
}
int main(int argc, char **argv) {
    if (model_tests())
        return 1;
    if (argc == 2)
        return recorded_test(argv[1]);
    if (argc != 1) {
        fputs("usage: rigid-restore-fixture.exe [numeric-fixture]\n", stderr);
        return 2;
    }
    return 0;
}
