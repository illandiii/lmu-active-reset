#ifndef LMU_ACTIVE_RESET_NATIVE_H
#define LMU_ACTIVE_RESET_NATIVE_H
#include "active_reset_control.h"
#include "../restore/native_memory_backend.h"
typedef struct ResetNativeAdapter {
    void *user;
    int (*capture)(void *, const ResetIdentity *);
    /* Build coherent physical+scoring plan and ephemeral bindings on owner thread.
     * Return zero on success. Must not publish an old plan after failure. */
    int (*prepare)(void *, const ResetIdentity *, RestorePlan *, RestoreBindings *,
                   uintptr_t *score);
    int (*context)(void *, uint64_t epoch);
    RestoreFlowOps flow;
} ResetNativeAdapter;
typedef struct ResetNativeRuntime {
    ResetControl control;
    RestoreFlow flow;
    RestoreFlowWork work;
    RestorePlan plan;
    RestoreBindings bindings;
    uintptr_t scoring;
    RestoreFlowReport report;
} ResetNativeRuntime;
/* No injection, hooks or SDK input writes. Host must provide authoritative
 * capture/prepare/contact adapters; no game adapter is enabled by this module. */
int active_reset_native_step(ResetNativeRuntime *, const ResetNativeAdapter *, uint64_t thread,
                             uint64_t now, const ResetIdentity *, int allowed,
                             const ResetRequest *);
#endif
