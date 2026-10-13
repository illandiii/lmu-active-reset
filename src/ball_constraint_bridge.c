#include "ball_constraint_core.h"
__declspec(dllexport) int fixture_ball(const RigidSavedBody *b, const BallGeometryInput *in,
                                       BarGeometryPrepared *out) {
    return ball_prepare_geometry(b, in, out);
}
