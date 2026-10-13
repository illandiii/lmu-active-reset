#include "active_reset_boundary.h"
#include "native_lock_witness.h"
BoundaryResetResult active_reset_boundary_dispatch(
    ResetNativeRuntime *runtime, SimulationFence *fence, uintptr_t key, uintptr_t held_native_lock,
    const ResetNativeAdapter *adapter, uint64_t now, const ResetIdentity *identity, int allowed,
    const ResetRequest *request, void(WINAPI *original)(uint64_t), uint64_t input) {
    BoundaryResetResult result{CONTROL_REJECTED, false};
    if (!runtime || !fence || !adapter || !original || !key || key != fence->object[FENCE_BOUNDARY])
        return result;
    if (!held_native_lock || !native_lock_witness(held_native_lock).held_by_caller)
        return result;
    const auto saves = runtime->control.saves, loads = runtime->control.loads;
    const auto completed = runtime->flow.completed, compensated = runtime->flow.compensated;
    FenceTicket ticket{};
    if (!simulation_fence_callback_enter(fence, FENCE_BOUNDARY, key, &ticket)) {
        result.control_result = GetCurrentThreadId() == fence->owner_thread ? CONTROL_QUARANTINED
                                                                            : CONTROL_WRONG_THREAD;
        return result;
    }
    if (ticket.fence) {
        // Freeze both adapters for the duration of this synchronous boundary.
        ResetNativeAdapter native = *adapter;
        FencedRestoreAdapter guarded{fence, native.flow, held_native_lock};
        native.flow = fenced_restore_ops(&guarded);
        result.control_result = active_reset_native_step(runtime, &native, GetCurrentThreadId(),
                                                         now, identity, allowed, request);
        // Also honor quarantine set by a previously completed control/flow path.
        if (runtime->control.quarantined || runtime->flow.quarantined)
            simulation_fence_quarantine(fence);
    }
    if (!native_lock_witness(held_native_lock).held_by_caller) {
        simulation_fence_quarantine(fence);
        runtime->flow.quarantined = 1;
        runtime->control.quarantined = 1;
        runtime->control.checkpoint_valid = 0;
        result.control_result = CONTROL_QUARANTINED;
        // The host lifetime may already have ended. Do not call into it again.
        // A successful transfer is not a completed reset until this boundary is safe.
        runtime->control.saves = saves;
        runtime->control.loads = loads;
        runtime->flow.completed = completed;
        runtime->flow.compensated = compensated;
        runtime->report.result = FLOW_QUARANTINED;
        runtime->report.cause = FLOW_NATIVE_LOCK_LOST;
    }
    if (simulation_fence_may_advance(&ticket)) {
        original(input);
        result.forwarded = true;
    }
    simulation_fence_callback_leave(&ticket);
    return result;
}
