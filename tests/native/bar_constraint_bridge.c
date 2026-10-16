#include "../../src/native/physics/bar_constraint_core.h"
__declspec(dllexport) int fixture_bar(const RigidSavedBody *b, const BarGeometryInput *in,
                                      BarGeometryPrepared *out) {
    return bar_prepare_geometry(b, in, out);
}
__declspec(dllexport) int fixture_bar_warm(const BarGeometryPrepared *g, const BarWarmState *before,
                                           double lo, double hi, BarWarmState *out) {
    return bar_prepare_warm_start(g, before, lo, hi, out);
}
