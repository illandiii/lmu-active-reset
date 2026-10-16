#include "restore_flow.h"
#include "restore_joint_25661166.h"
#include <string.h>
/* A host pin is still required. These checks prevent a known lost pin from
 * reaching either forward transfers or transaction-internal rollback. */
typedef struct PinnedMemory {
    RestoreBackend memory;
    RestoreFlowOps ops;
    uint64_t epoch;
    int lost;
} PinnedMemory;
static int pin_alive(PinnedMemory *p) {
    if (p->lost)
        return 0;
    if (!p->ops.owner_alive(p->ops.user, p->epoch)) {
        p->lost = 1;
        return 0;
    }
    return 1;
}
static int pinned_context(void *u, uint64_t epoch) {
    PinnedMemory *p = u;
    return epoch == p->epoch && pin_alive(p) && p->memory.context(p->memory.user, epoch);
}
static int pinned_allowed(void *u, uint32_t slot, uint32_t off, uint32_t n) {
    PinnedMemory *p = u;
    return pin_alive(p) && p->memory.allowed(p->memory.user, slot, off, n);
}
static int pinned_read(void *u, uint32_t slot, uint32_t off, void *value, uint32_t n) {
    PinnedMemory *p = u;
    return pin_alive(p) && p->memory.read(p->memory.user, slot, off, value, n);
}
static int pinned_write(void *u, uint32_t slot, uint32_t off, const void *value, uint32_t n) {
    PinnedMemory *p = u;
    return pin_alive(p) && p->memory.write(p->memory.user, slot, off, value, n);
}
static int restore_compensate_committed(const RestorePlan *p, const RestoreBackend *b,
                                        int (*owner)(void *, uint64_t), void *user,
                                        RestoreReport *r) {
    if (!r)
        return RESTORE_BAD_PLAN;
    *r = (RestoreReport){0, 0, RESTORE_BAD_PLAN};
    if (!p || !b || !owner || !b->allowed || !b->read || !b->write || !p->epoch || !p->count ||
        p->count > RESTORE_MAX_FIELDS)
        return r->result;
    if (!owner(user, p->epoch)) {
        r->result = RESTORE_CONTEXT_CHANGED;
        return r->result;
    }
    for (size_t i = 0; i < p->count; i++) {
        const RestoreField *f = p->fields + i;
        if (f->reserved || !f->size || f->size > RESTORE_FIELD_BYTES ||
            f->field_offset > UINT32_MAX - f->size ||
            !b->allowed(b->user, f->object_slot, f->field_offset, f->size))
            return r->result;
    }
    unsigned char check[RESTORE_FIELD_BYTES];
    for (size_t i = p->count; i > 0; i--) {
        const RestoreField *f = p->fields + i - 1;
        if (!owner(user, p->epoch))
            break;
        ++r->attempted;
        if (b->write(b->user, f->object_slot, f->field_offset, f->expected, f->size) &&
            b->read(b->user, f->object_slot, f->field_offset, check, f->size) &&
            !memcmp(check, f->expected, f->size))
            ++r->restored;
    }
    r->result = r->restored == p->count ? RESTORE_OK : RESTORE_ROLLBACK_FAILED;
    return r->result;
}
static int joint_valid(const RestorePlan *p) {
    if (!p || !p->epoch || p->count < 4 || p->count > RESTORE_MAX_FIELDS)
        return 0;
    unsigned mask = 0;
    for (size_t i = 0; i < p->count; i++) {
        const RestoreField *f = p->fields + i;
        if (f->object_slot > RESTORE_SCORING_SLOT || f->reserved || !f->size ||
            f->size > RESTORE_FIELD_BYTES)
            return 0;
        if (f->object_slot == RESTORE_SCORING_SLOT) {
            int32_t before, after;
            if (!scoring_field_allowed_25661166(0, f->field_offset, f->size))
                return 0;
            unsigned bit = f->field_offset == 0x160 ? 1 : f->field_offset == 0x27290 ? 2 : 4;
            memcpy(&before, f->expected, 4);
            memcpy(&after, f->desired, 4);
            if ((mask & bit) || before < 0 || before > 2 || after)
                return 0;
            mask |= bit;
        }
    }
    return mask == 7;
}
int restore_flow_execute(RestoreFlow *state, RestoreFlowWork *work, const RestorePlan *plan,
                         const RestoreBackend *backend, const RestoreFlowOps *ops,
                         RestoreFlowReport *r) {
    if (!r)
        return FLOW_BAD_ARGUMENT;
    memset(r, 0, sizeof(*r));
    r->result = FLOW_BAD_ARGUMENT;
    if (!state || !work || !backend || !backend->context || !backend->allowed || !backend->read ||
        !backend->write || !ops || !ops->enter || !ops->owner_alive || !ops->contacts ||
        !ops->leave || !ops->quarantine)
        return r->result;
    if (state->quarantined) {
        r->result = FLOW_QUARANTINED;
        return r->result;
    }
    if (state->busy) {
        r->result = FLOW_BUSY;
        return r->result;
    }
    if (!joint_valid(plan))
        return r->result;
    RestoreFlowOps frozen = *ops;
    PinnedMemory pin = {*backend, frozen, plan->epoch, 0};
    RestoreBackend memory = {&pin, pinned_context, pinned_allowed, pinned_read, pinned_write};
    work->frozen.epoch = plan->epoch;
    work->frozen.count = plan->count;
    memmove(work->frozen.fields, plan->fields, plan->count * sizeof(RestoreField));
    const RestorePlan *p = &work->frozen;
    state->busy = 1;
    if (!frozen.enter(frozen.user, p->epoch)) {
        state->busy = 0;
        r->result = FLOW_REJECTED;
        return r->result;
    }
    if (!pin_alive(&pin)) {
        r->cause = FLOW_OWNER_LOST;
        r->result = FLOW_QUARANTINED;
        goto finish;
    }
    r->cause = restore_transaction(p, &memory, &r->forward);
    if (r->cause) {
        r->result = r->cause == RESTORE_ROLLBACK_FAILED ? FLOW_QUARANTINED : FLOW_REJECTED;
        goto finish;
    }
    if (!memory.context(memory.user, p->epoch))
        r->cause = FLOW_CONTEXT_EXPIRED;
    else {
        ++r->contact_calls;
        if (!frozen.contacts(frozen.user, p->epoch, 0))
            r->cause = FLOW_CONTACT_FAILED;
    }
    if (!r->cause &&
        (!frozen.owner_alive(frozen.user, p->epoch) || !memory.context(memory.user, p->epoch)))
        r->cause = FLOW_CONTEXT_EXPIRED;
    if (!r->cause) {
        ++state->completed;
        r->result = FLOW_OK;
        goto finish;
    }
    if (restore_compensate_committed(p, &memory, frozen.owner_alive, frozen.user, &r->recovery)) {
        r->result = FLOW_QUARANTINED;
        goto finish;
    }
    ++r->contact_calls;
    if (!frozen.owner_alive(frozen.user, p->epoch) || !frozen.contacts(frozen.user, p->epoch, 1) ||
        !frozen.owner_alive(frozen.user, p->epoch)) {
        r->result = FLOW_QUARANTINED;
        goto finish;
    }
    ++state->compensated;
    r->result = FLOW_COMPENSATED;
finish:
    if (pin.lost) {
        r->cause = FLOW_OWNER_LOST;
        r->result = FLOW_QUARANTINED;
    }
    if (r->result == FLOW_QUARANTINED) {
        state->quarantined = 1;
        frozen.quarantine(frozen.user, r->cause);
    }
    frozen.leave(frozen.user, r->result != FLOW_QUARANTINED);
    state->busy = 0;
    return r->result;
}
