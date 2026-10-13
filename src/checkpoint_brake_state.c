#include "checkpoint_brake_state.h"
#include <math.h>
#include <string.h>
int brake_state_valid(const BrakeState s[4]) {
    if (!s)
        return 0;
    for (int w = 0; w < 4; w++) {
        if (!isfinite(s[w].cache_138) || s[w].cache_138 <= 0 || !isfinite(s[w].field_630))
            return 0;
        for (int i = 0; i < 5; i++)
            if (!isfinite(s[w].fields_5d8[i]))
                return 0;
        if (s[w].fields_5d8[0] < 0 || s[w].fields_5d8[1] < 0)
            return 0;
    }
    return 1;
}
int brake_state_save(BrakeBank *b, uint64_t sequence, const BrakeState s[4]) {
    if (!b || !sequence || !brake_state_valid(s))
        return 0;
    memcpy(b->wheels, s, sizeof(b->wheels));
    b->sequence = sequence;
    return 1;
}
int brake_state_prepare(const BrakeBank *b, uint64_t sequence, const BrakeState current[4],
                        BrakeState out[4]) {
    if (!b || !out || !sequence || b->sequence != sequence || !brake_state_valid(b->wheels) ||
        !brake_state_valid(current))
        return 0;
    memcpy(out, b->wheels, sizeof(b->wheels));
    return 1;
}
static int add(RestorePlan *p, const RestoreBackend *b, uint32_t slot, uint32_t offset,
               const void *data, uint32_t n) {
    for (size_t i = 0; i < p->count; i++) {
        const RestoreField *f = p->fields + i;
        if (f->object_slot == slot && offset < f->field_offset + f->size &&
            f->field_offset < offset + n)
            return RESTORE_BAD_PLAN;
    }
    if (p->count >= RESTORE_MAX_FIELDS || !b->allowed(b->user, slot, offset, n))
        return RESTORE_FIELD_FORBIDDEN;
    RestoreField *f = p->fields + p->count;
    memset(f, 0, sizeof(*f));
    f->object_slot = slot;
    f->field_offset = offset;
    f->size = n;
    if (!b->read(b->user, slot, offset, f->expected, n))
        return RESTORE_READ_FAILED;
    memcpy(f->desired, data, n);
    ++p->count;
    return 0;
}
int brake_state_plan(const BrakeBank *saved, uint64_t sequence, const RestoreBackend *b,
                     RestorePlan *work, RestorePlan *out) {
    if (!saved || !sequence || saved->sequence != sequence || !b || !b->context || !b->allowed ||
        !b->read || !work || !out || work == out || !work->count ||
        work->count > RESTORE_MAX_FIELDS - 12 || !brake_state_valid(saved->wheels))
        return RESTORE_BAD_PLAN;
    if (!b->context(b->user, work->epoch))
        return RESTORE_CONTEXT_CHANGED;
    for (uint32_t w = 0; w < 4; w++) {
        const BrakeState *state = saved->wheels + w;
        uint32_t slot = RESTORE_WHEEL_BASE + w;
        /* 5D8 is already restored by the mechanical dependency plan. */
        size_t matches = 0;
        for (size_t i = 0; i < work->count; i++) {
            const RestoreField *f = work->fields + i;
            if (f->object_slot == slot && f->field_offset == 0x5d8) {
                if (f->size != 8 || memcmp(f->desired, state->fields_5d8, 8))
                    return RESTORE_BAD_PLAN;
                ++matches;
            }
        }
        if (matches != 1)
            return RESTORE_BAD_PLAN;
        int status = add(work, b, slot, 0x5e0, state->fields_5d8 + 1, 32);
        if (status)
            return status;
        status = add(work, b, slot, 0x630, &state->field_630, 8);
        if (status)
            return status;
        status = add(work, b, slot, 0x138, &state->cache_138, 8);
        if (status)
            return status;
    }
    if (!b->context(b->user, work->epoch))
        return RESTORE_CONTEXT_CHANGED;
    *out = *work;
    return 0;
}
