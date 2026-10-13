#ifndef LMU_BAR_CONSTRAINT_CORE_H
#define LMU_BAR_CONSTRAINT_CORE_H
#include "rigid_restore_core.h"
typedef struct BarGeometryInput {
    double anchors_local[2][3], softness;
} BarGeometryInput;
typedef struct BarGeometryPrepared {
    double length;
    double jacobian[4][3];         /* linear A, angular A, linear B, angular B */
    double weighted[4][3];         /* M^-1 J^T */
    double inverse_effective_mass; /* Native relaxed coefficient: 1.125 / diagonal, build 14200. */
} BarGeometryPrepared;
enum BarResult { BAR_OK, BAR_BAD_ARGUMENT, BAR_INVALID_BODY, BAR_INVALID_GEOMETRY };
/* Pure geometry part of native Bar::A0, 14200 3EF0E0. Does not prepare limits,
 * bias, callback selection, cached impulses, breakage or apply warm-start.
 * Zero length is deliberately refused until its native fallback is verified. */
int bar_prepare_geometry(const RigidSavedBody bodies[2], const BarGeometryInput *input,
                         BarGeometryPrepared *output);
typedef struct BarWarmState {
    double velocity[2][3], omega[2][3], impulse;
} BarWarmState;
/* Numerical clamp-and-apply stage only. Caller must supply the bounds produced
 * by the native limit/bias stage; this does not recreate that earlier stage. */
int bar_prepare_warm_start(const BarGeometryPrepared *geometry, const BarWarmState *before,
                           double lower, double upper, BarWarmState *output);
#endif
