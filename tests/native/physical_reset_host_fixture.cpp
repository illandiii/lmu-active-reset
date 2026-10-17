// Reuse the owned-memory vehicle model and recorded numerical frame, never LMU.
#define main adapter_reference_main
#include "vehicle_restore_adapter_fixture.cpp"
#undef main
#include "../../src/native/runtime/physical_reset_host.h"
#include "../../src/native/runtime/physical_reset_trace.h"

static unsigned char host_owner[0x18000];
static double host_time = 42;
static volatile LONG original_boundaries, lifecycle_calls;
static int mismatched_reads;
static bool suppress_player_callback;
static bool inject_collision_jump;
static BodyworkFixtureModel host_bodywork;
static volatile LONG graphics_calls;
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureGraphicsClear(void *) {
    InterlockedIncrement(&graphics_calls);
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureGraphicsLoad(void *) {
    InterlockedIncrement(&graphics_calls);
}
extern "C" __declspec(dllexport) __declspec(noinline) void *WINAPI
FixtureGraphicsConstruct(void *p) {
    InterlockedIncrement(&graphics_calls);
    return p;
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureGraphicsDestroy(void *) {
    InterlockedIncrement(&graphics_calls);
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureGraphicsPublish(void *,
                                                                                         void *) {
    InterlockedIncrement(&graphics_calls);
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI
FixtureGraphicsComponentClear(void *) {
    InterlockedIncrement(&graphics_calls);
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI
FixtureGraphicsComponentLoad(void *, void *, void *, void *) {
    InterlockedIncrement(&graphics_calls);
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureGraphicsImpact(void *,
                                                                                        float,
                                                                                        void *,
                                                                                        void *) {
    InterlockedIncrement(&graphics_calls);
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureGraphicsRepair(void *) {
    InterlockedIncrement(&graphics_calls);
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI
FixtureGraphicsReset(void *, unsigned char) {
    InterlockedIncrement(&graphics_calls);
}
static unsigned correction_calls, collision_prepare_calls;
#if LMU_RESET_HOST_REVISION >= 15
static unsigned collision_rebuild_calls, spatial_calls, removed_pairs;
static double largest_restored_sweep;
static bool monitor_restored_sweep;
static unsigned expected_fields;
#else
static constexpr unsigned expected_fields = 10077;
#endif
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
    if (mismatched_reads) {
        if (mismatched_reads > 0)
            --mismatched_reads;
        *frame = {};
        frame->binding_status = 90205;
        *bindings = {};
        return false;
    }
    return fixture_read(a, b, epoch, time, frame, bindings);
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI
FixtureContacts(void *p, double dt, double horizon, void *world, double angular) {
    assert(reinterpret_cast<uintptr_t>(world) ==
           reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) + 0x1aa4090);
    FixtureRefresh(p, dt, horizon, reinterpret_cast<void *>(0x12345678), angular);
#if LMU_RESET_HOST_REVISION >= 15
    if (native_lock_witness(reinterpret_cast<uintptr_t>(&control_lock)).held_by_caller)
        host_owner[8 + 0x16cfa] = 1; // Model an obsolete pending collision at reset.
#endif
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureScore(void *p, float dt) {
    assert(p == score && dt == .0025f);
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureCollisionCorrect(void *p) {
    assert(p == host_owner + 8);
    ++correction_calls;
    if (inject_collision_jump &&
#if LMU_RESET_HOST_REVISION >= 15
        host_owner[8 + 0x16cfa] &&
#endif
        *reinterpret_cast<double *>(source_bindings.address[0] + 0x28) == 80) {
        inject_collision_jump = false;
        auto *position = reinterpret_cast<double *>(source_bindings.address[0] + 0x40);
        *position += 11;
    }
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureCollisionPrepare(void *p) {
    assert(p == host_owner + 8);
    ++collision_prepare_calls;
    host_owner[8 + 0x16cfa] = 0;
#if LMU_RESET_HOST_REVISION >= 15
    const auto *exported = reinterpret_cast<const float *>(host_owner + 8 + 0x159e8);
    for (unsigned i = 0; i < 3; ++i) {
        put(host_owner + 8 + 0x16d40 + i * 8, double{exported[9 + i]});
        put(host_owner + 8 + 0x16d28 + i * 8, double{exported[15 + i]});
    }
    for (unsigned i = 0; i < 9; ++i)
        put(host_owner + 8 + 0x16d90 + i * 8, double{exported[i]});
#endif
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureCollisionSpatial(void *p) {
    assert(p == host_owner + 8);
#if LMU_RESET_HOST_REVISION >= 15
    assert(native_lock_witness(reinterpret_cast<uintptr_t>(&game_lock)).held_by_caller);
    assert(native_lock_witness(reinterpret_cast<uintptr_t>(&control_lock)).held_by_caller);
    assert(get<uintptr_t>(host_owner + 8 + 0x17b28) == 0);
    assert(host_owner[8 + 0x16cfa] == 1);
    put(host_owner + 8 + 0x17b28, uintptr_t{0x12345678});
    ++spatial_calls;
#endif
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI
FixtureCollisionRemovePairs(void *, void *p) {
    assert(p == host_owner + 8);
#if LMU_RESET_HOST_REVISION >= 15
    assert(native_lock_witness(reinterpret_cast<uintptr_t>(&control_lock)).held_by_caller);
    assert(host_owner[8 + 0x16cfa] == 0);
    assert(get<double>(host_owner + 8 + 0x16d28) == 80);
    ++removed_pairs;
    ++collision_rebuild_calls;
    monitor_restored_sweep = true;
#endif
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
    void(WINAPI *volatile correct_fn)(void *) = FixtureCollisionCorrect;
    void(WINAPI *volatile prepare_fn)(void *) = FixtureCollisionPrepare;
#if LMU_RESET_HOST_REVISION < 15
    host_owner[8 + 0x16cfa] = 1;
#endif
    correct_fn(host_owner + 8);
    void(WINAPI *volatile score_fn)(void *, float) = FixtureScore;
    void(WINAPI *volatile dynamics_fn)(void *, double) = FixtureDynamics;
    score_fn(score, .0025f);
    if (!suppress_player_callback)
        dynamics_fn(reinterpret_cast<void *>(source_bindings.address[104]), host_time);
#if LMU_RESET_HOST_REVISION >= 15
    // Native export precedes the broad-phase's old/new shape sweep. A missing
    // reset of the collision proxy would connect positions 1000 m apart here.
    const double previous = get<double>(host_owner + 8 + 0x16d40);
    const double current =
        get<double>(reinterpret_cast<unsigned char *>(source_bindings.address[0] + 0x40));
    put(host_owner + 8 + 0x15a0c, static_cast<float>(current));
    if (monitor_restored_sweep) {
        const double sweep = std::fabs(previous - current);
        if (sweep > largest_restored_sweep)
            largest_restored_sweep = sweep;
    }
#endif
    prepare_fn(host_owner + 8);
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
static int run_fixture(int argc, char **argv) {
    assert(argc == 3 || argc == 4);
    original_boundaries = 0;
    correction_calls = collision_prepare_calls = 0;
    suppress_player_callback = argc == 4 && std::strcmp(argv[3], "no-player-callback") == 0;
    load_frame(argv[2]);
    // Reproduce the reader bug: sectors have advanced but applied body caches
    // have not. The original validator rejects it; the cache-aware one accepts
    // it without changing either the captured sectors or the native caches.
    RigidMassProperties mass[32];
    RigidPreparedBody prepared[32];
    FuelPrepared fuel;
    for (size_t i = 0; i < source_frame.total_count; ++i)
        mass[i] = source_frame.bodies[i].mass;
    auto &dependencies = source_frame.dependencies;
    dependencies.tires[0].sectors[0][0] += .001;
    assert(mechanical_prepare(source_frame.bodies, mass, source_frame.total_count,
                              &source_frame.links, &dependencies, &dependencies, prepared,
                              &fuel) == MECHANICAL_BODY_MISMATCH);
    auto validate = [] {
        return checkpoint_validate_cached_mechanical(
            source_frame.bodies, source_frame.total_count, &source_frame.links,
            &source_frame.dependencies, source_frame.tire_state, source_frame.tire_mass_cache);
    };
    const double advanced_sector = dependencies.tires[0].sectors[0][0];
    assert(validate() == 0 && dependencies.tires[0].sectors[0][0] == advanced_sector);
    const double cached_inertia = source_frame.tire_state[0].cached_sector_inertia[0];
    source_frame.tire_state[0].cached_sector_inertia[0] += .01;
    assert(validate() != 0);
    source_frame.tire_state[0].cached_sector_inertia[0] = cached_inertia;
    const double cached_mass = source_frame.tire_mass_cache[0];
    source_frame.tire_mass_cache[0] *= 1.01;
    assert(validate() != 0);
    source_frame.tire_mass_cache[0] = cached_mass;
    source_frame.bodies[0].velocity_world[0] = 80;
    auto memory = std::make_unique<unsigned char[]>(108 * 0x5000 + 2593 * 0x110);
    for (unsigned i = 0; i < RESTORE_OBJECT_COUNT; ++i)
        source_bindings.address[i] = reinterpret_cast<uintptr_t>(memory.get()) +
                                     (i < 108 ? i * 0x5000 : 108 * 0x5000 + (i - 108) * 0x110);
    source_bindings.body_count = source_frame.total_count;
    source_bindings.constraint_count = source_frame.constraint_count;
    source_bindings.engine_channels = source_frame.integration.channel_count;
#if LMU_RESET_HOST_REVISION >= 15
    collision_rebuild_calls = spatial_calls = removed_pairs = 0;
    largest_restored_sweep = 0;
    monitor_restored_sweep = false;
    std::memset(host_owner, 0, sizeof(host_owner));
    host_bodywork.bind(host_owner + 0x38);
    auto *physical = reinterpret_cast<unsigned char *>(source_bindings.address[104]);
    auto *body = reinterpret_cast<unsigned char *>(source_bindings.address[0]);
    for (unsigned i = 0; i < 3; ++i) {
        put(body + 0x40 + i * 8, source_frame.bodies[0].position_world[i]);
        put(body + 0x28 + i * 8, source_frame.bodies[0].velocity_world[i]);
        put(physical + 0x24b0 + i * 8, source_frame.bodies[0].position_world[i]);
        put(physical + 0x2498 + i * 8, source_frame.bodies[0].velocity_world[i]);
        put(host_owner + 8 + 0x15a0c + i * 4, float(source_frame.bodies[0].position_world[i]));
        put(host_owner + 8 + 0x15a24 + i * 4, float(source_frame.bodies[0].velocity_world[i]));
    }
    for (unsigned i = 0; i < 9; ++i) {
        put(physical + 0x2500 + i * 8, double(i % 4 == 0));
        put(host_owner + 8 + 0x159e8 + i * 4, float(i % 4 == 0));
    }
    put(host_owner + 8 + 0x15a50, float{1250});
#endif
    for (unsigned i = 0; i < source_frame.constraint_count; ++i)
        source_bindings.constraint_kind[i] = source_frame.recipes[i].kind;
    expected_fields = 10093 + initialize_hinge_axes() + AERO_FIELD_COUNT;
    write_fixture_aero(fixture_aero(false));
    init();
    for (unsigned i = 0; i < 4; ++i)
        source_bindings.address[96 + i] = reinterpret_cast<uintptr_t>(tire[i]);
    for (unsigned offset : {0x160u, 0x27290u, 0x27360u})
        put(score + offset, int32_t{2});
    InitializeCriticalSection(&game_lock);
    InitializeCriticalSection(&control_lock);
    InitializeCriticalSection(&gate_lock);
    wchar_t name[512];
#if LMU_RESET_HOST_REVISION >= 9
    wchar_t path[MAX_PATH];
    assert(MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, path, MAX_PATH));
    const wchar_t *back = wcsrchr(path, L'\\'), *forward = wcsrchr(path, L'/');
    const wchar_t *filename = back;
    if (!filename || (forward && forward > filename))
        filename = forward;
    swprintf(name, 512, L"Local\\LMU_PhysicalReset_%ls_%lu", filename ? filename + 1 : path,
             GetCurrentProcessId());
#else
    swprintf(name, 96, L"Local\\LMU_PhysicalReset25661166_v%u_%lu",
             static_cast<unsigned>(LMU_RESET_HOST_REVISION), GetCurrentProcessId());
#endif
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
    ipc->owner_thread = 0; // Resolve it from the actual nested callback.
    ipc->heartbeat_ms = GetTickCount64();
    ipc->allow = 1;
    unsigned char outer_prolog[16];
    std::memcpy(outer_prolog, reinterpret_cast<void *>(FixtureBoundary), sizeof(outer_prolog));
    assert(LoadLibraryA(argv[1]));
    for (unsigned i = 0; i < 4000 && ipc->status < 2; ++i) {
        ipc->heartbeat_ms = GetTickCount64();
        tick(ipc);
    }
    if (suppress_player_callback) {
        assert(ipc->status == 4 && ipc->error == 112 && !ipc->owner_thread &&
               !ipc->physical_field_write_attempts);
        assert(std::memcmp(outer_prolog, reinterpret_cast<void *>(FixtureBoundary),
                           sizeof(outer_prolog)) == 0);
        puts("PASS absent player callback: no guessed owner, no physical writes, hook removed.");
        return 0;
    }
    assert(ipc->status == 2);
    assert(ipc->owner_thread == GetCurrentThreadId());
    tick(ipc);
    auto saved = request(ipc, 1, HOST_SAVE);
    std::printf("host saved result=%llu saves=%llu readable=%llu\n", saved.result, saved.saves,
                saved.readable);
    assert(saved.result == CONTROL_OK && saved.saves == 1 && !saved.forward_writes);
    assert(saved.speed >= 80);
    host_bodywork.damage();
    for (size_t i = 0; i < source_frame.total_count; ++i)
        source_frame.bodies[i].position_world[0] += 1000;
    source_frame.bodies[0].velocity_world[0] = -100;
#if LMU_RESET_HOST_REVISION >= 15
    put(body + 0x40, source_frame.bodies[0].position_world[0]);
    put(body + 0x28, double{-100});
    put(physical + 0x24b0, source_frame.bodies[0].position_world[0]);
    put(physical + 0x2498, double{-100});
    put(host_owner + 8 + 0x15a24, float{-100});
    put(host_owner + 8 + 0x15a50, float{1240});
    put(physical + 0x4bc8, double{56940284});
    const float turned[9]{0, 0, 1, 0, 1, 0, -1, 0, 0};
    std::memcpy(host_owner + 8 + 0x159e8, turned, sizeof(turned));
    inject_collision_jump = true;
#endif
    auto plan = request(ipc, 2, HOST_PREPARE);
    assert(plan.result == 0 && plan.fields == expected_fields && !plan.forward_writes);
    assert(plan.verified_fields == plan.fields && !plan.mismatched_fields);
    assert(plan.pins == 2);
    auto denied = request(ipc, 3, HOST_RESTORE);
    assert(denied.result == CONTROL_REJECTED && denied.loads == 0 && !denied.forward_writes);
    ipc->permit_writes = 1;
#if LMU_RESET_HOST_REVISION >= 12 && LMU_RESET_HOST_REVISION < 15
    inject_collision_jump = true;
    host_owner[8 + 0x16cfa] = 1;
    put(host_owner + 8 + 0x16d40, double{314});
    put(host_owner + 8 + 0x15a0c, float{271});
#endif
    auto restored = request(ipc, 4, HOST_RESTORE);
    std::printf("host restore result=%llu writes=%llu fields=%llu contacts=%llu\n", restored.result,
                restored.forward_writes, restored.fields, restored.contact_calls);
    assert(restored.result == CONTROL_OK && restored.loads == 1 &&
           restored.forward_writes == expected_fields && restored.contact_mask == 15);
    assert(restored.verified_fields == expected_fields && restored.mismatched_fields == 0);
#if LMU_RESET_HOST_REVISION >= 21
    assert(host_bodywork.restored() && ipc->bodywork_result == 0 && ipc->bodywork_meshes == 1);
    assert(ipc->bodywork_vertex_bytes == 176 && ipc->bodywork_attempted_writes > 0 &&
           ipc->bodywork_attempted_writes == ipc->bodywork_verified_writes);
#endif
#if LMU_RESET_HOST_REVISION >= 15
    assert(collision_rebuild_calls == 1 && spatial_calls == 1 && removed_pairs == 1);
    assert(largest_restored_sweep < .001);
    assert(get<double>(body + 0x28) == 80 && get<double>(physical + 0x2498) == 80);
    assert(get<float>(host_owner + 8 + 0x15a50) == 1250);
    assert(get<double>(physical + 0x4bc8) == 0);
    assert(get<double>(body + 0x40) == saved.saved_position[0]);
    for (unsigned i = 0; i < 9; ++i)
        assert(get<double>(host_owner + 8 + 0x16d90 + i * 8) == double(i % 4 == 0));
#endif
#if LMU_RESET_HOST_REVISION >= 10
    wchar_t trace_name[520];
    swprintf(trace_name, 520, L"%ls_trace", name);
    HANDLE trace_map = OpenFileMappingW(FILE_MAP_READ, FALSE, trace_name);
    assert(trace_map);
    auto *trace = static_cast<const ResetTrace *>(
        MapViewOfFile(trace_map, FILE_MAP_READ, 0, 0, sizeof(ResetTrace)));
    assert(trace && trace->count >= 5);
    unsigned phases = 0;
#if LMU_RESET_HOST_REVISION >= 12
    double before_correction = 0, after_correction = 0;
#endif
    for (LONG64 i = 0; i < trace->count; ++i) {
        phases |= 1u << trace->records[i].phase;
        if (trace->records[i].phase == 3)
            assert(trace->records[i].bodies[0][3] == 80);
#if LMU_RESET_HOST_REVISION >= 12
        const auto &sample = trace->records[i];
        if (sample.phase == 11) {
            assert((sample.valid & (7u << 9)) == (7u << 9));
#if LMU_RESET_HOST_REVISION < 15
            assert(sample.collision_flags == 1 && sample.collision_body[0] == 314 &&
                   sample.exported_vehicle[9] == 271);
#endif
            before_correction = sample.bodies[0][0];
        }
        if (sample.phase == 12)
            after_correction = sample.bodies[0][0];
#endif
    }
    assert((phases & ((1u << 3) | (1u << 5) | (1u << 6))) == ((1u << 3) | (1u << 5) | (1u << 6)));
#if LMU_RESET_HOST_REVISION >= 11
    const unsigned boundaries = (1u << 7) | (1u << 8) | (1u << 9) | (1u << 10);
    assert((phases & boundaries) == boundaries);
#endif
#if LMU_RESET_HOST_REVISION >= 12
    const unsigned collision_phases = (1u << 11) | (1u << 12) | (1u << 13) | (1u << 14);
    assert((phases & collision_phases) == collision_phases);
#if LMU_RESET_HOST_REVISION >= 15
    assert(after_correction == before_correction);
    assert((phases & ((1u << 15) | (1u << 16))) == ((1u << 15) | (1u << 16)));
#else
    assert(after_correction - before_correction == 11);
#endif
    assert(correction_calls == static_cast<unsigned>(original_boundaries) &&
           collision_prepare_calls == static_cast<unsigned>(original_boundaries)
#if LMU_RESET_HOST_REVISION >= 15
                                          + collision_rebuild_calls
#endif
    );
#endif
    UnmapViewOfFile(trace);
    CloseHandle(trace_map);
#endif
    assert(restored.speed >= 100 && restored.before_position[0] - saved.saved_position[0] == 1000);
    assert(ipc->physical_field_write_attempts == expected_fields);
    for (unsigned i = 0; i < 3; ++i)
        assert(restored.after_position[i] == saved.saved_position[i]);
    mismatched_reads = -1;
    auto mismatch = request(ipc, 5, HOST_PREPARE);
    assert(mismatch.result == CONTROL_REJECTED && mismatch.cause == 90205 &&
           ipc->physical_field_write_attempts == expected_fields);
    assert(mismatch.pins == restored.pins + 1); // No retry/failover.
    mismatched_reads = 0;
#if LMU_RESET_HOST_REVISION >= 21
    // Same addresses after publication must not make an old model checkpoint valid.
    void(WINAPI *volatile graphics_publish)(void *, void *) = FixtureGraphicsPublish;
    graphics_publish(host_bodywork.component, nullptr);
    auto stale_graphics = request(ipc, 6, HOST_RESTORE);
    assert(stale_graphics.result != CONTROL_OK && stale_graphics.cause == 92007);
    assert(stale_graphics.forward_writes == 0 &&
           ipc->physical_field_write_attempts == expected_fields);
    auto refreshed_graphics = request(ipc, 7, HOST_SAVE);
    assert(refreshed_graphics.result == CONTROL_OK && refreshed_graphics.saves == 2);
#endif
    void(WINAPI *volatile reconstruct)(void *) = FixtureReconstruct;
    reconstruct(reinterpret_cast<void *>(source_bindings.address[104]));
    // A pit reconstruction invalidates the checkpoint even without another key.
    // The old host stops before a fresh instance can be admitted.
    for (unsigned i = 0; i < 500 && ipc->status == 2; ++i)
        Sleep(10);
    assert(ipc->status == 3 && !ipc->error && ipc->reserved == HOST_STOP_LIFECYCLE &&
           original_boundaries > 0 && ipc->physical_field_write_attempts == expected_fields);
    if (argc == 4 && !suppress_player_callback) {
        char second[MAX_PATH];
        std::snprintf(second, sizeof(second), "%s.restart.dll", argv[1]);
        assert(CopyFileA(argv[1], second, FALSE));
        DeleteCriticalSection(&game_lock);
        DeleteCriticalSection(&control_lock);
        DeleteCriticalSection(&gate_lock);
        char *next[]{argv[0], second, argv[2]};
        const int result = run_fixture(3, next);
        assert(!result);
        puts(
            "PASS restart: two distinct host instances in the same process, each cleanly stopped.");
        return result;
    }
    puts("PASS loaded physical host: moving save/restore at 80/100 m/s over 1000 m, bounded "
         "cache-aware inertia validation and immediate corruption refusal, write opt-in, full "
         "typed field restore, four "
         "contacts, native lock release before update, lifecycle invalidation and clean stop. "
         "Owned fixture memory only.");
    return 0;
}
int main(int argc, char **argv) {
    return run_fixture(argc, argv);
}
