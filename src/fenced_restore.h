#ifndef LMU_FENCED_RESTORE_H
#define LMU_FENCED_RESTORE_H
#include "simulation_fence.h"
extern "C" {
#include "restore_flow.h"
}
/* lifetime.enter/owner_alive remain authoritative host obligations. A callback
   fence alone does NOT pin native allocations or validate session identity. */
struct FencedRestoreAdapter {
    SimulationFence *fence;
    RestoreFlowOps lifetime;
    uintptr_t native_lock = 0;
};
RestoreFlowOps fenced_restore_ops(FencedRestoreAdapter *);
#endif
