#include "../../src/native/physics/fuel_restore_core.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            printf("FAIL fuel line %d\n", __LINE__);                                               \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    FuelSnapshot saved = {13.6, -.025, .735, .4, 0}, current = {8, -.02, .735, .4, 0};
    FuelPrepared f, unchanged;
    RigidSavedBody b[2] = {0};
    RigidMassProperties masses[2];
    RigidPreparedBody out[2], prior[2];
    for (int i = 0; i < 2; i++) {
        b[i].mass = (RigidMassProperties){1, {1, 1, 1}};
        b[i].orientation[0] = b[i].orientation[4] = b[i].orientation[8] = 1;
        masses[i] = b[i].mass;
    }
    b[1].mass.inverse_mass = 1 / (13.6 * .735 + .4);
    masses[1].inverse_mass = 1 / (8 * .735 + .4);
    CHECK(rigid_prepare_restore(b, masses, 2, out) == RIGID_PREPARE_MASS_CHANGED);
    CHECK(rigid_prepare_with_fuel(b, masses, 2, 1, &saved, &current, out, &f) == FUEL_OK);
    CHECK(fabs(f.body_mass - 10.396) < 1e-12 && f.saved.quantity == 13.6 &&
          f.saved.quantity_rate == -.025);
    CHECK(out[1].state.mass.inverse_mass == b[1].mass.inverse_mass);
    memset(out, 0xa5, sizeof(out));
    memcpy(prior, out, sizeof(out));
    memset(&f, 0xa5, sizeof(f));
    memcpy(&unchanged, &f, sizeof(f));
    // Battery/tire inertia or any other body's mass cannot be hidden by fuel handling.
    masses[0].inverse_mass = .5;
    CHECK(rigid_prepare_with_fuel(b, masses, 2, 1, &saved, &current, out, &f) ==
          FUEL_OTHER_BODY_CHANGED);
    CHECK(!memcmp(out, prior, sizeof(out)) && !memcmp(&f, &unchanged, sizeof(f)));
    masses[0].inverse_mass = 1;
    masses[1].inverse_inertia_local[2] = .9;
    CHECK(rigid_prepare_with_fuel(b, masses, 2, 1, &saved, &current, out, &f) ==
          FUEL_OTHER_BODY_CHANGED);
    CHECK(!memcmp(out, prior, sizeof(out)) && !memcmp(&f, &unchanged, sizeof(f)));
    masses[1].inverse_inertia_local[2] = 1;
    current.quantity = 7;
    CHECK(rigid_prepare_with_fuel(b, masses, 2, 1, &saved, &current, out, &f) ==
          FUEL_BODY_MISMATCH);
    current.quantity = 8;
    current.density = .8;
    CHECK(fuel_prepare_restore(&saved, &current, &f) == FUEL_CONFIGURATION_CHANGED);
    current.density = .735;
    saved.quantity = -1;
    CHECK(fuel_prepare_restore(&saved, &current, &f) == FUEL_INVALID_VALUE);
    saved.quantity = NAN;
    CHECK(fuel_prepare_restore(&saved, &current, &f) == FUEL_INVALID_VALUE);
    saved.quantity = 1e308;
    saved.density = 1e308;
    CHECK(fuel_prepare_restore(&saved, &current, &f) == FUEL_INVALID_VALUE);
    CHECK(!memcmp(&f, &unchanged, sizeof(f)));
    puts("PASS fuel: native mass example, fuel-only mass change, quantity rate, unrelated "
         "mass/inertia refused, wrong linkage/configuration/NaN/overflow refused, all-or-nothing "
         "outputs");
    return 0;
}
