#include "../../src/native/physics/angular_constraint_core.h"
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
    RigidSavedBody b[2] = {0};
    AngularGeometryInput in = {0};
    BarGeometryPrepared out[3], prior[3];
    for (int i = 0; i < 2; i++) {
        b[i].mass.inverse_mass = 1;
        for (int j = 0; j < 3; j++) {
            b[i].mass.inverse_inertia_local[j] = 1;
            b[i].orientation[j * 4] = 1;
        }
    }
    for (int j = 0; j < 3; j++)
        in.axes_local[j][j] = 1;
    CHECK(!angular_prepare_geometry(b, &in, 3, out));
    for (int j = 0; j < 3; j++) {
        CHECK(out[j].inverse_effective_mass == .5625);
        CHECK(out[j].weighted[1][j] == 1 && out[j].weighted[3][j] == -1);
    }
    b[0].orientation[0] = 0;
    b[0].orientation[1] = -1;
    b[0].orientation[3] = 1;
    b[0].orientation[4] = 0;
    CHECK(!angular_prepare_geometry(b, &in, 2, out));
    CHECK(out[0].jacobian[1][1] == 1 && out[1].jacobian[1][0] == -1 &&
          out[2].inverse_effective_mass == 0);
    in.softness[2] = 2;
    CHECK(!angular_prepare_geometry(b, &in, 3, out));
    CHECK(out[2].inverse_effective_mass == .28125);
    memcpy(prior, out, sizeof(out));
    in.axes_local[2][2] = NAN;
    CHECK(angular_prepare_geometry(b, &in, 3, out) != 0 && memcmp(prior, out, sizeof(out)) == 0);
    CHECK(angular_prepare_geometry(b, &in, 4, out) != 0 && memcmp(prior, out, sizeof(out)) == 0);
    puts("PASS angular geometry: 2/3 rows, rotated axes, softness, atomic invalid final row and "
         "count. Local only.");
    return 0;
}
