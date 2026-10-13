#ifndef LMU_ACTIVE_RESET_BOUNDARY_H
#define LMU_ACTIVE_RESET_BOUNDARY_H
#include "fenced_restore.h"
extern "C" {
#include "active_reset_native.h"
}
struct BoundaryResetResult {
    int control_result;
    bool forwarded;
};
/* Called from the enclosing update hook, BEFORE its original. The host must
 * establish native lifetime ownership and install the companion phase guards.
 * This module does not infer a pin from callback/thread observations.
 * The boundary key is a stable host identity, not the original's input value. */
BoundaryResetResult
active_reset_boundary_dispatch(ResetNativeRuntime *, SimulationFence *, uintptr_t boundary_key,
                               uintptr_t held_native_lock, const ResetNativeAdapter *, uint64_t now,
                               const ResetIdentity *, int allowed, const ResetRequest *,
                               void(WINAPI *original)(uint64_t), uint64_t input);
#endif
