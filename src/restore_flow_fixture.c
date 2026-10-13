#include "restore_flow.h"
#include "restore_joint_25661166.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
static RestoreFlow flow;
static RestoreFlowWork workspace;
static RestorePlan plan;
static unsigned char body[128], score[0x27400], old_body[128], old_score[0x27400];
static int held, owner_ok, context_ok, entered, leaves, resume_value, quarantines, writes,
    fail_write, mode, contact_count;
static int reads, unsafe_accesses;
static double contact_position;
static int context(void *u, uint64_t e) {
    (void)u;
    return held && context_ok && e == 42;
}
static int owner(void *u, uint64_t e) {
    (void)u;
    return held && owner_ok && e == 42;
}
static int enter(void *u, uint64_t e) {
    (void)u;
    ++entered;
    if (mode == 6)
        return 0;
    held = e == 42;
    return held;
}
static void leave(void *u, int resume) {
    (void)u;
    assert(held);
    held = 0;
    ++leaves;
    resume_value = resume;
}
static void quarantine(void *u, int cause) {
    (void)u;
    (void)cause;
    ++quarantines;
    assert(held);
}
static int allowed(void *u, uint32_t slot, uint32_t off, uint32_t n) {
    (void)u;
    return (slot == 0 && off == 0x40 && n == 8) ||
           (slot == 2700 && scoring_field_allowed_25661166(0, off, n));
}
static unsigned char *address(uint32_t slot, uint32_t off) {
    return (slot == 2700 ? score : body) + off;
}
static int read_field(void *u, uint32_t slot, uint32_t off, void *v, uint32_t n) {
    if (!owner_ok)
        ++unsafe_accesses;
    if (!allowed(u, slot, off, n))
        return 0;
    ++reads;
    memcpy(v, address(slot, off), n);
    if (mode == 8 && reads == 1)
        owner_ok = 0;
    return 1;
}
static int write_field(void *u, uint32_t slot, uint32_t off, const void *v, uint32_t n) {
    if (!owner_ok)
        ++unsafe_accesses;
    if (!allowed(u, slot, off, n))
        return 0;
    ++writes;
    memcpy(address(slot, off), v, writes == fail_write ? n / 2 : n);
    if (mode == 9 && writes == 1)
        owner_ok = 0;
    return writes != fail_write;
}
static int contacts(void *u, uint64_t e, int original) {
    (void)u;
    assert(owner(NULL, e));
    ++contact_count;
    double x;
    memcpy(&x, body + 0x40, 8);
    assert(x == (original ? 1 : 9));
    contact_position = x;
    if (!original) {
        if (mode == 1 || mode == 4 || mode == 5) {
            double altered = 99;
            memcpy(body + 0x40, &altered, 8);
            return 0;
        }
        if (mode == 2)
            context_ok = 0;
        if (mode == 3)
            owner_ok = 0;
        if (mode == 7) {
            memset(plan.fields[0].expected, 0, 8);
            return 0;
        }
    }
    return !(original && mode == 4);
}
static RestoreBackend backend = {NULL, context, allowed, read_field, write_field};
static RestoreFlowOps ops = {NULL, enter, owner, contacts, leave, quarantine};
static void init(void) {
    memset(&flow, 0, sizeof(flow));
    memset(&plan, 0, sizeof(plan));
    memset(body, 0x55, sizeof(body));
    memset(score, 0x73, sizeof(score));
    double old = 1, new_value = 9;
    memcpy(body + 0x40, &old, 8);
    plan.epoch = 42;
    plan.count = 4;
    plan.fields[0].size = 8;
    plan.fields[0].field_offset = 0x40;
    memcpy(plan.fields[0].expected, &old, 8);
    memcpy(plan.fields[0].desired, &new_value, 8);
    const unsigned offsets[3] = {0x160, 0x27290, 0x27360};
    int32_t flag = 2;
    for (unsigned i = 0; i < 3; i++) {
        RestoreField *f = plan.fields + i + 1;
        f->object_slot = 2700;
        f->field_offset = offsets[i];
        f->size = 4;
        memcpy(f->expected, &flag, 4);
        memcpy(score + offsets[i], &flag, 4);
    }
    memcpy(old_body, body, sizeof(body));
    memcpy(old_score, score, sizeof(score));
    held = entered = leaves = quarantines = writes = fail_write = mode = contact_count = reads =
        unsafe_accesses = 0;
    owner_ok = context_ok = 1;
    resume_value = -1;
    contact_position = 1;
}
static int run(RestoreFlowReport *r) {
    return restore_flow_execute(&flow, &workspace, &plan, &backend, &ops, r);
}
int main(void) {
    RestoreFlowReport r;
    init();
    assert(run(&r) == FLOW_OK && writes == 4 && contact_count == 1 && flow.completed == 1 &&
           resume_value == 1 && !held);
    for (int m = 1; m <= 2; m++) {
        init();
        mode = m;
        assert(run(&r) == FLOW_COMPENSATED && r.recovery.restored == 4 && flow.compensated == 1 &&
               contact_count == 2 && resume_value == 1);
        assert(!memcmp(body, old_body, sizeof(body)) && !memcmp(score, old_score, sizeof(score)) &&
               contact_position == 1);
    }
    for (int m = 3; m <= 5; m++) {
        init();
        mode = m;
        if (m == 5)
            fail_write = 6;
        assert(run(&r) == FLOW_QUARANTINED && flow.quarantined && quarantines == 1 &&
               resume_value == 0);
        int prior = writes;
        assert(run(&r) == FLOW_QUARANTINED && writes == prior);
    }
    init();
    mode = 6;
    assert(run(&r) == FLOW_REJECTED && !writes && !held && !leaves);
    init();
    mode = 7;
    assert(run(&r) == FLOW_COMPENSATED && !memcmp(body, old_body, sizeof(body)) &&
           !memcmp(score, old_score, sizeof(score)));
    for (int i = 1; i <= 4; i++) {
        init();
        fail_write = i;
        assert(run(&r) == FLOW_REJECTED && !contact_count &&
               r.forward.restored == r.forward.attempted);
        assert(!memcmp(body, old_body, sizeof(body)) && !memcmp(score, old_score, sizeof(score)));
    }
    init();
    flow.busy = 1;
    assert(run(&r) == FLOW_BUSY && !entered);
    init();
    plan.fields[3].desired[0] = 1;
    assert(run(&r) == FLOW_BAD_ARGUMENT && !entered && !writes);
    for (int m = 8; m <= 9; m++) {
        init();
        mode = m;
        assert(run(&r) == FLOW_QUARANTINED && r.cause == FLOW_OWNER_LOST && quarantines == 1 &&
               resume_value == 0);
        assert(!unsafe_accesses && !contact_count && writes == (m == 8 ? 0 : 1));
        if (m == 8)
            assert(!memcmp(body, old_body, sizeof(body)) &&
                   !memcmp(score, old_score, sizeof(score)));
        else
            assert(r.forward.attempted == 1 && r.forward.restored == 0);
        owner_ok = 1;
        int prior = writes;
        assert(run(&r) == FLOW_QUARANTINED && writes == prior);
    }
    puts("PASS integrated restore: physical/scoring commit, contact rebuild, compensation after "
         "contact/context failure, pinned plan, partial-write rollback, fatal recovery quarantine "
         "and command lockout.");
    return 0;
}
