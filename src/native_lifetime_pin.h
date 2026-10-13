#ifndef LMU_NATIVE_LIFETIME_PIN_H
#define LMU_NATIVE_LIFETIME_PIN_H
#include "native_lock_witness.h"

// Scope: callers serialized by the two supplied critical sections only.
// Acquires the world/control lock without waiting while the caller already owns
// the worker lock. It does not prove that every game mutator uses either lock.
struct NativeLifetimePin {
    uintptr_t worker_lock = 0, control_lock = 0;
    DWORD thread = 0;
    bool acquired = false;
};
bool native_lifetime_try_acquire(NativeLifetimePin *, uintptr_t worker_lock,
                                 uintptr_t control_lock);
bool native_lifetime_valid(const NativeLifetimePin *);
bool native_lifetime_release(NativeLifetimePin *);
#endif
