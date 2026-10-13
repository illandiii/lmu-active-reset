#include "checkpoint_cached_mass.h"
#include <float.h>
#include <math.h>
#include <string.h>
static int normalize(const TirePersistentState t[4], const double cache[4],
                     const MechanicalLinks *links, const RigidMassProperties *masses, size_t n,
                     TirePersistentState normalized[4]) {
    if (!t || !cache || !links || !masses || n < 9 || n > 32)
        return CHECKPOINT_BAD_ARGUMENT;
    for (int w = 0; w < 4; w++) {
        size_t i = links->tires[w];
        if (i >= n)
            return CHECKPOINT_INVALID_STATE;
        if (!isfinite(cache[w]) || cache[w] <= 0 || !isfinite(t[w].total_mass) ||
            cache[w] > t[w].total_mass)
            return CHECKPOINT_INVALID_STATE;
        double expected = 1 / cache[w], actual = masses[i].inverse_mass;
        if (!isfinite(actual) || fabs(actual - expected) > 32 * DBL_EPSILON * fabs(expected))
            return CHECKPOINT_INVALID_STATE;
        for (int s = 0; s < 8; s++)
            if (!isfinite(t[w].sector_mass[s]) || t[w].sector_mass[s] < 0)
                return CHECKPOINT_INVALID_STATE;
        normalized[w] = t[w];
        memset(normalized[w].sector_mass, 0, sizeof(normalized[w].sector_mass));
        /* Equivalent partition for the legacy numerical validator only.
         * The checkpoint and prepared output retain the real sector state. */
        normalized[w].sector_mass[0] = t[w].total_mass - cache[w];
    }
    return 0;
}
int checkpoint_save_cached_mass(CachedMassBank *b, const PartialCheckpoint *s,
                                const MechanicalLinks *l, const MechanicalDependencies *d,
                                const TirePersistentState t[4], const PowertrainPartialState *p,
                                const TransmissionPartialState *tr, const double cache[4]) {
    if (!b || !s || s->body_count > 32)
        return CHECKPOINT_BAD_ARGUMENT;
    RigidMassProperties mass[32];
    for (size_t i = 0; i < s->body_count; i++)
        mass[i] = s->bodies[i].mass;
    TirePersistentState numerical[4];
    int status = normalize(t, cache, l, mass, s->body_count, numerical);
    if (status)
        return status;
    CachedMassBank replacement = {0};
    status = checkpoint_save_transmission(&replacement.state, s, l, d, numerical, p, tr);
    if (status)
        return status;
    memcpy(replacement.state.vehicle.mechanical.tires, t,
           sizeof(replacement.state.vehicle.mechanical.tires));
    memcpy(replacement.tire_mass_cache, cache, sizeof(replacement.tire_mass_cache));
    *b = replacement;
    return 0;
}
int checkpoint_prepare_cached_mass(const CachedMassBank *b, const CheckpointIdentity *id,
                                   const RigidMassProperties *m, size_t nb,
                                   const ConstraintRecipe *r, size_t nc,
                                   const MechanicalDependencies *d, const TirePersistentState t[4],
                                   const PowertrainPartialState *p,
                                   const TransmissionPartialState *tr, const double cache[4],
                                   CachedMassPreparation *out) {
    if (!b || !out)
        return CHECKPOINT_BAD_ARGUMENT;
    if (b->state.vehicle.mechanical.base.occupied != 1)
        return CHECKPOINT_EMPTY;
    TirePersistentState numerical[4];
    int status = normalize(t, cache, &b->state.vehicle.mechanical.base.links, m, nb, numerical);
    if (status)
        return status;
    CachedMassPreparation prepared = {0};
    status = checkpoint_prepare_transmission(&b->state, id, m, nb, r, nc, d, numerical, p, tr,
                                             &prepared.state);
    if (status)
        return status;
    memcpy(prepared.tire_mass_cache, b->tire_mass_cache, sizeof(prepared.tire_mass_cache));
    *out = prepared;
    return 0;
}
