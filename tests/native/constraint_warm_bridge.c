#include "../../src/native/physics/constraint_warm_group.h"
__declspec(dllexport) int fixture_warm_group(const ConstraintVelocity *before, size_t n,
                                             const ConstraintPrepared *g, const ConstraintMemory *m,
                                             size_t count, ConstraintWarmPrepared *out) {
    return constraint_warm_prepare(before, n, g, m, count, out);
}
