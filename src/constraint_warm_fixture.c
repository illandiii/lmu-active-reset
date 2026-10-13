#include "constraint_warm_group.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            printf("FAIL warm group line %d\n", __LINE__);                                         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static ConstraintWarmPrepared out, prior;
int main(void) {
    ConstraintVelocity before[3] = {0};
    ConstraintPrepared g[2] = {0};
    ConstraintMemory m[2] = {0};
    for (int i = 0; i < 2; i++) {
        g[i].identity = m[i].identity = i;
        g[i].kind = m[i].kind = CONSTRAINT_BAR;
        g[i].row_count = m[i].row_count = 1;
        g[i].body_indices[0] = m[i].body_indices[0] = i;
        g[i].body_indices[1] = m[i].body_indices[1] = i + 1;
        g[i].rows[0].weighted[0][0] = 1;
        g[i].rows[0].weighted[2][0] = -1;
        m[i].rows[0] = (ConstraintRowMemory){i ? 3 : 2, -1, 1};
    }
    CHECK(!constraint_warm_prepare(before, 3, g, m, 2, &out));
    CHECK(out.bodies[0].linear[0] == 1 && out.bodies[1].linear[0] == 0 &&
          out.bodies[2].linear[0] == -1);
    CHECK(out.memory[0].rows[0].impulse == 1 && out.memory[1].rows[0].impulse == 1 &&
          m[0].rows[0].impulse == 2);
    memcpy(&prior, &out, sizeof(out));
    m[1].rows[0].upper = NAN;
    CHECK(constraint_warm_prepare(before, 3, g, m, 2, &out) == CONSTRAINT_WARM_INVALID_VALUE &&
          !memcmp(&out, &prior, sizeof(out)));
    m[1].rows[0].upper = 1;
    m[1].body_indices[1] = 0;
    CHECK(constraint_warm_prepare(before, 3, g, m, 2, &out) == CONSTRAINT_WARM_TOPOLOGY_MISMATCH &&
          !memcmp(&out, &prior, sizeof(out)));
    m[1].body_indices[1] = 2;
    g[1].rows[0].weighted[0][0] = INFINITY;
    CHECK(constraint_warm_prepare(before, 3, g, m, 2, &out) == CONSTRAINT_WARM_INVALID_VALUE &&
          !memcmp(&out, &prior, sizeof(out)));
    puts("PASS warm group: shared-body accumulation, clamped saved impulses, unchanged inputs and "
         "all-or-nothing final-row rejection. Local only.");
    return 0;
}
