#include "mechanical_prepare_core.h"
__declspec(dllexport) int fixture_mechanical(const RigidSavedBody *a, const RigidMassProperties *b,
                                             size_t n, const MechanicalLinks *links,
                                             const MechanicalDependencies *sa,
                                             const MechanicalDependencies *sb,
                                             RigidPreparedBody *out, FuelPrepared *fuel) {
    return mechanical_prepare(a, b, n, links, sa, sb, out, fuel);
}
