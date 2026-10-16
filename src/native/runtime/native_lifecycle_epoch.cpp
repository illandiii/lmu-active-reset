#include "native_lifecycle_epoch.h"
#include <limits>

void native_lifecycle_init(NativeLifecycleEpoch *epoch) {
    InitializeCriticalSection(&epoch->lock);
    epoch->generation = 1;
    epoch->mutations = 0;
}
void native_lifecycle_mutation_enter(NativeLifecycleEpoch *epoch) {
    EnterCriticalSection(&epoch->lock);
    // Saturate at zero rather than ever reviving a previously saved epoch.
    if (epoch->generation == std::numeric_limits<uint64_t>::max())
        epoch->generation = 0;
    else if (epoch->generation)
        ++epoch->generation;
    ++epoch->mutations;
}
void native_lifecycle_mutation_leave(NativeLifecycleEpoch *epoch) {
    if (epoch->mutations)
        --epoch->mutations;
    else
        epoch->generation = 0;
    LeaveCriticalSection(&epoch->lock);
}
bool native_lifecycle_try_pin(NativeLifecycleEpoch *epoch, uint64_t expected,
                              NativeLifecyclePin *pin) {
    if (!epoch || !pin || pin->epoch || !expected || !TryEnterCriticalSection(&epoch->lock))
        return false;
    if (epoch->mutations || epoch->generation != expected) {
        LeaveCriticalSection(&epoch->lock);
        return false;
    }
    *pin = {epoch, expected, GetCurrentThreadId()};
    return true;
}
bool native_lifecycle_valid(const NativeLifecyclePin *pin) {
    return pin && pin->epoch && pin->thread == GetCurrentThreadId() &&
           native_lock_witness(reinterpret_cast<uintptr_t>(&pin->epoch->lock)).held_by_caller &&
           !pin->epoch->mutations && pin->generation && pin->generation == pin->epoch->generation;
}
bool native_lifecycle_release(NativeLifecyclePin *pin) {
    if (!pin || !pin->epoch || pin->thread != GetCurrentThreadId() ||
        !native_lock_witness(reinterpret_cast<uintptr_t>(&pin->epoch->lock)).held_by_caller)
        return false;
    LeaveCriticalSection(&pin->epoch->lock);
    *pin = {};
    return true;
}
bool native_lifecycle_changed(NativeLifecycleEpoch *epoch, uint64_t expected) {
    if (!epoch || !TryEnterCriticalSection(&epoch->lock))
        return false; // The mutation owner will publish the new epoch on release.
    const bool changed = epoch->generation != expected;
    LeaveCriticalSection(&epoch->lock);
    return changed;
}
