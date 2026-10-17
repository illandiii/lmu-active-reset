#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "restore_native_memory_25661166.h"
#include <stdlib.h>
#include <string.h>
typedef struct NativeMemory {
    RestoreBindings bindings;
    DWORD owner_thread;
    int (*validate)(void *, uint64_t);
    void *user;
    uintptr_t scoring;
} NativeMemory;
static int context(void *user, uint64_t epoch) {
    NativeMemory *n = user;
    return GetCurrentThreadId() == n->owner_thread && epoch == n->bindings.epoch &&
           n->validate(n->user, epoch);
}
static int allowed(void *user, uint32_t slot, uint32_t offset, uint32_t size) {
    NativeMemory *n = user;
    if (slot == RESTORE_OBJECT_COUNT)
        return GetCurrentThreadId() == n->owner_thread && n->scoring >= 0x10000 &&
               n->scoring < UINT64_C(0x800000000000) - 0x27400 && size == 4 &&
               (offset == 0xd4 || offset == 0x160 || offset == 0x27290 || offset == 0x27360);
    return GetCurrentThreadId() == n->owner_thread &&
           restore_binding_field(&n->bindings, slot, offset, size);
}
static uintptr_t address_of(const NativeMemory *n, uint32_t slot, uint32_t offset) {
    return (slot == RESTORE_OBJECT_COUNT ? n->scoring : n->bindings.address[slot]) + offset;
}
static int read_field(void *user, uint32_t slot, uint32_t offset, void *out, uint32_t size) {
    NativeMemory *n = user;
    SIZE_T done = 0;
    if (!allowed(user, slot, offset, size))
        return 0;
    return ReadProcessMemory(GetCurrentProcess(), (void *)address_of(n, slot, offset), out, size,
                             &done) &&
           done == size;
}
typedef struct WritableRegion {
    uintptr_t begin, end;
} WritableRegion;
/* Per-preflight cache only. Never reuse permission observations for writes. */
static int writable_cached(uintptr_t address, SIZE_T size, WritableRegion *cache) {
    if (!size || address > UINTPTR_MAX - size)
        return 0;
    uintptr_t end = address + size;
    while (address < end) {
        if (address >= cache->begin && address < cache->end) {
            address = cache->end;
            continue;
        }
        MEMORY_BASIC_INFORMATION page;
        if (VirtualQuery((void *)address, &page, sizeof(page)) != sizeof(page) ||
            page.State != MEM_COMMIT || (page.Protect & PAGE_GUARD) ||
            !(page.Protect == PAGE_READWRITE || page.Protect == PAGE_WRITECOPY))
            return 0;
        uintptr_t base = (uintptr_t)page.BaseAddress;
        if (page.RegionSize > UINTPTR_MAX - base)
            return 0;
        uintptr_t next = base + page.RegionSize;
        if (next <= address)
            return 0;
        *cache = (WritableRegion){base, next};
        address = next;
    }
    return 1;
}
static int writable(uintptr_t address, SIZE_T size) {
    WritableRegion cache = {0, 0};
    return writable_cached(address, size, &cache);
}
static int write_field(void *user, uint32_t slot, uint32_t offset, const void *in, uint32_t size) {
    NativeMemory *n = user;
    SIZE_T done = 0;
    if (!allowed(user, slot, offset, size))
        return 0;
    uintptr_t address = address_of(n, slot, offset);
    if (!writable(address, size))
        return 0;
    /* No external context query here: reverse rollback must use the original
     * binding even after the forward-operation context has become invalid. */
    return WriteProcessMemory(GetCurrentProcess(), (void *)address, in, size, &done) &&
           done == size;
}
int restore_native_memory_execute(const RestorePlan *plan, const RestoreBindings *bindings,
                                  uint32_t thread, int (*validate)(void *, uint64_t), void *user,
                                  RestoreReport *report) {
    if (!report)
        return RESTORE_BAD_PLAN;
    *report = (RestoreReport){0, 0, RESTORE_BAD_PLAN};
    if (!plan || !bindings || !thread || !validate)
        return RESTORE_BAD_PLAN;
    if (GetCurrentThreadId() != thread) {
        report->result = RESTORE_CONTEXT_CHANGED;
        return report->result;
    }
    NativeMemory native = {*bindings, thread, validate, user, 0};
    int status = restore_bindings_validate(&native.bindings, plan);
    if (status) {
        report->result = status;
        return status;
    }
    WritableRegion cache = {0, 0};
    for (size_t i = 0; i < plan->count; i++) {
        const RestoreField *f = plan->fields + i;
        if (!writable_cached(address_of(&native, f->object_slot, f->field_offset), f->size,
                             &cache)) {
            report->result = RESTORE_FIELD_FORBIDDEN;
            return report->result;
        }
    }
    RestoreBackend backend = {&native, context, allowed, read_field, write_field};
    return restore_transaction(plan, &backend, report);
}
typedef struct JointRange {
    uintptr_t begin, end;
} JointRange;
static int compare_joint_range(const void *a, const void *b) {
    const JointRange *x = a;
    const JointRange *y = b;
    return x->begin < y->begin ? -1 : x->begin > y->begin;
}
static int validate_joint(const RestorePlan *plan, NativeMemory *native) {
    if (!plan || !plan->epoch || plan->epoch != native->bindings.epoch || plan->count < 4 ||
        plan->count > RESTORE_MAX_FIELDS)
        return RESTORE_BAD_PLAN;
    JointRange ranges[RESTORE_MAX_FIELDS];
    unsigned mask = 0;
    for (size_t i = 0; i < plan->count; i++) {
        const RestoreField *f = plan->fields + i;
        if (f->reserved || !allowed(native, f->object_slot, f->field_offset, f->size))
            return RESTORE_FIELD_FORBIDDEN;
        uintptr_t start = address_of(native, f->object_slot, f->field_offset);
        ranges[i] = (JointRange){start, start + f->size};
        if (f->object_slot == RESTORE_OBJECT_COUNT) {
            const int failure = f->field_offset == 0xd4;
            unsigned bit = failure                      ? 8
                           : f->field_offset == 0x160   ? 1
                           : f->field_offset == 0x27290 ? 2
                                                        : 4;
            int32_t expected, desired;
            memcpy(&expected, f->expected, 4);
            memcpy(&desired, f->desired, 4);
            if (mask & bit || expected < 0 || expected > (failure ? 9 : 2) || desired)
                return RESTORE_BAD_PLAN;
            mask |= bit;
        }
    }
    if (mask != 7 && mask != 15)
        return RESTORE_BAD_PLAN;
    qsort(ranges, plan->count, sizeof(*ranges), compare_joint_range);
    for (size_t i = 1; i < plan->count; i++)
        if (ranges[i].begin < ranges[i - 1].end)
            return RESTORE_BAD_PLAN;
    WritableRegion cache = {0, 0};
    for (size_t i = 0; i < plan->count; i++)
        if (!writable_cached(ranges[i].begin, ranges[i].end - ranges[i].begin, &cache))
            return RESTORE_FIELD_FORBIDDEN;
    return RESTORE_OK;
}
int restore_joint_native_preflight(const RestorePlan *plan, const RestoreBindings *bindings,
                                   uintptr_t score, uint32_t thread,
                                   int (*validate)(void *, uint64_t), void *user) {
    if (!bindings || !thread || !validate)
        return RESTORE_BAD_PLAN;
    if (GetCurrentThreadId() != thread)
        return RESTORE_CONTEXT_CHANGED;
    NativeMemory native = {*bindings, thread, validate, user, score};
    int status = validate_joint(plan, &native);
    if (status)
        return status;
    unsigned char bytes[RESTORE_FIELD_BYTES];
    for (size_t i = 0; i < plan->count; i++) {
        const RestoreField *f = plan->fields + i;
        if (!context(&native, plan->epoch))
            return RESTORE_CONTEXT_CHANGED;
        if (!read_field(&native, f->object_slot, f->field_offset, bytes, f->size))
            return RESTORE_READ_FAILED;
        if (memcmp(bytes, f->expected, f->size))
            return RESTORE_STATE_CHANGED;
    }
    return context(&native, plan->epoch) ? RESTORE_OK : RESTORE_CONTEXT_CHANGED;
}
int restore_joint_native_memory_execute(const RestorePlan *plan, const RestoreBindings *bindings,
                                        uintptr_t score, uint32_t thread,
                                        int (*validate)(void *, uint64_t), void *user,
                                        RestoreReport *report) {
    if (!report)
        return RESTORE_BAD_PLAN;
    *report = (RestoreReport){0, 0, RESTORE_BAD_PLAN};
    if (!bindings || !thread || !validate)
        return RESTORE_BAD_PLAN;
    if (GetCurrentThreadId() != thread) {
        report->result = RESTORE_CONTEXT_CHANGED;
        return report->result;
    }
    NativeMemory native = {*bindings, thread, validate, user, score};
    int status = validate_joint(plan, &native);
    if (status) {
        report->result = status;
        return status;
    }
    RestoreBackend backend = {&native, context, allowed, read_field, write_field};
    return restore_transaction(plan, &backend, report);
}
int restore_joint_native_flow_execute(RestoreFlow *flow, RestoreFlowWork *work,
                                      const RestorePlan *plan, const RestoreBindings *bindings,
                                      uintptr_t score, uint32_t thread,
                                      int (*validate)(void *, uint64_t), void *user,
                                      const RestoreFlowOps *ops, RestoreFlowReport *report) {
    if (!report)
        return FLOW_BAD_ARGUMENT;
    memset(report, 0, sizeof(*report));
    report->result = FLOW_BAD_ARGUMENT;
    if (!bindings || !thread || !validate)
        return report->result;
    if (GetCurrentThreadId() != thread) {
        report->result = FLOW_REJECTED;
        report->cause = RESTORE_CONTEXT_CHANGED;
        return report->result;
    }
    NativeMemory native = {*bindings, thread, validate, user, score};
    int status = validate_joint(plan, &native);
    if (status) {
        report->cause = status;
        return report->result;
    }
    RestoreBackend backend = {&native, context, allowed, read_field, write_field};
    return restore_flow_execute(flow, work, plan, &backend, ops, report);
}
