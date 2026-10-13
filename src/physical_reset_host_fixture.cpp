// Reuse the owned-memory vehicle model and recorded numerical frame, never LMU.
#define main adapter_reference_main
#include "vehicle_restore_adapter_fixture.cpp"
#undef main
#include "physical_reset_host_25661166.h"

static int host_owner;
static double host_time = 42;
static volatile LONG original_boundaries, lifecycle_calls;
extern "C" __declspec(dllexport) uintptr_t FixtureWorkerLock() {
    return reinterpret_cast<uintptr_t>(&game_lock);
}
extern "C" __declspec(dllexport) uintptr_t FixtureControlLock() {
    return reinterpret_cast<uintptr_t>(&control_lock);
}
extern "C" __declspec(dllexport) uintptr_t FixtureGate() {
    return reinterpret_cast<uintptr_t>(&gate_lock);
}
extern "C" __declspec(dllexport) bool FixtureHostRoots(uintptr_t *owner, uintptr_t *physics,
                                                       uintptr_t *scoring, uintptr_t *tires) {
    *owner = reinterpret_cast<uintptr_t>(&host_owner);
    *physics = source_bindings.address[104];
    *scoring = reinterpret_cast<uintptr_t>(score);
    for (unsigned i = 0; i < 4; ++i)
        tires[i] = reinterpret_cast<uintptr_t>(tire[i]);
    return true;
}
extern "C" __declspec(dllexport) bool FixtureHostRead(uintptr_t a, uintptr_t b, uint64_t epoch,
                                                      double time, SnapshotFrame *frame,
                                                      RestoreBindings *bindings) {
    return fixture_read(a, b, epoch, time, frame, bindings);
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI
FixtureContacts(void *p, double dt, double horizon, void *world, double angular) {
    assert(reinterpret_cast<uintptr_t>(world) ==
           reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) + 0x1aa4090);
    FixtureRefresh(p, dt, horizon, reinterpret_cast<void *>(0x12345678), angular);
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureScore(void *p, float dt) {
    assert(p == score && dt == .0025f);
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureDynamics(void *p,
                                                                                  double time) {
    assert(reinterpret_cast<uintptr_t>(p) == source_bindings.address[104] && time == host_time);
    void(WINAPI *volatile contacts)(void *, double, double, void *, double) = FixtureContacts;
    for (auto &t : tire)
        contacts(t, .001, .003,
                 reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) +
                                          0x1aa4090),
                 2.5);
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureBoundary(uint64_t value) {
    assert(value == 77);
    // The host must drop its additional lifetime scopes before the native update.
    assert(!native_lock_witness(reinterpret_cast<uintptr_t>(&control_lock)).held_by_caller);
    InterlockedIncrement(&original_boundaries);
    void(WINAPI *volatile score_fn)(void *, float) = FixtureScore;
    void(WINAPI *volatile dynamics_fn)(void *, double) = FixtureDynamics;
    score_fn(score, .0025f);
    dynamics_fn(reinterpret_cast<void *>(source_bindings.address[104]), host_time);
    host_time += .0025;
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureClear(void *,
                                                                               unsigned char) {
    InterlockedIncrement(&lifecycle_calls);
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureReconstruct(void *) {
    InterlockedIncrement(&lifecycle_calls);
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureDestroy(void *) {
    InterlockedIncrement(&lifecycle_calls);
}
extern "C" __declspec(dllexport) __declspec(noinline) void *WINAPI FixtureConstruct(void *p) {
    InterlockedIncrement(&lifecycle_calls);
    return p;
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureVehicleDestroy(void *) {
    InterlockedIncrement(&lifecycle_calls);
}
extern "C" __declspec(dllexport) __declspec(noinline) void *WINAPI
FixtureVehicleConstruct(void *p) {
    InterlockedIncrement(&lifecycle_calls);
    return p;
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureControl(void *, int, char,
                                                                                 char) {
    InterlockedIncrement(&lifecycle_calls);
}
static void tick(ResetHostIPC *ipc) {
    ipc->heartbeat_ms = GetTickCount64();
    EnterCriticalSection(&game_lock);
    void(WINAPI *volatile update)(uint64_t) = FixtureBoundary;
    update(77);
    LeaveCriticalSection(&game_lock);
    Sleep(1);
}
static ResetHostReport request(ResetHostIPC *ipc, uint64_t id, unsigned action) {
    ipc->request_action = action;
    ipc->request_tick = GetTickCount64();
    InterlockedExchange64(&ipc->request_id, id);
    const auto deadline = GetTickCount64() + 2500;
    while (ipc->completed_id != static_cast<LONG64>(id) && GetTickCount64() < deadline)
        tick(ipc);
    assert(ipc->completed_id == static_cast<LONG64>(id));
    return ipc->report;
}
int main(int argc, char **argv) {
    assert(argc == 3);
    load_frame(argv[2]);
    for (double &v : source_frame.bodies[0].velocity_world)
        v = 0;
    auto memory = std::make_unique<unsigned char[]>(108 * 0x5000 + 2592 * 0x110);
    for (unsigned i = 0; i < RESTORE_OBJECT_COUNT; ++i)
        source_bindings.address[i] = reinterpret_cast<uintptr_t>(memory.get()) +
                                     (i < 108 ? i * 0x5000 : 108 * 0x5000 + (i - 108) * 0x110);
    source_bindings.body_count = source_frame.total_count;
    source_bindings.constraint_count = source_frame.constraint_count;
    source_bindings.engine_channels = source_frame.integration.channel_count;
    for (unsigned i = 0; i < source_frame.constraint_count; ++i)
        source_bindings.constraint_kind[i] = source_frame.recipes[i].kind;
    init();
    for (unsigned i = 0; i < 4; ++i)
        source_bindings.address[96 + i] = reinterpret_cast<uintptr_t>(tire[i]);
    for (unsigned offset : {0x160u, 0x27290u, 0x27360u})
        put(score + offset, int32_t{2});
    InitializeCriticalSection(&game_lock);
    InitializeCriticalSection(&control_lock);
    InitializeCriticalSection(&gate_lock);
    wchar_t name[96];
    swprintf(name, 96, L"Local\\LMU_PhysicalReset25661166_v%u_%lu",
             static_cast<unsigned>(LMU_RESET_HOST_REVISION), GetCurrentProcessId());
    HANDLE map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                    sizeof(ResetHostIPC), name);
    assert(map && GetLastError() != ERROR_ALREADY_EXISTS);
    auto *ipc = static_cast<ResetHostIPC *>(
        MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ResetHostIPC)));
    assert(ipc);
    *ipc = {};
    ipc->magic = RESET_HOST_MAGIC;
    ipc->version = 1;
    ipc->pid = GetCurrentProcessId();
    ipc->owner_thread = GetCurrentThreadId();
    ipc->heartbeat_ms = GetTickCount64();
    ipc->allow = 1;
    assert(LoadLibraryA(argv[1]));
    for (unsigned i = 0; i < 200 && ipc->status < 2; ++i) {
        ipc->heartbeat_ms = GetTickCount64();
        Sleep(10);
    }
    assert(ipc->status == 2);
    tick(ipc);
    auto saved = request(ipc, 1, HOST_SAVE);
    std::printf("host saved result=%llu saves=%llu readable=%llu\n", saved.result, saved.saves,
                saved.readable);
    assert(saved.result == CONTROL_OK && saved.saves == 1 && !saved.forward_writes);
    auto plan = request(ipc, 2, HOST_PREPARE);
    assert(plan.result == 0 && plan.fields == 10077 && !plan.forward_writes);
    auto denied = request(ipc, 3, HOST_RESTORE);
    assert(denied.result == CONTROL_REJECTED && denied.loads == 0 && !denied.forward_writes);
    ipc->permit_writes = 1;
    auto restored = request(ipc, 4, HOST_RESTORE);
    std::printf("host restore result=%llu writes=%llu fields=%llu contacts=%llu\n", restored.result,
                restored.forward_writes, restored.fields, restored.contact_calls);
    assert(restored.result == CONTROL_OK && restored.loads == 1 &&
           restored.forward_writes == 10077 && restored.contact_mask == 15);
    assert(restored.verified_fields == 10077 && restored.mismatched_fields == 0);
    assert(ipc->physical_field_write_attempts == 10077);
    for (unsigned i = 0; i < 3; ++i)
        assert(restored.after_position[i] == saved.saved_position[i]);
    void(WINAPI *volatile reconstruct)(void *) = FixtureReconstruct;
    reconstruct(reinterpret_cast<void *>(source_bindings.address[104]));
    auto stale = request(ipc, 5, HOST_PREPARE);
    assert(stale.result == 1001 && stale.loads == 1);
    ipc->stop = 1;
    for (unsigned i = 0; i < 500 && ipc->status == 2; ++i)
        Sleep(10);
    assert(ipc->status == 3 && !ipc->error && original_boundaries > 0);
    puts("PASS loaded physical host: save, prepare, write opt-in, full 10077-field restore, four "
         "contacts, native lock release before update, lifecycle invalidation and clean stop. "
         "Owned fixture memory only.");
}
