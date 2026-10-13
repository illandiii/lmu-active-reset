#include "restore_thermal_plan.h"
#include <string.h>
static int add(RestorePlan *p, const RestoreBackend *b, uint32_t slot, uint32_t offset,
               const void *data, uint32_t n) {
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
int restore_thermal_plan(const ThermalHistoryBank *saved, uint64_t sequence,
                         const RestoreBackend *b, RestorePlan *work, RestorePlan *out) {
    if (!saved || !sequence || saved->prior.sequence != sequence || !b || !b->context ||
        !b->allowed || !b->read || !work || !out || work == out || !work->count ||
        work->count > RESTORE_MAX_FIELDS - 9216 || !thermal_grid_valid(&saved->prior.grid) ||
        !tread_history_valid(&saved->history))
        return RESTORE_BAD_PLAN;
    if (!b->context(b->user, work->epoch))
        return RESTORE_CONTEXT_CHANGED;
    /* Mechanical plan must not already contain thermal fields. */
    for (size_t i = 0; i < work->count; i++)
        if (work->fields[i].object_slot >= RESTORE_THERMAL_BASE)
            return RESTORE_BAD_PLAN;
    for (uint32_t w = 0; w < 4; w++)
        for (uint32_t row = 0; row < 72; row++)
            for (uint32_t k = 0; k < 9; k++) {
                const ThermalCell *cell = &saved->prior.grid.tires[w].cells[row][k];
                uint32_t slot = RESTORE_THERMAL_BASE + (w * 72 + row) * 9 + k;
                int status = add(work, b, slot, 0, cell->layers, 40);
                if (status)
                    return status;
                status = add(work, b, slot, 0x40, cell->coefficients, 32);
                if (status)
                    return status;
                if (k < 7) {
                    const TreadHistory *h = &saved->history.nodes[w][row][k];
                    status = add(work, b, slot, 0x28, h->fields_28, 16);
                    if (status)
                        return status;
                    status = add(work, b, slot, 0x100, &h->field_100, 8);
                    if (status)
                        return status;
                }
            }
    if (!b->context(b->user, work->epoch))
        return RESTORE_CONTEXT_CHANGED;
    *out = *work;
    return 0;
}
