#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../../src/native/restore/restore_native_memory_25661166.h"
static RestorePlan plan;
static RestoreBindings bindings;
typedef struct Context {
    unsigned calls, fail_at;
    int rebind;
} Context;
static int valid(void *p, uint64_t epoch) {
    Context *c = p;
    ++c->calls;
    if (c->rebind)
        bindings.address[0] = 0; /* External binding mutation cannot redirect rollback. */
    return epoch == 77 && (!c->fail_at || c->calls < c->fail_at);
}
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            printf("FAIL %d %s\n", __LINE__, #x);                                                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    unsigned char *memory = VirtualAlloc(NULL, 0x5000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(memory);
    unsigned char original[0x5000];
    memset(original, 0x51, sizeof(original));
    memcpy(memory, original, sizeof(original));
    bindings.epoch = 77;
    bindings.body_count = 1;
    bindings.address[0] = (uintptr_t)memory;
    plan.epoch = 77;
    plan.count = 2;
    for (unsigned i = 0; i < 2; i++) {
        RestoreField *f = plan.fields + i;
        f->field_offset = i ? 0x58 : 8;
        f->size = 8;
        memcpy(f->expected, original + f->field_offset, 8);
        memset(f->desired, 0x22 + i, 8);
    }
    RestoreReport report;
    Context c = {0, 0, 0};
    DWORD tid = GetCurrentThreadId();
    CHECK(restore_native_memory_execute(&plan, &bindings, tid, valid, &c, &report) == RESTORE_OK);
    for (unsigned i = 0; i < sizeof(original); i++)
        CHECK(memory[i] == ((i >= 8 && i < 16) ? 0x22 : (i >= 0x58 && i < 0x60) ? 0x23 : 0x51));
    memcpy(memory, original, sizeof(original));
    c = (Context){0, 4, 1};
    CHECK(restore_native_memory_execute(&plan, &bindings, tid, valid, &c, &report) ==
          RESTORE_CONTEXT_CHANGED);
    CHECK(report.attempted == 1 && report.restored == 1 &&
          !memcmp(memory, original, sizeof(original)));
    bindings.address[0] = (uintptr_t)memory;
    c = (Context){0, 0, 0};
    CHECK(restore_native_memory_execute(&plan, &bindings, tid + 1, valid, &c, &report) ==
              RESTORE_CONTEXT_CHANGED &&
          c.calls == 0);
    plan.fields[0].field_offset = 0x18;
    CHECK(restore_native_memory_execute(&plan, &bindings, tid, valid, &c, &report) ==
          RESTORE_FIELD_FORBIDDEN);
    plan.fields[0].field_offset = 8;
    DWORD prior;
    CHECK(VirtualProtect(memory, 0x5000, PAGE_READONLY, &prior));
    CHECK(restore_native_memory_execute(&plan, &bindings, tid, valid, &c, &report) ==
          RESTORE_FIELD_FORBIDDEN);
    CHECK(report.attempted == 0 && report.restored == 0);
    CHECK(!memcmp(memory, original, sizeof(original)));
    CHECK(VirtualProtect(memory, 0x5000, prior, &prior));
    bindings.body_count = 2;
    bindings.address[1] = (uintptr_t)(memory + 0x1000);
    plan.fields[1].object_slot = 1;
    CHECK(VirtualProtect(memory + 0x1000, 0x1000, PAGE_READONLY, &prior));
    CHECK(restore_native_memory_execute(&plan, &bindings, tid, valid, &c, &report) ==
          RESTORE_FIELD_FORBIDDEN);
    CHECK(report.attempted == 0 && !memcmp(memory, original, sizeof(original)));
    CHECK(VirtualProtect(memory + 0x1000, 0x1000, prior, &prior));
    /* A field straddling the cached writable region must query its second page. */
    bindings.address[1] = (uintptr_t)(memory + 0xff4);
    plan.fields[1].field_offset = 8;
    const DWORD denied[] = {PAGE_READONLY, PAGE_NOACCESS, PAGE_READWRITE | PAGE_GUARD};
    for (unsigned i = 0; i < sizeof(denied) / sizeof(*denied); i++) {
        CHECK(VirtualProtect(memory + 0x1000, 0x1000, denied[i], &prior));
        CHECK(restore_native_memory_execute(&plan, &bindings, tid, valid, &c, &report) ==
              RESTORE_FIELD_FORBIDDEN);
        CHECK(report.attempted == 0 && report.restored == 0);
        CHECK(VirtualProtect(memory + 0x1000, 0x1000, prior, &prior));
        CHECK(!memcmp(memory, original, sizeof(original)));
    }
    CHECK(restore_native_memory_execute(&plan, &bindings, tid, valid, &c, &report) == RESTORE_OK);
    memcpy(memory, original, sizeof(original));
    /* Permission caching must not survive one operation. */
    CHECK(VirtualProtect(memory + 0x1000, 0x1000, PAGE_READONLY, &prior));
    CHECK(restore_native_memory_execute(&plan, &bindings, tid, valid, &c, &report) ==
          RESTORE_FIELD_FORBIDDEN);
    CHECK(report.attempted == 0);
    CHECK(VirtualProtect(memory + 0x1000, 0x1000, prior, &prior));
    CHECK(!memcmp(memory, original, sizeof(original)));
    unsigned char *score = VirtualAlloc(NULL, 0x28000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(score);
    const uint32_t offsets[] = {0x160, 0x27290, 0x27360};
    plan.count = 5;
    for (unsigned i = 0; i < 3; i++) {
        RestoreField *f = plan.fields + 2 + i;
        f->object_slot = RESTORE_OBJECT_COUNT;
        f->field_offset = offsets[i];
        f->size = 4;
        int32_t flag = 2;
        memcpy(score + offsets[i], &flag, 4);
        memcpy(f->expected, &flag, 4);
        memset(f->desired, 0, 4);
    }
    CHECK(restore_joint_native_preflight(&plan, &bindings, (uintptr_t)score, tid, valid, &c) ==
          RESTORE_OK);
    CHECK(VirtualProtect(score + 0x27000, 0x1000, PAGE_READONLY, &prior));
    CHECK(restore_joint_native_preflight(&plan, &bindings, (uintptr_t)score, tid, valid, &c) ==
          RESTORE_FIELD_FORBIDDEN);
    CHECK(restore_joint_native_memory_execute(&plan, &bindings, (uintptr_t)score, tid, valid, &c,
                                              &report) == RESTORE_FIELD_FORBIDDEN);
    CHECK(report.attempted == 0 && !memcmp(memory, original, sizeof(original)));
    for (unsigned i = 0; i < 3; i++) {
        int32_t flag;
        memcpy(&flag, score + offsets[i], 4);
        CHECK(flag == 2);
    }
    CHECK(VirtualProtect(score + 0x27000, 0x1000, prior, &prior));
    CHECK(restore_joint_native_memory_execute(&plan, &bindings, (uintptr_t)score, tid, valid, &c,
                                              &report) == RESTORE_OK);
    CHECK(VirtualFree(score, 0, MEM_RELEASE));
    CHECK(VirtualFree(memory, 0, MEM_RELEASE));
    printf(
        "PASS native self-memory backend; exact typed fields; readback; owner thread; frozen "
        "bindings; rollback after context loss; readonly pages refused. FIXTURE PROCESS ONLY.\n");
    return 0;
}
