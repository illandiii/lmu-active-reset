#include "../../src/native/physics/ball_constraint_core.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            printf("FAIL Ball line %d\n", __LINE__);                                               \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    RigidSavedBody b[2] = {0};
    BallGeometryInput g = {0};
    BarGeometryPrepared rows[3], old[3];
    for (int i = 0; i < 2; i++) {
        b[i].mass = (RigidMassProperties){1, {1, 1, 1}};
        b[i].orientation[0] = b[i].orientation[4] = b[i].orientation[8] = 1;
    }
    CHECK(!ball_prepare_geometry(b, &g, rows));
    for (int j = 0; j < 3; j++)
        CHECK(rows[j].jacobian[0][j] == 1 && rows[j].jacobian[2][j] == -1 &&
              rows[j].inverse_effective_mass == .5625);
    g.anchors_local[0][1] = g.anchors_local[1][1] = 1;
    CHECK(!ball_prepare_geometry(b, &g, rows));
    CHECK(rows[0].inverse_effective_mass == .28125 && rows[1].inverse_effective_mass == .5625 &&
          rows[2].inverse_effective_mass == .28125);
    CHECK(rows[0].jacobian[1][2] == -1 && rows[2].jacobian[1][0] == 1);
    g.softness[2] = 2;
    CHECK(!ball_prepare_geometry(b, &g, rows));
    CHECK(rows[2].inverse_effective_mass == .1875);
    memcpy(old, rows, sizeof(old));
    g.softness[2] = NAN;
    CHECK(ball_prepare_geometry(b, &g, rows) == BAR_INVALID_GEOMETRY &&
          !memcmp(old, rows, sizeof(old)));
    g.softness[2] = 0;
    b[1].orientation[0] = 2;
    CHECK(ball_prepare_geometry(b, &g, rows) == BAR_INVALID_BODY &&
          !memcmp(old, rows, sizeof(old)));
    puts("PASS Ball geometry: three axes, coincident anchors, lever arms, independent softness, "
         "last-row failure atomicity and invalid rotation. Local only.");
    return 0;
}
