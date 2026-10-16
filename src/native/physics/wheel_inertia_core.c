#include "wheel_inertia_core.h"
#include <math.h>
#include <string.h>
int wheel_prepare_inertia(const WheelInertiaInputs saved[4], WheelPreparedInertia output[4]) {
    if (!saved || !output)
        return WHEEL_INERTIA_BAD_ARGUMENT;
    WheelPreparedInertia prepared[4];
    for (int i = 0; i < 4; i++) {
        const WheelInertiaInputs *s = saved + i;
        if (!isfinite(s->base_130) || !isfinite(s->term_4b8) || !isfinite(s->term_5d8) ||
            !isfinite(s->factor_4f0) || s->base_130 <= 0 || s->term_4b8 < 0 || s->term_5d8 < 0 ||
            s->factor_4f0 < 0)
            return WHEEL_INERTIA_INVALID_VALUE;
        double axial = ((s->term_4b8 + s->term_4b8) + s->term_5d8) * s->factor_4f0 + s->base_130;
        if (!isfinite(axial) || axial <= 0 || !isfinite(1 / axial) || 1 / axial <= 0)
            return WHEEL_INERTIA_INVALID_VALUE;
        prepared[i] = (WheelPreparedInertia){axial, 1 / axial};
    }
    memcpy(output, prepared, sizeof(prepared));
    return WHEEL_INERTIA_OK;
}
