#include "checkpoint_bank.h"
#include <math.h>
#include <string.h>
static int identity_valid(const CheckpointIdentity *i) {
    unsigned int nonzero = 0;
    for (size_t j = 0; j < 32; j++)
        nonzero |= i->executable_sha256[j];
    return i->process_epoch && i->session_epoch && i->vehicle_epoch && i->configuration_epoch &&
           nonzero;
}
static int identity_equal(const CheckpointIdentity *a, const CheckpointIdentity *b) {
    return a->process_epoch == b->process_epoch && a->session_epoch == b->session_epoch &&
           a->vehicle_epoch == b->vehicle_epoch &&
           a->configuration_epoch == b->configuration_epoch &&
           !memcmp(a->executable_sha256, b->executable_sha256, 32);
}
static int validate(const PartialCheckpoint *s, CheckpointPreparation *p) {
    if (!identity_valid(&s->identity) || !s->sequence || !isfinite(s->simulation_time) ||
        s->simulation_time < 0 || !s->body_count || s->body_count > RIGID_RESTORE_MAX_BODIES ||
        !s->constraint_count || s->constraint_count > CONSTRAINT_GROUP_MAX)
        return CHECKPOINT_INVALID_STATE;
    RigidMassProperties mass[32];
    for (size_t i = 0; i < s->body_count; i++)
        mass[i] = s->bodies[i].mass;
    if (rigid_prepare_restore(s->bodies, mass, s->body_count, p->bodies))
        return CHECKPOINT_INVALID_STATE;
    if (constraint_prepare_group(s->bodies, s->body_count, s->recipes, s->constraint_count,
                                 p->constraints))
        return CHECKPOINT_INVALID_STATE;
    for (size_t i = 0; i < s->constraint_count; i++) {
        const ConstraintPrepared *g = p->constraints + i;
        const ConstraintMemory *m = s->memory + i;
        if (m->identity != g->identity || m->kind != g->kind || m->row_count != g->row_count ||
            m->body_indices[0] != g->body_indices[0] || m->body_indices[1] != g->body_indices[1])
            return CHECKPOINT_INVALID_STATE;
        for (size_t j = 0; j < m->row_count; j++)
            if (!isfinite(m->rows[j].impulse) || !isfinite(m->rows[j].lower) ||
                !isfinite(m->rows[j].upper) || m->rows[j].lower > m->rows[j].upper)
                return CHECKPOINT_INVALID_STATE;
        p->memory[i] = *m;
    }
    p->sequence = s->sequence;
    p->body_count = s->body_count;
    p->constraint_count = s->constraint_count;
    p->full_restore_supported = 0;
    return CHECKPOINT_OK;
}
int checkpoint_save(CheckpointBank *bank, const PartialCheckpoint *capture) {
    if (!bank || !capture)
        return CHECKPOINT_BAD_ARGUMENT;
    CheckpointPreparation prepared = {0};
    int result = validate(capture, &prepared);
    if (result)
        return result;
    /* Prepare a replacement before touching the prior slot, including aliases. */
    PartialCheckpoint copy = *capture;
    bank->saved = copy;
    bank->occupied = 1;
    bank->has_mechanical_dependencies = 0;
    memset(&bank->links, 0, sizeof(bank->links));
    memset(&bank->dependencies, 0, sizeof(bank->dependencies));
    return CHECKPOINT_OK;
}
void checkpoint_clear(CheckpointBank *bank) {
    if (bank)
        memset(bank, 0, sizeof(*bank));
}
int checkpoint_prepare(const CheckpointBank *bank, const CheckpointIdentity *identity,
                       const RigidMassProperties *mass, size_t nb, const ConstraintRecipe *topology,
                       size_t nc, CheckpointPreparation *output) {
    if (!bank || !identity || !mass || !topology || !output)
        return CHECKPOINT_BAD_ARGUMENT;
    if (bank->occupied != 1)
        return CHECKPOINT_EMPTY;
    const PartialCheckpoint *saved = &bank->saved;
    if (!identity_valid(identity) || !identity_equal(&saved->identity, identity))
        return CHECKPOINT_IDENTITY_CHANGED;
    if (nb != saved->body_count || nc != saved->constraint_count || nb > RIGID_RESTORE_MAX_BODIES ||
        nc > CONSTRAINT_GROUP_MAX)
        return CHECKPOINT_TOPOLOGY_CHANGED;
    for (size_t i = 0; i < nc; i++)
        if (topology[i].identity != saved->recipes[i].identity ||
            topology[i].kind != saved->recipes[i].kind ||
            topology[i].body_indices[0] != saved->recipes[i].body_indices[0] ||
            topology[i].body_indices[1] != saved->recipes[i].body_indices[1])
            return CHECKPOINT_TOPOLOGY_CHANGED;
    CheckpointPreparation prepared = {0};
    int result = validate(saved, &prepared);
    if (result)
        return result;
    int rigid = rigid_prepare_restore(saved->bodies, mass, nb, prepared.bodies);
    if (rigid)
        return rigid == RIGID_PREPARE_MASS_CHANGED ? CHECKPOINT_MASS_CHANGED
                                                   : CHECKPOINT_INVALID_STATE;
    *output = prepared;
    return CHECKPOINT_OK;
}
int checkpoint_save_mechanical(CheckpointBank *bank, const PartialCheckpoint *capture,
                               const MechanicalLinks *links,
                               const MechanicalDependencies *dependencies) {
    if (!bank || !capture || !links || !dependencies)
        return CHECKPOINT_BAD_ARGUMENT;
    CheckpointBank replacement = {0};
    int status = checkpoint_save(&replacement, capture);
    if (status)
        return status;
    RigidMassProperties mass[32];
    RigidPreparedBody bodies[32];
    FuelPrepared fuel;
    for (size_t i = 0; i < capture->body_count; i++)
        mass[i] = capture->bodies[i].mass;
    if (mechanical_prepare(capture->bodies, mass, capture->body_count, links, dependencies,
                           dependencies, bodies, &fuel))
        return CHECKPOINT_INVALID_STATE;
    replacement.has_mechanical_dependencies = 1;
    replacement.links = *links;
    replacement.dependencies = *dependencies;
    *bank = replacement;
    return CHECKPOINT_OK;
}
int checkpoint_prepare_mechanical(const CheckpointBank *bank, const CheckpointIdentity *identity,
                                  const RigidMassProperties *mass, size_t nb,
                                  const ConstraintRecipe *topology, size_t nc,
                                  const MechanicalDependencies *current,
                                  CheckpointPreparation *output) {
    if (!current || !output || !bank)
        return CHECKPOINT_BAD_ARGUMENT;
    CheckpointPreparation prepared = {0};
    int status = checkpoint_prepare(bank, identity, mass, nb, topology, nc, &prepared);
    if (status != CHECKPOINT_OK && status != CHECKPOINT_MASS_CHANGED)
        return status;
    if (bank->has_mechanical_dependencies != 1)
        return CHECKPOINT_INVALID_STATE;
    if (status) {
        status = validate(&bank->saved, &prepared);
        if (status)
            return status;
    }
    FuelPrepared fuel;
    if (mechanical_prepare(bank->saved.bodies, mass, nb, &bank->links, &bank->dependencies, current,
                           prepared.bodies, &fuel))
        return CHECKPOINT_MASS_CHANGED;
    *output = prepared;
    return CHECKPOINT_OK;
}
