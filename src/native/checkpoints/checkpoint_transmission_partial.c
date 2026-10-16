#include "checkpoint_transmission_partial.h"
#include <math.h>
static int valid(const TransmissionPartialState *t) {
    return t && isfinite(t->shaft_0) && isfinite(t->shaft_8) && isfinite(t->input_18) &&
           isfinite(t->input_20) && isfinite(t->input_30) && isfinite(t->input_38) &&
           !t->reserved && t->gear_64 >= -1 && t->gear_64 <= 16;
}
int checkpoint_save_transmission(TransmissionBank *b, const PartialCheckpoint *s,
                                 const MechanicalLinks *l, const MechanicalDependencies *d,
                                 const TirePersistentState tires[4],
                                 const PowertrainPartialState *p,
                                 const TransmissionPartialState *t) {
    if (!b || !t)
        return CHECKPOINT_BAD_ARGUMENT;
    if (!valid(t))
        return CHECKPOINT_INVALID_STATE;
    TransmissionBank replacement = {0};
    int result = checkpoint_save_vehicle_partial(&replacement.vehicle, s, l, d, tires, p);
    if (result)
        return result;
    replacement.transmission = *t;
    *b = replacement;
    return 0;
}
int checkpoint_prepare_transmission(
    const TransmissionBank *b, const CheckpointIdentity *id, const RigidMassProperties *m,
    size_t nb, const ConstraintRecipe *r, size_t nc, const MechanicalDependencies *d,
    const TirePersistentState tires[4], const PowertrainPartialState *p,
    const TransmissionPartialState *t, TransmissionPreparation *out) {
    if (!b || !t || !out)
        return CHECKPOINT_BAD_ARGUMENT;
    if (!valid(t) || !valid(&b->transmission))
        return CHECKPOINT_INVALID_STATE;
    TransmissionPreparation result = {0};
    int status = checkpoint_prepare_vehicle_partial(&b->vehicle, id, m, nb, r, nc, d, tires, p,
                                                    &result.vehicle);
    if (status)
        return status;
    result.transmission = b->transmission;
    *out = result;
    return 0;
}
