#include "fenced_restore.h"
/* Reuse the full transaction fault matrix with a real callback fence. */
#define main baseline_fixture_main
extern "C" {
#include "restore_flow_fixture.c"
}
#undef main
static SimulationFence fence;
static int run_fenced(int fault) {
    init();
    mode = fault;
    if (fault == 5)
        fail_write = 6;
    uintptr_t objects[3] = {0x10000, 0x20000, 0x30000};
    simulation_fence_init(&fence, 42, GetCurrentThreadId(), objects);
    FenceTicket current{};
    assert(simulation_fence_callback_enter(&fence, FENCE_BOUNDARY, objects[0], &current));
    RestoreFlowOps native_ops = {nullptr, enter, owner, contacts, leave, quarantine};
    FencedRestoreAdapter adapter{&fence, native_ops};
    auto guarded = fenced_restore_ops(&adapter);
    RestoreFlowReport report{};
    int result = restore_flow_execute(&flow, &workspace, &plan, &backend, &guarded, &report);
    bool fatal = result == FLOW_QUARANTINED;
    assert(simulation_fence_may_advance(&current) == !fatal);
    simulation_fence_callback_leave(&current);
    assert(simulation_fence_stop(&fence) == !fatal);
    if (fatal) {
        for (unsigned phase = 0; phase < 3; phase++) {
            FenceTicket next{};
            assert(!simulation_fence_callback_enter(&fence, phase, objects[phase], &next));
            assert(!simulation_fence_may_advance(&next));
        }
        assert(fence.mode == FENCE_QUARANTINED);
    }
    return result;
}
int main() {
    assert(!baseline_fixture_main());
    assert(run_fenced(0) == FLOW_OK);
    for (int fault = 1; fault <= 2; fault++)
        assert(run_fenced(fault) == FLOW_COMPENSATED);
    for (int fault = 3; fault <= 5; fault++)
        assert(run_fenced(fault) == FLOW_QUARANTINED);
    assert(run_fenced(6) == FLOW_REJECTED);
    puts("PASS fence wired to restore flow: success/recovered resume; owner loss, contact recovery "
         "failure and rollback failure suppress CURRENT and FUTURE native callbacks; detach "
         "refused after quarantine.");
    return 0;
}
