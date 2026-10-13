#include "tire_inertia_core.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            printf("FAIL tire inertia line %d\n", __LINE__);                                       \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    TireInertiaPartition s[4] = {0};
    TirePreparedInertia out[4], prior[4];
    for (int w = 0; w < 4; w++)
        for (int j = 0; j < 3; j++) {
            s[w].total_table[j] = j ? 0.4877238869667053f : 0.8421462178230286f;
        }
    CHECK(tire_prepare_inertia(s, out) == TIRE_INERTIA_OK);
    CHECK(out[0].internal[0] == 0.8421462178230286 && out[0].sector_sum[2] == 0);
    CHECK(out[0].inverse_internal[0] == 1 / 0.8421462178230286);
    for (int w = 0; w < 4; w++)
        for (int j = 0; j < 3; j++) {
            s[w].total_table[j] = 16.f;
            for (int k = 0; k < 8; k++)
                s[w].sectors[k][j] = .125 * (k + 1);
        }
    CHECK(tire_prepare_inertia(s, out) == TIRE_INERTIA_OK);
    CHECK(out[3].sector_sum[2] == 4.5 && out[3].internal[2] == 11.5 &&
          out[3].inverse_internal[2] == 1 / 11.5);
    memset(out, 0xa5, sizeof(out));
    memcpy(prior, out, sizeof(out));
    double cases[] = {-1, NAN, INFINITY, DBL_MAX, 16};
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        s[3].sectors[7][2] = cases[i];
        CHECK(tire_prepare_inertia(s, out) == TIRE_INERTIA_INVALID_VALUE);
        CHECK(!memcmp(out, prior, sizeof(out)));
    }
    s[3].sectors[7][2] = 1;
    s[3].total_table[2] = NAN;
    CHECK(tire_prepare_inertia(s, out) == TIRE_INERTIA_INVALID_VALUE);
    CHECK(!memcmp(out, prior, sizeof(out)));
    CHECK(tire_prepare_inertia(NULL, out) == TIRE_INERTIA_BAD_ARGUMENT);
    CHECK(tire_prepare_inertia(s, NULL) == TIRE_INERTIA_BAD_ARGUMENT);
    puts("PASS tire inertia: native float example, eight unequal sectors, last-tire failure "
         "atomicity, negative/NaN/overflow/nonpositive residual refused. Local preparation only.");
    return 0;
}
