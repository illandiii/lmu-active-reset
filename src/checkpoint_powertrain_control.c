#include "checkpoint_powertrain_control.h"
#include <math.h>
#include <string.h>
int powertrain_control_valid(const PowertrainControl *s) {
    if (!s || !s->table_token || s->active_e0 > 1 || s->latched_e1 > 1)
        return 0;
    for (unsigned i = 0; i < 6; i++)
        if (s->reserved[i])
            return 0;
    const double values[6] = {s->ramp_d0,   s->timer_e8,  s->factor_f0,
                              s->factor_f8, s->engine_50, s->engine_1b0};
    for (unsigned i = 0; i < 6; i++)
        if (!isfinite(values[i]))
            return 0;
    if (s->timer_e8 < 0)
        return 0;
    for (unsigned i = 0; i < 4; i++) {
        uint32_t index = i ? s->index_dc : s->index_d8, count = s->table_counts[i];
        if (count > 64 || (count ? index >= count : index != 0))
            return 0;
    }
    return 1;
}
int powertrain_control_save(PowertrainControlBank *b, uint64_t sequence,
                            const PowertrainControl *s) {
    if (!b || !sequence || !powertrain_control_valid(s))
        return 0;
    b->sequence = sequence;
    b->state = *s;
    return 1;
}
int powertrain_control_prepare(const PowertrainControlBank *b, uint64_t sequence,
                               const PowertrainControl *current, PowertrainControl *out) {
    if (!b || !out || !sequence || b->sequence != sequence ||
        !powertrain_control_valid(&b->state) || !powertrain_control_valid(current) ||
        b->state.table_token != current->table_token ||
        memcmp(b->state.table_counts, current->table_counts, sizeof(current->table_counts)))
        return 0;
    *out = b->state;
    return 1;
}
int powertrain_control_plan(const PowertrainControlBank *b, uint64_t sequence,
                            const RestoreBackend *backend, RestorePlan *work, RestorePlan *out) {
    if (!b || !sequence || b->sequence != sequence || !powertrain_control_valid(&b->state) ||
        !backend || !backend->context || !backend->allowed || !backend->read || !work || !out ||
        work == out || !work->count || work->count > RESTORE_MAX_FIELDS - 6)
        return RESTORE_BAD_PLAN;
    if (!backend->context(backend->user, work->epoch))
        return RESTORE_CONTEXT_CHANGED;
    const uint32_t slots[6] = {104, 104, 104, 104, 105, 105},
                   offsets[6] = {0x4810, 0x4818, 0x4820, 0x4828, 0x50, 0x1b0},
                   sizes[6] = {8, 8, 2, 24, 8, 8};
    const void *values[6] = {&b->state.ramp_d0,  &b->state.index_d8,  &b->state.active_e0,
                             &b->state.timer_e8, &b->state.engine_50, &b->state.engine_1b0};
    for (unsigned i = 0; i < 6; i++) {
        for (size_t j = 0; j < work->count; j++) {
            const RestoreField *f = work->fields + j;
            if (f->object_slot == slots[i] &&
                (uint64_t)offsets[i] < (uint64_t)f->field_offset + f->size &&
                f->field_offset < offsets[i] + sizes[i])
                return RESTORE_BAD_PLAN;
        }
        if (!backend->allowed(backend->user, slots[i], offsets[i], sizes[i]))
            return RESTORE_FIELD_FORBIDDEN;
        RestoreField *f = work->fields + work->count;
        memset(f, 0, sizeof(*f));
        f->object_slot = slots[i];
        f->field_offset = offsets[i];
        f->size = sizes[i];
        if (!backend->read(backend->user, slots[i], offsets[i], f->expected, sizes[i]))
            return RESTORE_READ_FAILED;
        memcpy(f->desired, values[i], sizes[i]);
        ++work->count;
    }
    if (!backend->context(backend->user, work->epoch))
        return RESTORE_CONTEXT_CHANGED;
    *out = *work;
    return RESTORE_OK;
}
