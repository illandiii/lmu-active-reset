#ifndef LMU_BALL_CONSTRAINT_CORE_H
#define LMU_BALL_CONSTRAINT_CORE_H
#include "bar_constraint_core.h"
typedef struct BallGeometryInput {
    double anchors_local[2][3], softness[3];
} BallGeometryInput;
/* Three fixed world-axis rows, native BallJoint A0 3E39D0, build 14200.
 * Geometry only; the limit/bias stage and persistent history are separate. */
int ball_prepare_geometry(const RigidSavedBody bodies[2], const BallGeometryInput *input,
                          BarGeometryPrepared output[3]);
#endif
