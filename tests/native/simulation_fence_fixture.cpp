#include "../../src/native/runtime/simulation_fence.h"
#include <cstdio>
#include <cstring>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
static SimulationFence fence;
static uintptr_t objects[] = {0x10000, 0x20000, 0x30000};
static DWORD WINAPI concurrent(void *) {
    FenceTicket t{};
    assert(!simulation_fence_callback_enter(&fence, FENCE_SCORING, objects[1], &t));
    assert(!t.admitted);
    assert(!simulation_fence_hold(&fence, 42));
    return 0;
}
static void init() {
    simulation_fence_init(&fence, 42, GetCurrentThreadId(), objects);
}
int main() {
    init();
    FenceTicket d{}, s{}, foreign{};
    assert(!simulation_fence_hold(&fence, 42));
    assert(simulation_fence_callback_enter(&fence, FENCE_BOUNDARY, objects[0], &d));
    assert(simulation_fence_callback_enter(&fence, FENCE_SCORING, objects[1], &s));
    assert(!simulation_fence_hold(&fence, 42));
    simulation_fence_callback_leave(&s);
    assert(!simulation_fence_hold(&fence, 43));
    assert(simulation_fence_hold(&fence, 42));
    assert(simulation_fence_owner_alive(&fence, 42));
    assert(!simulation_fence_stop(&fence));
    simulation_fence_release(&fence, true);
    simulation_fence_callback_leave(&d);
    assert(simulation_fence_stop(&fence));
    assert(!simulation_fence_callback_enter(&fence, FENCE_BOUNDARY, objects[0], &d));
    assert(simulation_fence_callback_enter(&fence, FENCE_BOUNDARY, 0x99999, &foreign) &&
           foreign.admitted);
    init();
    assert(simulation_fence_callback_enter(&fence, FENCE_BOUNDARY, objects[0], &d));
    assert(simulation_fence_hold(&fence, 42));
    simulation_fence_mark_dirty(&fence);
    HANDLE thread = CreateThread(nullptr, 0, concurrent, nullptr, 0, nullptr);
    assert(thread && WaitForSingleObject(thread, 1000) == WAIT_OBJECT_0);
    CloseHandle(thread);
    assert(!simulation_fence_owner_alive(&fence, 42));
    simulation_fence_release(&fence, true);
    assert(fence.mode == FENCE_QUARANTINED);
    assert(!simulation_fence_may_advance(&d));
    simulation_fence_callback_leave(&d);
    assert(!simulation_fence_stop(&fence));
    for (unsigned i = 0; i < FENCE_PHASES; i++)
        assert(!simulation_fence_callback_enter(&fence, i, objects[i], &s));
    assert(!simulation_fence_may_advance(&s));
    simulation_fence_release(&fence, true);
    assert(fence.mode == FENCE_QUARANTINED);
    init();
    assert(simulation_fence_callback_enter(&fence, FENCE_BOUNDARY, objects[0], &d));
    assert(!simulation_fence_stop(&fence));
    simulation_fence_callback_leave(&d);
    assert(simulation_fence_stop(&fence));
    uintptr_t tires[] = {0x31000, 0x32000, 0x33000, 0x34000};
    simulation_fence_init(&fence, 42, GetCurrentThreadId(), objects, tires, 4);
    for (auto tire : tires) {
        assert(simulation_fence_callback_enter(&fence, FENCE_CONTACTS, tire, &s) &&
               s.fence == &fence);
        simulation_fence_callback_leave(&s);
    }
    simulation_fence_quarantine(&fence);
    for (auto tire : tires)
        assert(!simulation_fence_callback_enter(&fence, FENCE_CONTACTS, tire, &s));
    assert(simulation_fence_callback_enter(&fence, FENCE_CONTACTS, 0x99000, &foreign) &&
           !foreign.fence);
    simulation_fence_callback_leave(&foreign);
    tires[3] = tires[0];
    simulation_fence_init(&fence, 42, GetCurrentThreadId(), objects, tires, 4);
    assert(fence.mode == FENCE_QUARANTINED && !simulation_fence_stop(&fence));
    assert(!simulation_fence_callback_enter(&fence, FENCE_CONTACTS, 0x99000, &foreign));
    puts("PASS registered-callback fence: exclusive host boundary, nested phase rejection, actual "
         "concurrent thread, permanent quarantine, all four tires, duplicate-tire refusal, no "
         "heartbeat-release, foreign pass-through and stop drain.");
}
