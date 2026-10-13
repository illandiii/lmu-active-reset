#ifndef LMU_NATIVE_LIFECYCLE_EPOCH_H
#define LMU_NATIVE_LIFECYCLE_EPOCH_H
#include "native_lock_witness.h"

// This guard covers registered mutation entries only. The host supplies their
// build-specific coverage and also holds the native worker/control locks.
struct NativeLifecycleEpoch {
    CRITICAL_SECTION lock;
    uint64_t generation;
    unsigned mutations;
};
struct NativeLifecyclePin {
    NativeLifecycleEpoch *epoch = nullptr;
    uint64_t generation = 0;
    DWORD thread = 0;
};
void native_lifecycle_init(NativeLifecycleEpoch *);
void native_lifecycle_mutation_enter(NativeLifecycleEpoch *);
void native_lifecycle_mutation_leave(NativeLifecycleEpoch *);
bool native_lifecycle_try_pin(NativeLifecycleEpoch *, uint64_t expected, NativeLifecyclePin *);
bool native_lifecycle_valid(const NativeLifecyclePin *);
bool native_lifecycle_release(NativeLifecyclePin *);
#endif
