#include "constraint_group_core.h"
__declspec(dllexport) int fixture_constraint_group(const RigidSavedBody *b, size_t nb,
                                                   const ConstraintRecipe *r, size_t nr,
                                                   ConstraintPrepared *out) {
    return constraint_prepare_group(b, nb, r, nr, out);
}
