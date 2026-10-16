#include "native_lifetime_pin.h"

bool native_lifetime_try_acquire(NativeLifetimePin *pin, uintptr_t worker, uintptr_t control) {
    if (!pin || pin->acquired || !worker || !control || worker == control ||
        !native_lock_witness(worker).held_by_caller)
        return false;
    // Never wait while retaining the worker lock: session exit can hold the
    // control lock while waiting for this same worker to finish.
    if (!TryEnterCriticalSection(reinterpret_cast<CRITICAL_SECTION *>(control)))
        return false;
    pin->worker_lock = worker;
    pin->control_lock = control;
    pin->thread = GetCurrentThreadId();
    pin->acquired = true;
    if (!native_lifetime_valid(pin)) {
        native_lifetime_release(pin);
        return false;
    }
    return true;
}

bool native_lifetime_valid(const NativeLifetimePin *pin) {
    return pin && pin->acquired && pin->thread == GetCurrentThreadId() &&
           native_lock_witness(pin->worker_lock).held_by_caller &&
           native_lock_witness(pin->control_lock).held_by_caller;
}

bool native_lifetime_release(NativeLifetimePin *pin) {
    if (!pin || !pin->acquired || pin->thread != GetCurrentThreadId() ||
        !native_lock_witness(pin->control_lock).held_by_caller)
        return false;
    LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION *>(pin->control_lock));
    *pin = {};
    return true;
}
