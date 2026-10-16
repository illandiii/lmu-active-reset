#include "fenced_restore.h"
#include "native_lock_witness.h"
static bool lock_owned(FencedRestoreAdapter *a) {
    return !a->native_lock || native_lock_witness(a->native_lock).held_by_caller;
}
static int enter(void *user, uint64_t epoch) {
    auto *a = static_cast<FencedRestoreAdapter *>(user);
    if (!a || !a->fence || !a->lifetime.enter || !a->lifetime.owner_alive ||
        !a->lifetime.contacts || !a->lifetime.leave || !a->lifetime.quarantine)
        return 0;
    if (!lock_owned(a) || !simulation_fence_hold(a->fence, epoch))
        return 0;
    if (!a->lifetime.enter(a->lifetime.user, epoch)) {
        simulation_fence_release(a->fence, true);
        return 0;
    }
    // Conservative: after admission, no detach even if an early write fails.
    simulation_fence_mark_dirty(a->fence);
    return 1;
}
static int owner(void *user, uint64_t epoch) {
    auto *a = static_cast<FencedRestoreAdapter *>(user);
    if (!lock_owned(a)) {
        simulation_fence_quarantine(a->fence);
        return 0;
    }
    return simulation_fence_owner_alive(a->fence, epoch) &&
           a->lifetime.owner_alive(a->lifetime.user, epoch);
}
static int contacts(void *user, uint64_t epoch, int original) {
    auto *a = static_cast<FencedRestoreAdapter *>(user);
    return owner(user, epoch) && a->lifetime.contacts(a->lifetime.user, epoch, original);
}
static void leave(void *user, int resume) {
    auto *a = static_cast<FencedRestoreAdapter *>(user);
    if (!lock_owned(a))
        simulation_fence_quarantine(a->fence);
    a->lifetime.leave(a->lifetime.user, resume);
    if (!lock_owned(a))
        simulation_fence_quarantine(a->fence);
    simulation_fence_release(a->fence, resume != 0);
}
static void quarantine(void *user, int cause) {
    auto *a = static_cast<FencedRestoreAdapter *>(user);
    simulation_fence_quarantine(a->fence);
    a->lifetime.quarantine(a->lifetime.user, cause);
}
RestoreFlowOps fenced_restore_ops(FencedRestoreAdapter *a) {
    return {a, enter, owner, contacts, leave, quarantine};
}
