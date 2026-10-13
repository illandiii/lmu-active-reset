#include "checkpoint_vehicle_partial.h"
#include <math.h>
#include <string.h>
static int valid(const PowertrainPartialState *p) {
    return p && p->engine_type_rva == 0x1793578 && isfinite(p->engine_1f8) &&
           isfinite(p->engine_250) && isfinite(p->engine_260) && isfinite(p->engine_280) &&
           isfinite(p->hdv_48a0) && isfinite(p->hdv_48a8) && isfinite(p->hdv_48b0) &&
           isfinite(p->hdv_48b8) && isfinite(p->hdv_48e8) && isfinite(p->hdv_48f0) &&
           isfinite(p->hdv_48f8);
}
int checkpoint_save_vehicle_partial(VehiclePartialBank *b, const PartialCheckpoint *s,
                                    const MechanicalLinks *l, const MechanicalDependencies *d,
                                    const TirePersistentState t[4],
                                    const PowertrainPartialState *p) {
    if (!b || !p)
        return CHECKPOINT_BAD_ARGUMENT;
    if (!valid(p))
        return CHECKPOINT_INVALID_STATE;
    VehiclePartialBank replacement = {0};
    int status = checkpoint_save_persistent(&replacement.mechanical, s, l, d, t);
    if (status)
        return status;
    replacement.powertrain = *p;
    *b = replacement;
    return 0;
}
int checkpoint_prepare_vehicle_partial(const VehiclePartialBank *b, const CheckpointIdentity *id,
                                       const RigidMassProperties *m, size_t nb,
                                       const ConstraintRecipe *r, size_t nc,
                                       const MechanicalDependencies *d,
                                       const TirePersistentState t[4],
                                       const PowertrainPartialState *p,
                                       VehiclePartialPreparation *out) {
    if (!b || !p || !out)
        return CHECKPOINT_BAD_ARGUMENT;
    if (!valid(p) || !valid(&b->powertrain))
        return CHECKPOINT_INVALID_STATE;
    VehiclePartialPreparation result = {0};
    int status =
        checkpoint_prepare_persistent(&b->mechanical, id, m, nb, r, nc, d, t, &result.mechanical);
    if (status)
        return status;
    result.dependencies = b->mechanical.original_dependencies;
    memcpy(result.tires, b->mechanical.tires, sizeof(result.tires));
    result.powertrain = b->powertrain;
    *out = result;
    return 0;
}
