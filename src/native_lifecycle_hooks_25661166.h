#ifndef LMU_NATIVE_LIFECYCLE_HOOKS_25661166_H
#define LMU_NATIVE_LIFECYCLE_HOOKS_25661166_H
#include "native_lifecycle_epoch.h"

// The caller initializes MinHook and enables/disables the complete hook set.
// Exactly one guard is supported per module, retained until process exit.
bool native_lifecycle_hooks_create(uintptr_t module_base, NativeLifecycleEpoch *);
#endif
