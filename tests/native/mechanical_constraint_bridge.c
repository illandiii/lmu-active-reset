#include "../../src/native/physics/mechanical_constraint_core.h"
__declspec(dllexport) int
fixture_mechanical_constraint(const RigidSavedBody *s, const RigidMassProperties *c, size_t n,
                              const MechanicalLinks *links, const MechanicalDependencies *a,
                              const MechanicalDependencies *b, const ConstraintRecipe *r, size_t nr,
                              MechanicalConstraintPrepared *out) {
    return mechanical_constraint_prepare(s, c, n, links, a, b, r, nr, out);
}
