#include "checkpoint_powertrain_control.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static RestorePlan work, out;
static unsigned char memory[0xa000], before[0xa000];
static int writes, fail_write, fail_read, deny, stale;
static int context(void *user, uint64_t epoch) {
    (void)user;
    return !stale && epoch == 42;
}
static int allowed(void *user, uint32_t slot, uint32_t off, uint32_t n) {
    (void)user;
    return !deny && ((slot == 104 && ((off == 0x448 && n == 8) || (off == 0x4810 && n == 8) ||
                                      (off == 0x4818 && n == 8) || (off == 0x4820 && n == 2) ||
                                      (off == 0x4828 && n == 24))) ||
                     (slot == 105 && (off == 0x50 || off == 0x1b0) && n == 8));
}
static int read_field(void *user, uint32_t slot, uint32_t off, void *value, uint32_t n) {
    (void)user;
    (void)slot;
    if (fail_read)
        return 0;
    memcpy(value, memory + (slot == 105 ? 0x5000 : 0) + off, n);
    return 1;
}
static int write_field(void *user, uint32_t slot, uint32_t off, const void *value, uint32_t n) {
    (void)user;
    (void)slot;
    ++writes;
    if (writes == fail_write) {
        memcpy(memory + (slot == 105 ? 0x5000 : 0) + off, value, n / 2);
        return 0;
    }
    memcpy(memory + (slot == 105 ? 0x5000 : 0) + off, value, n);
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
    PowertrainControl s = {0}, current = {0}, prepared = {0};
    PowertrainControlBank bank = {0}, saved;
    s.ramp_d0 = .5;
    s.timer_e8 = 2;
    s.factor_f0 = 3;
    s.factor_f8 = 4;
    s.engine_50 = 5;
    s.engine_1b0 = 101325;
    s.index_dc = 1;
    s.active_e0 = 1;
    s.table_token = 99;
    s.table_counts[1] = s.table_counts[2] = s.table_counts[3] = 2;
    current = s;
    RestoreBackend backend = {NULL, context, allowed, read_field, write_field};
    RestoreReport report;
    assert(powertrain_control_save(&bank, 7, &s));
    saved = bank;
    assert(powertrain_control_prepare(&bank, 7, &current, &prepared) &&
           !memcmp(&s, &prepared, sizeof(s)));
    assert(!powertrain_control_prepare(&bank, 8, &current, &prepared));
    current.engine_1b0 = NAN;
    assert(!powertrain_control_save(&bank, 8, &current) && !memcmp(&bank, &saved, sizeof(bank)));
    assert(!powertrain_control_prepare(&bank, 7, &current, &prepared));
    reset_plan();
    assert(powertrain_control_plan(&bank, 7, &backend, &work, &out) == RESTORE_OK &&
           out.count == 7);
    memcpy(before, memory, sizeof(memory));
    assert(restore_transaction(&out, &backend, &report) == RESTORE_OK);
    for (size_t i = 0; i < out.count; i++) {
        RestoreField *f = out.fields + i;
        unsigned off = (f->object_slot == 105 ? 0x5000 : 0) + f->field_offset;
        assert(!memcmp(memory + off, f->desired, f->size));
        memcpy(before + off, f->desired, f->size);
    }
    assert(!memcmp(before, memory, sizeof(memory)));
    for (int failure = 1; failure <= 7; failure++) {
        memset(memory, 0, sizeof(memory));
        reset_plan();
        assert(!powertrain_control_plan(&bank, 7, &backend, &work, &out));
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
    assert(powertrain_control_plan(&bank, 7, &backend, &work, &out) == RESTORE_FIELD_FORBIDDEN &&
           out.count == 123);
    deny = 0;
    reset_plan();
    fail_read = 1;
    assert(powertrain_control_plan(&bank, 7, &backend, &work, &out) == RESTORE_READ_FAILED &&
           out.count == 123);
    fail_read = 0;
    reset_plan();
    stale = 1;
    assert(powertrain_control_plan(&bank, 7, &backend, &work, &out) == RESTORE_CONTEXT_CHANGED &&
           out.count == 123);
    stale = 0;
    reset_plan();
    work.fields[0].field_offset = 0x4810;
    assert(powertrain_control_plan(&bank, 7, &backend, &work, &out) == RESTORE_BAD_PLAN &&
           out.count == 123);
    current = s;
    current.table_token++;
    assert(!powertrain_control_prepare(&bank, 7, &current, &prepared));
    current = s;
    current.table_counts[1]++;
    assert(!powertrain_control_prepare(&bank, 7, &current, &prepared));
    current = s;
    current.index_d8 = 1;
    assert(!powertrain_control_valid(&current));
    current = s;
    current.index_dc = 2;
    assert(!powertrain_control_valid(&current));
    current = s;
    current.timer_e8 = -1;
    assert(!powertrain_control_valid(&current));
    current = s;
    current.active_e0 = 2;
    assert(!powertrain_control_valid(&current));
    current = s;
    current.reserved[5] = 1;
    assert(!powertrain_control_valid(&current));
    puts("PASS partial powertrain control: exact fields, untouched neighbours, seven partial-write "
         "rollbacks, table identity/index/finite/timer/flags checks and plan rejection.");
    return 0;
}
