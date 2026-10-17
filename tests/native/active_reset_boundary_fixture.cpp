#include "../../src/native/runtime/active_reset_boundary.h"
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "../../src/native/runtime/native_lock_witness.h"
static CRITICAL_SECTION game_lock;
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
static ResetNativeRuntime runtime;
static SimulationFence fence;
static unsigned char body[256], score[0x27400];
static double saved;
static bool held, alive;
static int mode, original_calls, contact_calls, quarantines, prepared, context_calls;
static const uint64_t input = 0x1234567887654321ULL;
static void WINAPI original(uint64_t v) {
    assert(v == input && !held);
    ++original_calls;
}
static int capture(void *, const ResetIdentity *) {
    memcpy(&saved, body + 8, 8);
    return 0;
}
static int prepare(void *, const ResetIdentity *, RestorePlan *p, RestoreBindings *b,
                   uintptr_t *s) {
    ++prepared;
    memset(p, 0, sizeof(*p));
    memset(b, 0, sizeof(*b));
    p->epoch = b->epoch = 42;
    p->count = 4;
    b->body_count = 1;
    b->address[0] = (uintptr_t)body;
    *s = (uintptr_t)score;
    auto &f = p->fields[0];
    f.field_offset = 8;
    f.size = 8;
    memcpy(f.expected, body + 8, 8);
    memcpy(f.desired, &saved, 8);
    const unsigned offsets[] = {0x160, 0x27290, 0x27360};
    for (unsigned i = 0; i < 3; i++) {
        auto &g = p->fields[i + 1];
        g.object_slot = RESTORE_OBJECT_COUNT;
        g.field_offset = offsets[i];
        g.size = 4;
        memcpy(g.expected, score + offsets[i], 4);
    }
    return 0;
}
static int context(void *, uint64_t e) {
    ++context_calls;
    if (mode == 4 && context_calls == 4)
        LeaveCriticalSection(&game_lock);
    if (mode == 3 && context_calls == 4) {
        alive = false;
        return 0;
    }
    return held && alive && e == 42;
}
static int enter(void *, uint64_t e) {
    held = e == 42;
    return held;
}
static int owner(void *, uint64_t e) {
    return held && alive && e == 42;
}
static int contacts(void *, uint64_t, int old) {
    ++contact_calls;
    if (!old && (mode == 1 || mode == 2)) {
        double changed = 99;
        memcpy(body + 8, &changed, 8);
        return 0;
    }
    return !(old && mode == 2);
}
static void leave(void *, int resume) {
    assert(held);
    held = false;
    if (mode == 2 || mode == 3 || mode == 4)
        assert(!resume);
    if (mode == 5)
        LeaveCriticalSection(&game_lock);
    if (mode == 6)
        simulation_fence_quarantine(&fence);
}
static void quarantine(void *, int) {
    ++quarantines;
}
static ResetIdentity identity{11, 22, 33, 44};
static ResetNativeAdapter adapter{
    nullptr, capture, prepare, context, {nullptr, enter, owner, contacts, leave, quarantine}};
static BoundaryResetResult dispatch(uint64_t id, unsigned action, uint64_t issued = 1000,
                                    uint64_t now = 1000) {
    ResetRequest r{id, issued, identity, action};
    EnterCriticalSection(&game_lock);
    auto result = active_reset_boundary_dispatch(&runtime, &fence, 0x1234, (uintptr_t)&game_lock,
                                                 &adapter, now, &identity, 1, &r, original, input);
    if (native_lock_witness((uintptr_t)&game_lock).held_by_caller)
        LeaveCriticalSection(&game_lock);
    return result;
}
static void init(int fault) {
    memset(&runtime, 0, sizeof(runtime));
    memset(body, 0, sizeof(body));
    memset(score, 0, sizeof(score));
    double x = 1;
    memcpy(body + 8, &x, 8);
    int flag = 2;
    for (unsigned off : {0x160u, 0x27290u, 0x27360u})
        memcpy(score + off, &flag, 4);
    runtime.control.owner_thread = GetCurrentThreadId();
    uintptr_t objects[] = {0x1234, (uintptr_t)score, 0x9876};
    simulation_fence_init(&fence, 42, GetCurrentThreadId(), objects);
    held = false;
    alive = true;
    mode = fault;
    original_calls = contact_calls = quarantines = prepared = context_calls = 0;
}
int main() {
    InitializeCriticalSection(&game_lock);
    for (int fault = 0; fault <= 6; fault++) {
        init(fault);
        auto r = dispatch(1, RESET_SAVE);
        assert(r.control_result == CONTROL_OK && r.forwarded && runtime.control.saves == 1);
        double x = 2;
        memcpy(body + 8, &x, 8);
        r = dispatch(2, RESET_LOAD);
        if (fault < 2) {
            assert(r.forwarded && original_calls == 2 &&
                   r.control_result == (fault ? CONTROL_RESTORE_RECOVERED : CONTROL_OK));
            memcpy(&x, body + 8, 8);
            assert(x == (fault ? 2 : 1));
            assert(!quarantines && simulation_fence_stop(&fence));
        } else {
            // Fault 5 ends the host lifetime before losing the lock: quarantine
            // belongs to the outer dispatcher, which must not reenter the host.
            assert(!r.forwarded && r.control_result == CONTROL_QUARANTINED &&
                   quarantines == (fault >= 5 ? 0 : 1) && original_calls == 1);
            assert(runtime.flow.quarantined && runtime.control.quarantined &&
                   !runtime.control.checkpoint_valid);
            assert(!runtime.control.loads && !runtime.flow.completed && !runtime.flow.compensated);
            if (fault == 4 || fault == 5)
                assert(runtime.report.result == FLOW_QUARANTINED &&
                       runtime.report.cause == FLOW_NATIVE_LOCK_LOST);
            if (fault == 6)
                assert(runtime.report.cause == FLOW_OWNER_LOST);
            int prior = prepared;
            r = dispatch(3, RESET_LOAD);
            assert(!r.forwarded && prepared == prior && !simulation_fence_stop(&fence));
            r = active_reset_boundary_dispatch(&runtime, &fence, 0x9999, (uintptr_t)&game_lock,
                                               &adapter, 1000, &identity, 1, nullptr, original,
                                               input);
            assert(!r.forwarded && original_calls == 1);
        }
    }
    init(0);
    auto missing =
        active_reset_boundary_dispatch(&runtime, &fence, 0x1234, (uintptr_t)&game_lock, &adapter,
                                       1000, &identity, 1, nullptr, original, input);
    assert(!missing.forwarded && missing.control_result == CONTROL_REJECTED && !prepared &&
           !original_calls);
    assert(dispatch(1, RESET_LOAD).control_result == CONTROL_NO_CHECKPOINT && !prepared);
    assert(dispatch(2, RESET_SAVE).control_result == CONTROL_OK);
    assert(dispatch(3, RESET_LOAD, 1000, 2501).control_result == CONTROL_REJECTED && !prepared);
    assert(dispatch(3, RESET_LOAD).control_result == CONTROL_REJECTED && !prepared);
    ++identity.session;
    assert(dispatch(4, RESET_LOAD).control_result == CONTROL_NO_CHECKPOINT && !prepared);
    puts("PASS outer boundary -> command -> native plan -> fenced flow -> contacts; "
         "success/compensation forward once; lost owner/fatal recovery suppress current and future "
         "originals; stale, duplicate and changed-session requests refused. Owned memory only.");
}
