#include "checkpoint_shift_transient.h"
#include <math.h>
#include <string.h>
static int gear(int32_t value, int32_t maximum) {
    return value >= -1 && value <= maximum;
}
int shift_transient_valid(const ShiftTransientState *s) {
    if (!s || s->gears[0] < 1 || s->gears[0] > 16)
        return 0;
    const double values[5] = {s->timer_4ac0, s->control_4668, s->clock_a8, s->event_50, s->dt_b0};
    for (unsigned i = 0; i < 5; i++)
        if (!isfinite(values[i]))
            return 0;
    if (s->dt_b0 <= 0 || s->clock_a8 < s->event_50)
        return 0;
    for (unsigned i = 1; i < 4; i++)
        if (!gear(s->gears[i], s->gears[0]))
            return 0;
    for (unsigned i = 4; i < 6; i++)
        if (s->gears[i] < -1 || s->gears[i] > 1)
            return 0;
    if (s->notification[0] < 0 || s->notification[0] > 2 ||
        !gear(s->notification[1], s->gears[0]) || !gear(s->request[0], s->gears[0]) ||
        s->request[1] < 0 || s->request[1] > 6)
        return 0;
    for (unsigned i = 0; i < 2; i++)
        if (!isfinite(s->accumulators[i]) || s->accumulators[i] < 0)
            return 0;
    return s->accumulators[0] <= 1;
}
int shift_transient_save(ShiftTransientBank *bank, uint64_t sequence,
                         const ShiftTransientState *state) {
    if (!bank || !sequence || !shift_transient_valid(state))
        return 0;
    bank->sequence = sequence;
    bank->state = *state;
    return 1;
}
int shift_transient_prepare(const ShiftTransientBank *bank, uint64_t sequence,
                            const ShiftTransientState *current, ShiftTransientState *out) {
    if (!bank || !out || !sequence || bank->sequence != sequence ||
        !shift_transient_valid(&bank->state) || !shift_transient_valid(current) ||
        current->gears[0] != bank->state.gears[0] || current->dt_b0 != bank->state.dt_b0 ||
        current->clock_a8 < bank->state.clock_a8)
        return 0;
    ShiftTransientState next = bank->state;
    double elapsed = bank->state.clock_a8 - bank->state.event_50;
    next.clock_a8 = current->clock_a8;
    next.event_50 = current->clock_a8 - elapsed;
    if (!isfinite(elapsed) || !shift_transient_valid(&next))
        return 0;
    /* Preserve elapsed-time semantics, refuse catastrophic cancellation. */
    double error = fabs((next.clock_a8 - next.event_50) - elapsed);
    if (error > 1e-9 * fmax(1.0, fabs(elapsed)))
        return 0;
    *out = next;
    return 1;
}

int shift_transient_plan(const ShiftTransientBank *bank, uint64_t sequence,
                         const ShiftTransientState *prepared, const RestoreBackend *backend,
                         RestorePlan *work, RestorePlan *out) {
    ShiftTransientState checked;
    if (!prepared || !shift_transient_prepare(bank, sequence, prepared, &checked) ||
        memcmp(&checked, prepared, sizeof(checked)) || !backend || !backend->context ||
        !backend->allowed || !backend->read || !work || !out || work == out || !work->count ||
        work->count > RESTORE_MAX_FIELDS - 8)
        return RESTORE_BAD_PLAN;
    int selected_found = 0;
    for (size_t j = 0; j < work->count; j++) {
        const RestoreField *f = work->fields + j;
        if (f->object_slot == 106 && f->field_offset == 0x64 && f->size == 4) {
            int32_t selected;
            memcpy(&selected, f->desired, 4);
            if (selected != prepared->gears[1])
                return RESTORE_BAD_PLAN;
            ++selected_found;
        }
    }
    if (selected_found != 1)
        return RESTORE_BAD_PLAN;
    if (!backend->context(backend->user, work->epoch))
        return RESTORE_CONTEXT_CHANGED;
    const uint32_t offsets[8] = {0x4ac0, 0x4668, 0x50, 0x4b30, 0x4bb8, 0x4ba8, 0x4ae8, 0x48},
                   sizes[8] = {8, 8, 8, 4, 4, 16, 8, 8};
    const void *values[8] = {&prepared->timer_4ac0,      &prepared->control_4668,
                             &prepared->event_50,        &prepared->accumulators[0],
                             &prepared->accumulators[1], prepared->gears + 2,
                             prepared->notification,     prepared->request};
    for (unsigned i = 0; i < 8; i++) {
        for (size_t j = 0; j < work->count; j++) {
            const RestoreField *f = work->fields + j;
            if (f->object_slot == 104 &&
                (uint64_t)offsets[i] < (uint64_t)f->field_offset + f->size &&
                f->field_offset < offsets[i] + sizes[i])
                return RESTORE_BAD_PLAN;
        }
        if (!backend->allowed(backend->user, 104, offsets[i], sizes[i]))
            return RESTORE_FIELD_FORBIDDEN;
        RestoreField *f = work->fields + work->count;
        memset(f, 0, sizeof(*f));
        f->object_slot = 104;
        f->field_offset = offsets[i];
        f->size = sizes[i];
        if (!backend->read(backend->user, 104, offsets[i], f->expected, sizes[i]))
            return RESTORE_READ_FAILED;
        memcpy(f->desired, values[i], sizes[i]);
        ++work->count;
    }
    if (!backend->context(backend->user, work->epoch))
        return RESTORE_CONTEXT_CHANGED;
    *out = *work;
    return RESTORE_OK;
}
