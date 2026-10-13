#include "checkpoint_persistent_bank.h"
#include <string.h>
int checkpoint_save_persistent(PersistentCheckpointBank *b, const PartialCheckpoint *s,
                               const MechanicalLinks *l, const MechanicalDependencies *d,
                               const TirePersistentState t[4]) {
    if (!b || !s || !l || !d || !t || s->body_count > 32)
        return CHECKPOINT_BAD_ARGUMENT;
    RigidMassProperties mass[32];
    for (size_t i = 0; i < s->body_count; i++)
        mass[i] = s->bodies[i].mass;
    MechanicalDependencies normalized;
    if (tire_persistent_validate(t, d, mass, s->body_count, l, &normalized))
        return CHECKPOINT_INVALID_STATE;
    PersistentCheckpointBank replacement = {0};
    int status = checkpoint_save_mechanical(&replacement.base, s, l, &normalized);
    if (status)
        return status;
    replacement.original_dependencies = *d;
    memcpy(replacement.tires, t, sizeof(replacement.tires));
    *b = replacement;
    return 0;
}
int checkpoint_prepare_persistent(const PersistentCheckpointBank *b,
                                  const CheckpointIdentity *identity,
                                  const RigidMassProperties *mass, size_t nb,
                                  const ConstraintRecipe *topology, size_t nc,
                                  const MechanicalDependencies *d, const TirePersistentState t[4],
                                  CheckpointPreparation *out) {
    if (!b || !identity || !mass || !topology || !d || !t || !out || nb > 32)
        return CHECKPOINT_BAD_ARGUMENT;
    if (b->base.occupied != 1)
        return CHECKPOINT_EMPTY;
    MechanicalDependencies normalized;
    if (tire_persistent_validate(t, d, mass, nb, &b->base.links, &normalized))
        return CHECKPOINT_INVALID_STATE;
    RigidMassProperties proposed[32];
    for (size_t i = 0; i < nb; i++)
        proposed[i] = mass[i];
    for (int w = 0; w < 4; w++) {
        size_t i = b->base.links.tires[w];
        proposed[i].inverse_mass = b->base.saved.bodies[i].mass.inverse_mass;
    }
    return checkpoint_prepare_mechanical(&b->base, identity, proposed, nb, topology, nc,
                                         &normalized, out);
}
