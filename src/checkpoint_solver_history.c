#include "checkpoint_solver_history.h"
#include <math.h>
static int valid(const SolverHistory *h, size_t nb, const ConstraintRecipe *r, size_t nc) {
    if (!h || !r || nb > 32 || nc > 64 || !isfinite(h->driveline_timer_46c8))
        return 0;
    for (size_t i = 0; i < nb; i++) {
        if (h->bodies[i].reserved)
            return 0;
        for (int j = 0; j < 3; j++)
            if (!isfinite(h->bodies[i].force[j]) || !isfinite(h->bodies[i].torque[j]))
                return 0;
    }
    for (size_t i = 0; i < nc; i++) {
        if (r[i].kind < 1 || r[i].kind > 4)
            return 0;
        size_t rows = r[i].kind == 1 ? 1 : r[i].kind == 3 ? 2 : 3;
        for (size_t j = 0; j < rows; j++) {
            const SolverRowHistory *row = &h->rows[i][j];
            if (!isfinite(row->bias) || !isfinite(row->coefficient) || row->coefficient < 0)
                return 0;
            for (int k = 0; k < 12; k++)
                if (!isfinite(row->jacobian[k]) || !isfinite(row->weighted[k]))
                    return 0;
        }
    }
    return 1;
}
int checkpoint_save_history(HistoryBank *b, const PartialCheckpoint *s, const MechanicalLinks *l,
                            const MechanicalDependencies *d, const TirePersistentState t[4],
                            const PowertrainPartialState *p, const TransmissionPartialState *tr,
                            const double c[4], const SolverHistory *h) {
    if (!b || !s || !h)
        return CHECKPOINT_BAD_ARGUMENT;
    if (!valid(h, s->body_count, s->recipes, s->constraint_count))
        return CHECKPOINT_INVALID_STATE;
    HistoryBank replacement = {0};
    int status = checkpoint_save_cached_mass(&replacement.state, s, l, d, t, p, tr, c);
    if (status)
        return status;
    replacement.history = *h;
    *b = replacement;
    return 0;
}
int checkpoint_prepare_history(const HistoryBank *b, const CheckpointIdentity *id,
                               const RigidMassProperties *m, size_t nb, const ConstraintRecipe *r,
                               size_t nc, const MechanicalDependencies *d,
                               const TirePersistentState t[4], const PowertrainPartialState *p,
                               const TransmissionPartialState *tr, const double c[4],
                               const SolverHistory *h, HistoryPreparation *out) {
    if (!b || !h || !out)
        return CHECKPOINT_BAD_ARGUMENT;
    if (!valid(h, nb, r, nc))
        return CHECKPOINT_INVALID_STATE;
    const PartialCheckpoint *saved = &b->state.state.vehicle.mechanical.base.saved;
    if (!valid(&b->history, saved->body_count, saved->recipes, saved->constraint_count))
        return CHECKPOINT_INVALID_STATE;
    HistoryPreparation prepared = {0};
    int status = checkpoint_prepare_cached_mass(&b->state, id, m, nb, r, nc, d, t, p, tr, c,
                                                &prepared.state);
    if (status)
        return status;
    prepared.history = b->history;
    *out = prepared;
    return 0;
}
