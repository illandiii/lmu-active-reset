#include "physical_scoring_transaction.h"
#include <string.h>
typedef struct JointBackend {
    RestoreBackend physics, scoring;
} JointBackend;
static const RestoreBackend *route(JointBackend *b, uint32_t slot) {
    return slot == RESTORE_SCORING_SLOT ? &b->scoring : &b->physics;
}
static int context(void *user, uint64_t epoch) {
    JointBackend *b = user;
    return b->physics.context(b->physics.user, epoch) && b->scoring.context(b->scoring.user, epoch);
}
static int allowed(void *user, uint32_t slot, uint32_t off, uint32_t n) {
    JointBackend *b = user;
    if (slot > RESTORE_SCORING_SLOT)
        return 0;
    const RestoreBackend *target = route(b, slot);
    if (slot == RESTORE_SCORING_SLOT && !scoring_field_allowed_25661166(0, off, n))
        return 0;
    return target->allowed(target->user, slot == RESTORE_SCORING_SLOT ? 0 : slot, off, n);
}
static int read_field(void *user, uint32_t slot, uint32_t off, void *value, uint32_t n) {
    const RestoreBackend *target = route(user, slot);
    return target->read(target->user, slot == RESTORE_SCORING_SLOT ? 0 : slot, off, value, n);
}
static int write_field(void *user, uint32_t slot, uint32_t off, const void *value, uint32_t n) {
    const RestoreBackend *target = route(user, slot);
    return target->write(target->user, slot == RESTORE_SCORING_SLOT ? 0 : slot, off, value, n);
}
int restore_joint_prepare(const RestorePlan *physical, const LapGuardFlags *flags,
                          RestorePlan *out) {
    LapGuardFlags invalid;
    if (!physical || !out || physical == out || !physical->epoch || !physical->count ||
        physical->count > RESTORE_MAX_FIELDS - 3 || !lap_guard_prepare(flags, &invalid))
        return RESTORE_BAD_PLAN;
    for (size_t i = 0; i < physical->count; i++)
        if (physical->fields[i].object_slot >= RESTORE_SCORING_SLOT ||
            physical->fields[i].reserved || !physical->fields[i].size ||
            physical->fields[i].size > RESTORE_FIELD_BYTES)
            return RESTORE_BAD_PLAN;
    const uint32_t offsets[3] = {0x160, 0x27290, 0x27360};
    const int32_t before[3] = {flags->current, flags->remembered, flags->last_computed};
    out->epoch = physical->epoch;
    out->count = physical->count;
    memcpy(out->fields, physical->fields, physical->count * sizeof(RestoreField));
    for (unsigned i = 0; i < 3; i++) {
        RestoreField *field = out->fields + out->count++;
        memset(field, 0, sizeof(*field));
        field->object_slot = RESTORE_SCORING_SLOT;
        field->field_offset = offsets[i];
        field->size = 4;
        memcpy(field->expected, before + i, 4);
    }
    return RESTORE_OK;
}
static int complete(const RestoreBackend *b) {
    return b && b->context && b->allowed && b->read && b->write;
}
int restore_joint_execute(const RestorePlan *plan, const RestoreBackend *physical,
                          const RestoreBackend *scoring, RestoreReport *report) {
    if (report)
        *report = (RestoreReport){0, 0, RESTORE_BAD_PLAN};
    if (!complete(physical) || !complete(scoring) || !plan || !report || plan->count < 4 ||
        plan->count > RESTORE_MAX_FIELDS)
        return RESTORE_BAD_PLAN;
    unsigned mask = 0;
    for (size_t i = 0; i < plan->count; i++)
        if (plan->fields[i].object_slot == RESTORE_SCORING_SLOT) {
            const RestoreField *f = plan->fields + i;
            int32_t expected, desired;
            if (!scoring_field_allowed_25661166(0, f->field_offset, f->size))
                return RESTORE_BAD_PLAN;
            const int failure = f->field_offset == 0xd4;
            unsigned bit = failure                      ? 8
                           : f->field_offset == 0x160   ? 1
                           : f->field_offset == 0x27290 ? 2
                                                        : 4;
            memcpy(&expected, f->expected, 4);
            memcpy(&desired, f->desired, 4);
            if (mask & bit || expected < 0 || expected > (failure ? 9 : 2) || desired)
                return RESTORE_BAD_PLAN;
            mask |= bit;
        }
    if (mask != 7 && mask != 15)
        return RESTORE_BAD_PLAN;
    JointBackend frozen = {*physical, *scoring};
    RestoreBackend backend = {&frozen, context, allowed, read_field, write_field};
    return restore_transaction(plan, &backend, report);
}
