#include "../../src/native/restore/restore_transaction.h"
#include <stdio.h>
#include <string.h>
typedef struct Memory {
    unsigned char data[4][64];
    unsigned writes;
    int failure, fail_at, context_calls;
} Memory;
static RestorePlan plan;
static Memory mem;
static RestoreReport report;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            printf("FAIL transaction line %d\n", __LINE__);                                        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static int context(void *p, uint64_t epoch) {
    Memory *m = p;
    ++m->context_calls;
    return epoch == 42 && !(m->failure == 3 && m->context_calls >= 5);
}
static int allowed(void *p, uint32_t slot, uint32_t off, uint32_t size) {
    (void)p;
    return slot < 4 && off <= 64 && size <= 64 - off;
}
static int read_field(void *p, uint32_t slot, uint32_t off, void *data, uint32_t size) {
    Memory *m = p;
    if (!allowed(p, slot, off, size))
        return 0;
    memcpy(data, m->data[slot] + off, size);
    return 1;
}
static int write_field(void *p, uint32_t slot, uint32_t off, const void *data, uint32_t size) {
    Memory *m = p;
    if (!allowed(p, slot, off, size))
        return 0;
    ++m->writes;
    if (m->failure == 1 && m->writes == (unsigned)m->fail_at) {
        memcpy(m->data[slot] + off, data, size / 2);
        return 0;
    }
    if (m->failure == 2 && m->writes >= (unsigned)m->fail_at)
        return 0;
    if (m->failure == 4 && m->writes == (unsigned)m->fail_at)
        return 1; /* falsely successful backend */
    memcpy(m->data[slot] + off, data, size);
    return 1;
}
static void reset(void) {
    memset(&mem, 0, sizeof(mem));
    memset(&plan, 0, sizeof(plan));
    plan.count = 4;
    plan.epoch = 42;
    for (int i = 0; i < 4; i++) {
        plan.fields[i].object_slot = i;
        plan.fields[i].field_offset = 8;
        plan.fields[i].size = 16;
        memset(plan.fields[i].desired, i + 1, 16);
    }
}
int main(void) {
    RestoreBackend b = {&mem, context, allowed, read_field, write_field};
    reset();
    CHECK(restore_transaction(&plan, &b, &report) == 0 && report.attempted == 4 && mem.writes == 4);
    for (int at = 1; at <= 4; at++) {
        reset();
        mem.failure = 1;
        mem.fail_at = at;
        CHECK(restore_transaction(&plan, &b, &report) == RESTORE_WRITE_FAILED &&
              report.restored == (size_t)at);
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 64; j++)
                CHECK(mem.data[i][j] == 0);
    }
    reset();
    mem.failure = 2;
    mem.fail_at = 2;
    CHECK(restore_transaction(&plan, &b, &report) == RESTORE_ROLLBACK_FAILED);
    reset();
    mem.failure = 4;
    mem.fail_at = 3;
    CHECK(restore_transaction(&plan, &b, &report) == RESTORE_VERIFY_FAILED && report.restored == 3);
    reset();
    mem.failure = 3;
    CHECK(restore_transaction(&plan, &b, &report) == RESTORE_CONTEXT_CHANGED &&
          report.restored == 2);
    reset();
    mem.data[3][8] = 9;
    CHECK(restore_transaction(&plan, &b, &report) == RESTORE_STATE_CHANGED && mem.writes == 0);
    reset();
    plan.fields[3].object_slot = 0;
    CHECK(restore_transaction(&plan, &b, &report) == RESTORE_BAD_PLAN && mem.writes == 0);
    reset();
    plan.fields[3].size = 129;
    CHECK(restore_transaction(&plan, &b, &report) == RESTORE_BAD_PLAN && mem.writes == 0);
    reset();
    plan.fields[3].object_slot = 4;
    CHECK(restore_transaction(&plan, &b, &report) == RESTORE_FIELD_FORBIDDEN && mem.writes == 0);
    reset();
    plan.fields[3].field_offset = UINT32_MAX;
    CHECK(restore_transaction(&plan, &b, &report) == RESTORE_BAD_PLAN && mem.writes == 0);
    puts("PASS transaction: preflight, all partial-write rollback positions, rollback failure, "
         "readback verification, context loss, stale data, overlap and bounds. Owned fixture "
         "memory only.");
    return 0;
}
