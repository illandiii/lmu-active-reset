#include "checkpoint_engine_integration.h"
#include <math.h>
static int valid(const EngineIntegration *e) {
    if (!e || e->channel_count > 2)
        return 0;
    for (int i = 0; i < 7; i++)
        if (!isfinite(e->outputs_208[i]))
            return 0;
    for (int i = 0; i < 3; i++)
        if (!isfinite(e->fields_240[i]))
            return 0;
    for (uint32_t i = 0; i < e->channel_count; i++)
        for (int j = 0; j < 2; j++)
            if (!isfinite(e->channel_state[i][j]))
                return 0;
    return 1;
}
int checkpoint_save_engine(EngineBank *b, const PartialCheckpoint *s, const MechanicalLinks *l,
                           const MechanicalDependencies *d, const TirePersistentState t[4],
                           const PowertrainPartialState *p, const TransmissionPartialState *tr,
                           const double c[4], const SolverHistory *h, const EngineIntegration *e) {
    if (!b || !e)
        return CHECKPOINT_BAD_ARGUMENT;
    if (!valid(e))
        return CHECKPOINT_INVALID_STATE;
    EngineBank replacement = {0};
    int status = checkpoint_save_history(&replacement.prior, s, l, d, t, p, tr, c, h);
    if (status)
        return status;
    replacement.integration = *e;
    *b = replacement;
    return 0;
}
int checkpoint_prepare_engine(const EngineBank *b, const CheckpointIdentity *id,
                              const RigidMassProperties *m, size_t nb, const ConstraintRecipe *r,
                              size_t nc, const MechanicalDependencies *d,
                              const TirePersistentState t[4], const PowertrainPartialState *p,
                              const TransmissionPartialState *tr, const double c[4],
                              const SolverHistory *h, const EngineIntegration *e,
                              EnginePreparation *out) {
    if (!b || !e || !out)
        return CHECKPOINT_BAD_ARGUMENT;
    if (!valid(e) || !valid(&b->integration))
        return CHECKPOINT_INVALID_STATE;
    if (e->channel_count != b->integration.channel_count)
        return CHECKPOINT_TOPOLOGY_CHANGED;
    EnginePreparation result = {0};
    int status =
        checkpoint_prepare_history(&b->prior, id, m, nb, r, nc, d, t, p, tr, c, h, &result.prior);
    if (status)
        return status;
    result.integration = b->integration;
    *out = result;
    return 0;
}
