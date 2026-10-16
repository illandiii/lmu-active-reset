#include "../../src/native/physics/mechanical_constraint_core.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            printf("FAIL integrated preparation line %d\n", __LINE__);                             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static MechanicalConstraintPrepared out, prior;
int main(void) {
    RigidSavedBody saved[17] = {0};
    RigidMassProperties current[17];
    MechanicalDependencies a = {0}, b;
    MechanicalLinks links = {0, {1, 2, 3, 4}, {5, 6, 7, 8}};
    ConstraintRecipe recipes[31] = {0};
    a.fuel = (FuelSnapshot){13.6, -.1, .735, .4, 0};
    for (int i = 0; i < 17; i++) {
        saved[i].mass = (RigidMassProperties){1, {1, 1, 1}};
        saved[i].position_world[0] = i;
        for (int j = 0; j < 3; j++)
            saved[i].orientation[j * 4] = 1;
        current[i] = saved[i].mass;
    }
    for (int w = 0; w < 4; w++) {
        a.wheels[w] = (WheelInertiaInputs){.4, .001, .036, 3.618};
        for (int j = 0; j < 3; j++)
            a.tires[w].total_table[j] = 1.f;
    }
    b = a;
    b.fuel.quantity = 8;
    for (int w = 0; w < 4; w++) {
        b.wheels[w].term_5d8 = .035;
        b.tires[w].total_table[0] = 1.25f;
    }
    WheelPreparedInertia wa[4], wb[4];
    CHECK(!wheel_prepare_inertia(a.wheels, wa) && !wheel_prepare_inertia(b.wheels, wb));
    for (int w = 0; w < 4; w++) {
        saved[1 + w].mass.inverse_inertia_local[0] = wa[w].inverse_axial;
        current[1 + w].inverse_inertia_local[0] = wb[w].inverse_axial;
        current[5 + w].inverse_inertia_local[0] = .8;
    }
    saved[0].mass.inverse_mass = 1 / (13.6 * .735 + .4);
    current[0].inverse_mass = 1 / (8 * .735 + .4);
    for (size_t i = 0; i < 31; i++) {
        ConstraintRecipe *r = recipes + i;
        r->identity = i;
        r->body_indices[0] = i % 17;
        r->body_indices[1] = (i + 1) % 17;
        r->kind = i < 20   ? CONSTRAINT_BAR
                  : i < 26 ? CONSTRAINT_BALL
                  : i < 30 ? CONSTRAINT_HINGE
                           : CONSTRAINT_ORIENTATION;
        if (r->kind >= CONSTRAINT_HINGE)
            for (int j = 0; j < 3; j++)
                r->geometry.angular.axes_local[j][j] = 1;
    }
    CHECK(!mechanical_constraint_prepare(saved, current, 17, &links, &a, &b, recipes, 31, &out));
    CHECK(out.body_count == 17 && out.constraint_count == 31 && out.row_count == 49);
    CHECK(out.bodies[4].state.mass.inverse_inertia_local[0] == wa[3].inverse_axial &&
          out.bodies[8].state.mass.inverse_inertia_local[0] == 1);
    CHECK(out.constraints[0].rows[0].inverse_effective_mass ==
          1.125 / (saved[0].mass.inverse_mass + saved[1].mass.inverse_mass));
    memcpy(&prior, &out, sizeof(out));
    recipes[30].geometry.angular.softness[2] = NAN;
    CHECK(mechanical_constraint_prepare(saved, current, 17, &links, &a, &b, recipes, 31, &out) ==
              MECHANICAL_CONSTRAINT_GEOMETRY_FAILURE &&
          !memcmp(&prior, &out, sizeof(out)));
    recipes[30].geometry.angular.softness[2] = 0;
    recipes[30].body_indices[1] = 17;
    CHECK(mechanical_constraint_prepare(saved, current, 17, &links, &a, &b, recipes, 31, &out) ==
              MECHANICAL_CONSTRAINT_GEOMETRY_FAILURE &&
          !memcmp(&prior, &out, sizeof(out)));
    recipes[30].body_indices[1] = 14;
    recipes[30].identity = 0;
    CHECK(mechanical_constraint_prepare(saved, current, 17, &links, &a, &b, recipes, 31, &out) ==
              MECHANICAL_CONSTRAINT_GEOMETRY_FAILURE &&
          !memcmp(&prior, &out, sizeof(out)));
    recipes[30].identity = 30;
    recipes[30].kind = 99;
    CHECK(mechanical_constraint_prepare(saved, current, 17, &links, &a, &b, recipes, 31, &out) ==
              MECHANICAL_CONSTRAINT_GEOMETRY_FAILURE &&
          !memcmp(&prior, &out, sizeof(out)));
    recipes[30].kind = CONSTRAINT_ORIENTATION;
    saved[16].orientation[8] = 2;
    CHECK(mechanical_constraint_prepare(saved, current, 17, &links, &a, &b, recipes, 31, &out) ==
              MECHANICAL_CONSTRAINT_BODY_FAILURE &&
          !memcmp(&prior, &out, sizeof(out)));
    puts("PASS integrated owned preparation: 17 bodies, 31 constraints, 49 rows; saved masses feed "
         "geometry; last-row/body, bad link, duplicate and unknown type failures leave whole "
         "output unchanged. No live restore.");
    return 0;
}
