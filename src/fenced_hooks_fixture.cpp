#include "fenced_restore.h"
#include "MinHook.h"
#include <cstdlib>
#define main baseline_fixture_main
extern "C" {
#include "restore_flow_fixture.c"
}
#undef main
static SimulationFence fence;
static RestoreFlowOps guarded;
static RestoreFlowReport result;
static unsigned dynamics_original_calls, score_original_calls, contact_original_calls,
    parent_continuation_writes;
static void(WINAPI *dynamics_original)(void *, double);
static void(WINAPI *score_original)(void *, float);
static void(WINAPI *contact_original)(void *, double, double, void *, double);
static unsigned char tire[4][16];
static bool request = true;
extern "C" __declspec(noinline) void WINAPI NativeContacts(void *object, double dt, double horizon,
                                                           void *world, double angular) {
    assert((object == tire[0] || object == tire[1] || object == tire[2] || object == tire[3]) &&
           dt == .001 && horizon == .003 && world == (void *)0x12345678 && angular == 2.5);
    ++contact_original_calls;
}
extern "C" __declspec(noinline) void WINAPI NativeScoring(void *object, float dt) {
    assert(object == score && dt == .125f);
    ++score_original_calls;
}
extern "C" __declspec(noinline) void WINAPI NativeVehicleUpdate(void *object, double dt) {
    assert(object == body && dt == .25);
    ++dynamics_original_calls;
    ++parent_continuation_writes;
}
static void WINAPI guarded_update(void *object, double dt) {
    FenceTicket ticket{};
    if (!simulation_fence_callback_enter(&fence, FENCE_BOUNDARY, (uintptr_t)object, &ticket))
        return;
    if (request) {
        request = false;
        restore_flow_execute(&flow, &workspace, &plan, &backend, &guarded, &result);
    }
    if (simulation_fence_may_advance(&ticket))
        dynamics_original(object, dt);
    simulation_fence_callback_leave(&ticket);
}
static void WINAPI guarded_scoring(void *object, float dt) {
    FenceTicket t{};
    if (!simulation_fence_callback_enter(&fence, FENCE_SCORING, (uintptr_t)object, &t))
        return;
    if (simulation_fence_may_advance(&t))
        score_original(object, dt);
    simulation_fence_callback_leave(&t);
}
static void WINAPI guarded_contacts(void *object, double dt, double horizon, void *world,
                                    double angular) {
    FenceTicket t{};
    if (!simulation_fence_callback_enter(&fence, FENCE_CONTACTS, (uintptr_t)object, &t))
        return;
    if (simulation_fence_may_advance(&t))
        contact_original(object, dt, horizon, world, angular);
    simulation_fence_callback_leave(&t);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    int fault = atoi(argv[1]);
    assert(fault >= 0 && fault <= 6);
    init();
    mode = fault;
    if (mode == 5)
        fail_write = 6;
    uintptr_t objects[] = {(uintptr_t)body, (uintptr_t)score, (uintptr_t)tire[0]};
    uintptr_t tires[] = {(uintptr_t)tire[0], (uintptr_t)tire[1], (uintptr_t)tire[2],
                         (uintptr_t)tire[3]};
    simulation_fence_init(&fence, 42, GetCurrentThreadId(), objects, tires, 4);
    FencedRestoreAdapter adapter{&fence, ops};
    guarded = fenced_restore_ops(&adapter);
    void *targets[] = {(void *)NativeVehicleUpdate, (void *)NativeScoring, (void *)NativeContacts};
    void *hooks[] = {(void *)guarded_update, (void *)guarded_scoring, (void *)guarded_contacts};
    void **trampolines[] = {(void **)&dynamics_original, (void **)&score_original,
                            (void **)&contact_original};
    unsigned char original[3][32];
    assert(MH_Initialize() == MH_OK);
    for (int i = 0; i < 3; i++) {
        memcpy(original[i], targets[i], 32);
        assert(MH_CreateHook(targets[i], hooks[i], trampolines[i]) == MH_OK);
    }
    assert(MH_EnableHook(MH_ALL_HOOKS) == MH_OK);
    void(WINAPI *volatile d)(void *, double) = NativeVehicleUpdate;
    void(WINAPI *volatile s)(void *, float) = NativeScoring;
    void(WINAPI *volatile c)(void *, double, double, void *, double) = NativeContacts;
    d(body, .25);
    bool fatal = fault >= 3 && fault <= 5;
    assert((result.result == FLOW_QUARANTINED) == fatal);
    assert(dynamics_original_calls == (fatal ? 0u : 1u));
    d(body, .25);
    s(score, .125f);
    for (auto &wheel : tire)
        c(wheel, .001, .003, (void *)0x12345678, 2.5);
    if (fatal) {
        assert(!dynamics_original_calls && !score_original_calls && !contact_original_calls &&
               !parent_continuation_writes);
        assert(!simulation_fence_stop(&fence));
        for (int i = 0; i < 3; i++)
            assert(memcmp(original[i], targets[i], 32));
        puts("PASS native hooks: fatal recovery suppresses current/future registered update, "
             "Score, Contacts; hooks remain installed until fixture process exit.");
    } else {
        assert(dynamics_original_calls == 2 && parent_continuation_writes == 2 &&
               score_original_calls == 1 && contact_original_calls == 4);
        assert(simulation_fence_stop(&fence));
        assert(MH_DisableHook(MH_ALL_HOOKS) == MH_OK);
        for (int i = 0; i < 3; i++)
            assert(!memcmp(original[i], targets[i], 32));
        puts("PASS native hooks: successful/recovered/rejected operation resumes; update/score and "
             "each of four tire calls forward exactly once; clean stop restores code.");
    }
    return 0;
}
