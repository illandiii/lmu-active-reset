#include "checkpoint_driveline_outputs.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static RestorePlan work, out;
static unsigned char memory[0x5000], before[0x5000];
static int writes, fail_write, fail_read, deny, stale;
static int context(void *user, uint64_t epoch) {
    (void)user;
    return !stale && epoch == 42;
}
static int allowed(void *user, uint32_t slot, uint32_t off, uint32_t n) {
    (void)user;
    return !deny && slot == 104 && n == 8 &&
           (off == 0x448 || off == 0x46d0 || off == 0x46e0 || off == 0x46e8 || off == 0x4b38);
}
static int read_field(void *user, uint32_t slot, uint32_t off, void *value, uint32_t n) {
    (void)user;
    (void)slot;
    if (fail_read)
        return 0;
    memcpy(value, memory + off, n);
    return 1;
}
static int write_field(void *user, uint32_t slot, uint32_t off, const void *value, uint32_t n) {
    (void)user;
    (void)slot;
    ++writes;
    if (writes == fail_write) {
        memcpy(memory + off, value, n / 2);
        return 0;
    }
    memcpy(memory + off, value, n);
    return 1;
}
static void reset_plan(void) {
    memset(&work, 0, sizeof(work));
    work.epoch = 42;
    work.count = 1;
    work.fields[0].object_slot = 104;
    work.fields[0].field_offset = 0x448;
    work.fields[0].size = 8;
    memcpy(work.fields[0].expected, memory + 0x448, 8);
    memcpy(work.fields[0].desired, memory + 0x448, 8);
}
int main(void) {
    DrivelineOutputs s = {1, -2, 3.5, 0}, current = {9, 8, 7, 6}, prepared = {0};
    DrivelineOutputsBank bank = {0}, saved;
    RestoreBackend backend = {NULL, context, allowed, read_field, write_field};
    RestoreReport report;
    assert(driveline_outputs_save(&bank, 7, &s));
    saved = bank;
    assert(driveline_outputs_prepare(&bank, 7, &current, &prepared) &&
           !memcmp(&s, &prepared, sizeof(s)));
    assert(!driveline_outputs_prepare(&bank, 8, &current, &prepared));
    current.hdv_46e8 = NAN;
    assert(!driveline_outputs_save(&bank, 8, &current) && !memcmp(&bank, &saved, sizeof(bank)));
    assert(!driveline_outputs_prepare(&bank, 7, &current, &prepared));
    reset_plan();
    assert(driveline_outputs_plan(&bank, 7, &backend, &work, &out) == RESTORE_OK && out.count == 5);
    memcpy(before, memory, sizeof(memory));
    assert(restore_transaction(&out, &backend, &report) == RESTORE_OK);
    const unsigned offsets[4] = {0x46d0, 0x46e0, 0x46e8, 0x4b38};
    const double values[4] = {1, -2, 3.5, 0};
    for (unsigned i = 0; i < 4; i++) {
        double value;
        memcpy(&value, memory + offsets[i], 8);
        assert(value == values[i]);
        memcpy(before + offsets[i], values + i, 8);
    }
    assert(!memcmp(before, memory, sizeof(memory)));
    for (int failure = 1; failure <= 5; failure++) {
        memset(memory, 0, sizeof(memory));
        reset_plan();
        assert(!driveline_outputs_plan(&bank, 7, &backend, &work, &out));
        memcpy(before, memory, sizeof(memory));
        writes = 0;
        fail_write = failure;
        assert(restore_transaction(&out, &backend, &report) == RESTORE_WRITE_FAILED);
        assert(!memcmp(before, memory, sizeof(memory)));
    }
    fail_write = 0;
    reset_plan();
    deny = 1;
    out.count = 123;
    assert(driveline_outputs_plan(&bank, 7, &backend, &work, &out) == RESTORE_FIELD_FORBIDDEN &&
           out.count == 123);
    deny = 0;
    reset_plan();
    fail_read = 1;
    assert(driveline_outputs_plan(&bank, 7, &backend, &work, &out) == RESTORE_READ_FAILED &&
           out.count == 123);
    fail_read = 0;
    reset_plan();
    stale = 1;
    assert(driveline_outputs_plan(&bank, 7, &backend, &work, &out) == RESTORE_CONTEXT_CHANGED &&
           out.count == 123);
    stale = 0;
    reset_plan();
    work.fields[0].field_offset = 0x46d0;
    assert(driveline_outputs_plan(&bank, 7, &backend, &work, &out) == RESTORE_BAD_PLAN &&
           out.count == 123);
    puts(
        "PASS driveline outputs: checkpoint identity, finite values, exact destinations, untouched "
        "neighbours, five partial-write rollbacks, forbidden/read/context/overlap rejection.");
    return 0;
}
