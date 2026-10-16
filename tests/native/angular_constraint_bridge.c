#include "../../src/native/physics/angular_constraint_core.h"
__declspec(dllexport) int fixture_angular(const RigidSavedBody *b, const AngularGeometryInput *in,
                                          size_t count, BarGeometryPrepared *out) {
    return angular_prepare_geometry(b, in, count, out);
}
