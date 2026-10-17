#include "../../src/native/restore/physical_scoring_transaction.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
static RestorePlan physical, joint;
static unsigned char car[64], score[0x27400], prior_car[64], prior_score[0x27400];
static int writes, fail_at, lose_context_at, context_ok = 1;
static int context(void *user, uint64_t epoch) {
    (void)user;
    return context_ok && epoch == 77;
}
static int allowed(void *user, uint32_t slot, uint32_t off, uint32_t n) {
    return user == car ? (slot == 0 && off == 8 && n == 8)
                       : scoring_field_allowed_25661166(slot, off, n);
}
static int read_field(void *user, uint32_t slot, uint32_t off, void *out, uint32_t n) {
    if (!allowed(user, slot, off, n))
        return 0;
    memcpy(out, (unsigned char *)user + off, n);
    return 1;
}
static int write_field(void *user, uint32_t slot, uint32_t off, const void *in, uint32_t n) {
    if (!allowed(user, slot, off, n))
        return 0;
    ++writes;
    memcpy((unsigned char *)user + off, in, writes == fail_at ? n / 2 : n);
    if (writes == lose_context_at)
        context_ok = 0;
    return writes != fail_at;
}
static void init(void) {
    memset(car, 0x5a, sizeof(car));
    memset(score, 0x7b, sizeof(score));
    int32_t valid = 2;
    memcpy(score + 0x160, &valid, 4);
    memcpy(score + 0x27290, &valid, 4);
    memcpy(score + 0x27360, &valid, 4);
    memcpy(prior_car, car, sizeof(car));
    memcpy(prior_score, score, sizeof(score));
    memset(&physical, 0, sizeof(physical));
    physical.epoch = 77;
    physical.count = 1;
    RestoreField *f = physical.fields;
    f->object_slot = 0;
    f->field_offset = 8;
    f->size = 8;
    memcpy(f->expected, car + 8, 8);
    double value = 123.5;
    memcpy(f->desired, &value, 8);
    LapGuardFlags flags = {2, 2, 2, 1, 0};
    assert(restore_joint_prepare(&physical, &flags, &joint) == 0);
    writes = fail_at = lose_context_at = 0;
    context_ok = 1;
}
int main(void) {
    RestoreBackend p = {car, context, allowed, read_field, write_field},
                   s = {score, context, allowed, read_field, write_field};
    RestoreReport report;
    init();
    assert(restore_joint_execute(&joint, &p, &s, &report) == 0 && writes == 4);
    memcpy(prior_car + 8, joint.fields[0].desired, 8);
    const unsigned offsets[3] = {0x160, 0x27290, 0x27360};
    for (unsigned i = 0; i < 3; i++)
        memset(prior_score + offsets[i], 0, 4);
    assert(!memcmp(car, prior_car, sizeof(car)) && !memcmp(score, prior_score, sizeof(score)));
    for (int failure = 1; failure <= 4; failure++) {
        init();
        fail_at = failure;
        assert(restore_joint_execute(&joint, &p, &s, &report) == RESTORE_WRITE_FAILED);
        assert(report.restored == report.attempted && !memcmp(car, prior_car, sizeof(car)) &&
               !memcmp(score, prior_score, sizeof(score)));
        init();
        lose_context_at = failure;
        assert(restore_joint_execute(&joint, &p, &s, &report) == RESTORE_CONTEXT_CHANGED);
        assert(report.restored == report.attempted && !memcmp(car, prior_car, sizeof(car)) &&
               !memcmp(score, prior_score, sizeof(score)));
    }
    init();
    score[0x160] = 1;
    assert(restore_joint_execute(&joint, &p, &s, &report) == RESTORE_STATE_CHANGED && writes == 0);
    init();
    joint.fields[3].field_offset = 0x2735c;
    assert(restore_joint_execute(&joint, &p, &s, &report) == RESTORE_BAD_PLAN && writes == 0);
    init();
    joint.fields[3].desired[0] = 1;
    assert(restore_joint_execute(&joint, &p, &s, &report) == RESTORE_BAD_PLAN && writes == 0);
    init();
    joint.count = 3;
    assert(restore_joint_execute(&joint, &p, &s, &report) == RESTORE_BAD_PLAN && writes == 0);
    puts("PASS joint physics/scoring: exact fields and untouched record bytes; four partial-write "
         "and four context-loss rollbacks; stale flags, old offset, nonzero score and incomplete "
         "plan rejected.");
    return 0;
}
