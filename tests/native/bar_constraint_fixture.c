#include "../../src/native/physics/bar_constraint_core.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            printf("FAIL bar line %d\n", __LINE__);                                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    RigidSavedBody b[2] = {0};
    BarGeometryInput in = {0};
    BarGeometryPrepared out, prior;
    for (int i = 0; i < 2; i++) {
        b[i].mass = (RigidMassProperties){1, {1, 1, 1}};
        b[i].orientation[0] = b[i].orientation[4] = b[i].orientation[8] = 1;
    }
    b[0].position_world[0] = 2;
    CHECK(bar_prepare_geometry(b, &in, &out) == BAR_OK);
    CHECK(out.length == 2 && out.jacobian[0][0] == 1 && out.jacobian[2][0] == -1 &&
          out.inverse_effective_mass == .5625);
    in.anchors_local[0][1] = in.anchors_local[1][1] = 1;
    in.softness = .5;
    CHECK(bar_prepare_geometry(b, &in, &out) == BAR_OK);
    CHECK(out.jacobian[1][2] == -1 && out.jacobian[3][2] == 1 &&
          out.inverse_effective_mass == 1.125 / 4.5);
    // A rigid world rotation must rotate every Jacobian, leaving effective mass unchanged.
    b[0].position_world[0] = 0;
    b[0].position_world[1] = 2;
    for (int i = 0; i < 2; i++) {
        double r[9] = {0, -1, 0, 1, 0, 0, 0, 0, 1};
        memcpy(b[i].orientation, r, sizeof(r));
    }
    CHECK(bar_prepare_geometry(b, &in, &out) == BAR_OK);
    CHECK(out.jacobian[0][1] == 1 && out.jacobian[1][2] == -1 &&
          out.inverse_effective_mass == 1.125 / 4.5);
    BarWarmState warm = {0}, applied, unchanged;
    warm.impulse = 5;
    CHECK(bar_prepare_warm_start(&out, &warm, -2, 2, &applied) == BAR_OK);
    CHECK(applied.impulse == 2 && applied.velocity[0][1] == 2 && applied.velocity[1][1] == -2 &&
          applied.omega[0][2] == -2 && applied.omega[1][2] == 2);
    memcpy(&unchanged, &applied, sizeof(applied));
    CHECK(bar_prepare_warm_start(&out, &warm, 2, -2, &applied) == BAR_INVALID_GEOMETRY &&
          !memcmp(&unchanged, &applied, sizeof(applied)));
    warm.omega[1][2] = NAN;
    CHECK(bar_prepare_warm_start(&out, &warm, -2, 2, &applied) == BAR_INVALID_BODY &&
          !memcmp(&unchanged, &applied, sizeof(applied)));
    memset(&out, 0xa5, sizeof(out));
    memcpy(&prior, &out, sizeof(out));
    in.anchors_local[1][2] = NAN;
    CHECK(bar_prepare_geometry(b, &in, &out) == BAR_INVALID_GEOMETRY &&
          !memcmp(&out, &prior, sizeof(out)));
    in.anchors_local[1][2] = 0;
    b[0].position_world[1] = 0;
    CHECK(bar_prepare_geometry(b, &in, &out) == BAR_INVALID_GEOMETRY &&
          !memcmp(&out, &prior, sizeof(out)));
    b[1].mass.inverse_mass = -1;
    CHECK(bar_prepare_geometry(b, &in, &out) == BAR_INVALID_BODY &&
          !memcmp(&out, &prior, sizeof(out)));
    puts("PASS Bar geometry: analytical linear/offset anchors, opposing angular Jacobians, rigid "
         "rotation invariance, degenerate/NaN/invalid body atomic rejection. No impulse "
         "application.");
    return 0;
}
