#include "restore_transaction.h"
#include <string.h>
#include <limits.h>
#include <stdlib.h>
typedef struct FieldRange {
    uint32_t slot, start, end;
} FieldRange;
static int compare_field(const void *a, const void *b) {
    const FieldRange *x = a;
    const FieldRange *y = b;
    if (x->slot != y->slot)
        return x->slot < y->slot ? -1 : 1;
    return x->start < y->start ? -1 : x->start > y->start;
}
static int finish(RestoreReport *report, size_t attempted, size_t restored, int result) {
    *report = (RestoreReport){attempted, restored, result};
    return result;
}
static int rollback(const RestorePlan *plan, const RestoreBackend *b, size_t count,
                    RestoreReport *report, int cause) {
    unsigned char verify[RESTORE_FIELD_BYTES];
    size_t restored = 0;
    for (size_t k = count; k > 0; k--) {
        const RestoreField *f = plan->fields + k - 1;
        /* Do not follow stale/rebound object slots during rollback. A backend
         * must retain the original owner binding until the transaction ends. */
        if (b->allowed(b->user, f->object_slot, f->field_offset, f->size) &&
            b->write(b->user, f->object_slot, f->field_offset, f->expected, f->size) &&
            b->read(b->user, f->object_slot, f->field_offset, verify, f->size) &&
            !memcmp(verify, f->expected, f->size))
            ++restored;
    }
    return finish(report, count, restored, restored == count ? cause : RESTORE_ROLLBACK_FAILED);
}
int restore_transaction(const RestorePlan *p, const RestoreBackend *b, RestoreReport *report) {
    if (!report)
        return RESTORE_BAD_PLAN;
    if (!p || !b || !b->context || !b->allowed || !b->read || !b->write || !p->epoch || !p->count ||
        p->count > RESTORE_MAX_FIELDS)
        return finish(report, 0, 0, RESTORE_BAD_PLAN);
    if (!b->context(b->user, p->epoch))
        return finish(report, 0, 0, RESTORE_CONTEXT_CHANGED);
    unsigned char current[RESTORE_FIELD_BYTES];
    FieldRange ranges[RESTORE_MAX_FIELDS];
    for (size_t i = 0; i < p->count; i++) {
        const RestoreField *f = p->fields + i;
        if (f->reserved || !f->size || f->size > RESTORE_FIELD_BYTES ||
            f->field_offset > UINT32_MAX - f->size)
            return finish(report, 0, 0, RESTORE_BAD_PLAN);
        ranges[i] = (FieldRange){f->object_slot, f->field_offset, f->field_offset + f->size};
    }
    qsort(ranges, p->count, sizeof(*ranges), compare_field);
    for (size_t i = 1; i < p->count; i++)
        if (ranges[i].slot == ranges[i - 1].slot && ranges[i].start < ranges[i - 1].end)
            return finish(report, 0, 0, RESTORE_BAD_PLAN);
    for (size_t i = 0; i < p->count; i++) {
        const RestoreField *f = p->fields + i;
        if (!b->allowed(b->user, f->object_slot, f->field_offset, f->size))
            return finish(report, 0, 0, RESTORE_FIELD_FORBIDDEN);
        if (!b->read(b->user, f->object_slot, f->field_offset, current, f->size))
            return finish(report, 0, 0, RESTORE_READ_FAILED);
        if (memcmp(current, f->expected, f->size))
            return finish(report, 0, 0, RESTORE_STATE_CHANGED);
    }
    if (!b->context(b->user, p->epoch))
        return finish(report, 0, 0, RESTORE_CONTEXT_CHANGED);
    for (size_t i = 0; i < p->count; i++) {
        const RestoreField *f = p->fields + i;
        if (!b->context(b->user, p->epoch))
            return rollback(p, b, i, report, RESTORE_CONTEXT_CHANGED);
        if (!b->write(b->user, f->object_slot, f->field_offset, f->desired, f->size))
            return rollback(p, b, i + 1, report, RESTORE_WRITE_FAILED);
        if (!b->read(b->user, f->object_slot, f->field_offset, current, f->size) ||
            memcmp(current, f->desired, f->size))
            return rollback(p, b, i + 1, report, RESTORE_VERIFY_FAILED);
    }
    if (!b->context(b->user, p->epoch))
        return rollback(p, b, p->count, report, RESTORE_CONTEXT_CHANGED);
    return finish(report, p->count, 0, RESTORE_OK);
}
