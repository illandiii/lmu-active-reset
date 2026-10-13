#ifndef LMU_ANGULAR_CONSTRAINT_CORE_H
#define LMU_ANGULAR_CONSTRAINT_CORE_H
#include "bar_constraint_core.h"
typedef struct AngularGeometryInput {
    double axes_local[3][3], softness[3];
} AngularGeometryInput;
/* OriJoint has three rows, PureHinge two. Local axes belong to body A.
 * This is the angular geometry stage only, excluding limits and bias. */
int angular_prepare_geometry(const RigidSavedBody bodies[2], const AngularGeometryInput *input,
                             size_t count, BarGeometryPrepared output[3]);
#endif
