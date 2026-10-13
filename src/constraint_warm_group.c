#include "constraint_warm_group.h"
#include <math.h>
#include <string.h>
int constraint_warm_prepare(const ConstraintVelocity *before, size_t nb,
                            const ConstraintPrepared *geometry, const ConstraintMemory *memory,
                            size_t count, ConstraintWarmPrepared *output) {
    if (!before || !geometry || !memory || !output || !nb || nb > RIGID_RESTORE_MAX_BODIES ||
        !count || count > CONSTRAINT_GROUP_MAX)
        return CONSTRAINT_WARM_BAD_ARGUMENT;
    ConstraintWarmPrepared prepared = {0};
    prepared.body_count = nb;
    prepared.constraint_count = count;
    for (size_t i = 0; i < nb; i++)
        for (int j = 0; j < 3; j++)
            if (!isfinite(before[i].linear[j]) || !isfinite(before[i].angular[j]))
                return CONSTRAINT_WARM_INVALID_VALUE;
    memcpy(prepared.bodies, before, nb * sizeof(*before));
    for (size_t i = 0; i < count; i++) {
        const ConstraintPrepared *g = geometry + i;
        const ConstraintMemory *m = memory + i;
        size_t rows = g->kind == CONSTRAINT_BAR                                           ? 1
                      : g->kind == CONSTRAINT_HINGE                                       ? 2
                      : (g->kind == CONSTRAINT_BALL || g->kind == CONSTRAINT_ORIENTATION) ? 3
                                                                                          : 0;
        if (!rows || g->row_count != rows || m->row_count != rows || m->identity != g->identity ||
            m->kind != g->kind || g->body_indices[0] >= nb || g->body_indices[1] >= nb ||
            g->body_indices[0] == g->body_indices[1] || m->body_indices[0] != g->body_indices[0] ||
            m->body_indices[1] != g->body_indices[1])
            return CONSTRAINT_WARM_TOPOLOGY_MISMATCH;
        for (size_t j = 0; j < i; j++)
            if (geometry[j].identity == g->identity)
                return CONSTRAINT_WARM_TOPOLOGY_MISMATCH;
        prepared.memory[i] = *m;
        for (size_t row = 0; row < rows; row++) {
            BarWarmState state = {0}, next;
            state.impulse = m->rows[row].impulse;
            for (int side = 0; side < 2; side++) {
                ConstraintVelocity *b = prepared.bodies + g->body_indices[side];
                memcpy(state.velocity[side], b->linear, sizeof(b->linear));
                memcpy(state.omega[side], b->angular, sizeof(b->angular));
            }
            if (bar_prepare_warm_start(g->rows + row, &state, m->rows[row].lower,
                                       m->rows[row].upper, &next))
                return CONSTRAINT_WARM_INVALID_VALUE;
            prepared.memory[i].rows[row].impulse = next.impulse;
            for (int side = 0; side < 2; side++) {
                ConstraintVelocity *b = prepared.bodies + g->body_indices[side];
                memcpy(b->linear, next.velocity[side], sizeof(b->linear));
                memcpy(b->angular, next.omega[side], sizeof(b->angular));
            }
        }
    }
    memcpy(output, &prepared, sizeof(prepared));
    return CONSTRAINT_WARM_OK;
}
