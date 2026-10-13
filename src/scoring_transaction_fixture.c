#include "scoring_plan_25661166.h"
#include <stdio.h>
#include <string.h>
static unsigned char memory[0x28000], original[0x28000];
static RestorePlan plan;
typedef struct State {
    unsigned writes, fail;
    uint64_t epoch;
} State;
static int context(void *p, uint64_t epoch) {
    return ((State *)p)->epoch == epoch;
}
static int allowed(void *p, uint32_t slot, uint32_t off, uint32_t n) {
    (void)p;
    return scoring_field_allowed_25661166(slot, off, n);
}
static int read_field(void *p, uint32_t slot, uint32_t off, void *out, uint32_t n) {
    (void)p;
    if (!scoring_field_allowed_25661166(slot, off, n))
        return 0;
    memcpy(out, memory + off, n);
    return 1;
}
static int write_field(void *p, uint32_t slot, uint32_t off, const void *in, uint32_t n) {
    State *s = p;
    if (!scoring_field_allowed_25661166(slot, off, n))
        return 0;
    ++s->writes;
    if (s->writes == s->fail) {
        memcpy(memory + off, in, n / 2);
        return 0;
    }
    memcpy(memory + off, in, n);
    return 1;
}
#define REQUIRE(x)                                                                                 \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                                        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    LapGuardFlags flags = {2, 1, 2, 1, 0};
    State state = {0, 0, 19};
    RestoreReport report;
    RestoreBackend backend = {&state, context, allowed, read_field, write_field};
    REQUIRE(scoring_plan_25661166(&flags, 19, &plan));
    memset(original, 0x5a, sizeof(original));
    for (size_t i = 0; i < plan.count; i++)
        memcpy(original + plan.fields[i].field_offset, plan.fields[i].expected, 4);
    memcpy(memory, original, sizeof(memory));
    REQUIRE(restore_transaction(&plan, &backend, &report) == RESTORE_OK);
    for (size_t p = 0; p < sizeof(memory); p++) {
        int changed = 0;
        for (size_t j = 0; j < plan.count; j++)
            if (p >= plan.fields[j].field_offset && p < plan.fields[j].field_offset + 4)
                changed = 1;
        REQUIRE(memory[p] == (changed ? 0 : original[p]));
    }
    for (unsigned fail = 1; fail <= 3; fail++) {
        memcpy(memory, original, sizeof(memory));
        state.writes = 0;
        state.fail = fail;
        REQUIRE(restore_transaction(&plan, &backend, &report) == RESTORE_WRITE_FAILED);
        REQUIRE(report.restored == fail && memcmp(memory, original, sizeof(memory)) == 0);
    }
    state.fail = 0;
    state.writes = 0;
    memory[0x27360] ^= 1;
    REQUIRE(restore_transaction(&plan, &backend, &report) == RESTORE_STATE_CHANGED &&
            state.writes == 0);
    memcpy(memory, original, sizeof(memory));
    state.epoch = 20;
    REQUIRE(restore_transaction(&plan, &backend, &report) == RESTORE_CONTEXT_CHANGED &&
            state.writes == 0);
    state.epoch = 19;
    plan.fields[2].field_offset = 0x2735c;
    REQUIRE(restore_transaction(&plan, &backend, &report) == RESTORE_FIELD_FORBIDDEN &&
            state.writes == 0);
    REQUIRE(!scoring_field_allowed_25661166(0, 0x10, 4));
    REQUIRE(!scoring_field_allowed_25661166(0, 0x164, 4));
    REQUIRE(!scoring_field_allowed_25661166(1, 0x160, 4));
    REQUIRE(!scoring_field_allowed_25661166(0, 0x160, 8));
    size_t prior_count = plan.count;
    flags.current = 3;
    REQUIRE(!scoring_plan_25661166(&flags, 19, &plan) && plan.count == prior_count &&
            plan.fields[2].field_offset == 0x2735c);
    printf(
        "PASS scoring 12-byte transaction; all other bytes preserved; three partial-write rollback "
        "points; stale data/context and old-build offset refused. OWNED MEMORY ONLY.\n");
    return 0;
}
