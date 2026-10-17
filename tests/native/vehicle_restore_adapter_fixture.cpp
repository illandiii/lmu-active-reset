#include "../../src/native/restore/vehicle_restore_adapter_25661166.h"
#include "../../src/native/runtime/active_reset_boundary.h"
#include "../../src/native/runtime/native_lifetime_handoff.h"
#include "vehicle_contact_fixture_model.h"
#include <memory>
#include <cstdlib>

static SnapshotFrame source_frame;
static RestoreBindings source_bindings;
static unsigned char score[0x27400];
static CRITICAL_SECTION game_lock, control_lock, gate_lock;
static HANDLE control_entered, control_yield, control_resumed;
static DWORD WINAPI control_loop(void *) {
    EnterCriticalSection(&control_lock);
    SetEvent(control_entered);
    assert(WaitForSingleObject(control_yield, 5000) == WAIT_OBJECT_0);
    LeaveCriticalSection(&control_lock);
    EnterCriticalSection(&gate_lock);
    LeaveCriticalSection(&gate_lock);
    EnterCriticalSection(&control_lock);
    SetEvent(control_resumed);
    LeaveCriticalSection(&control_lock);
    return 0;
}
static bool pinned = true;
static int original_calls;
static int fault;
static unsigned char collision_vehicle[0x18000];
static unsigned collision_refreshes;
static int rebuild_fixture_collision(void *, uintptr_t vehicle) {
    assert(vehicle == reinterpret_cast<uintptr_t>(collision_vehicle));
    assert(native_lock_witness(reinterpret_cast<uintptr_t>(&game_lock)).held_by_caller);
    assert(native_lock_witness(reinterpret_cast<uintptr_t>(&control_lock)).held_by_caller);
    ++collision_refreshes;
    return fault != 3 || collision_refreshes != 1;
}
static void WINAPI refresh(void *object, double dt, double horizon, void *world, double angular) {
    const auto forward_calls = fault == 1 ? 2u : fault == 2 ? 1u : 4u;
    assert(angular == (calls < forward_calls ? 2.5 : 3.5));
    // The geometric fixture's fixed input is independent of this argument test.
    FixtureRefresh(object, dt, horizon, world, 2.5);
    if (fault == 2 || (fault == 1 && calls == 2)) {
        const auto nodes = get<uintptr_t>(static_cast<unsigned char *>(object) + 0x318);
        reinterpret_cast<unsigned char *>(nodes)[0x67] = 1;
    }
}
static bool fixture_read(uintptr_t, uintptr_t, uint64_t epoch, double time, SnapshotFrame *frame,
                         RestoreBindings *bindings) {
    *frame = source_frame;
    frame->simulation_time = time;
    *bindings = source_bindings;
    bindings->epoch = epoch;
    return true;
}
static int lifetime(void *, const VehicleRestoreContext *) {
    return pinned;
}
static void WINAPI original(uint64_t value) {
    assert(value == 77);
    ++original_calls;
}
static void load_frame(const char *path) {
    FILE *file = std::fopen(path, "rb");
    assert(file);
    assert(std::fread(&source_frame, sizeof(source_frame), 1, file) == 1);
    assert(std::fgetc(file) == EOF);
    std::fclose(file);
}

int main(int argc, char **argv) {
    assert(argc == 4);
    fault = std::atoi(argv[3]);
    assert(fault >= 0 && fault <= 3);
    load_frame(argv[1]);
    auto memory = std::make_unique<unsigned char[]>(108 * 0x5000 + 2593 * 0x110);
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
    score[0x164] = 1;
    score[0x180] = 0x5a; // unrelated record byte must survive.
    InitializeCriticalSection(&game_lock);
    InitializeCriticalSection(&control_lock);
    InitializeCriticalSection(&gate_lock);
    VehicleRestoreHost host{0x12345678 - 0x1aa4090,
                            reinterpret_cast<uintptr_t>(&game_lock),
                            reinterpret_cast<uintptr_t>(&control_lock),
                            GetCurrentThreadId(),
                            nullptr,
                            lifetime,
                            refresh};
    host.collision_vehicle = reinterpret_cast<uintptr_t>(collision_vehicle);
    host.rebuild_collision = rebuild_fixture_collision;
    source_bindings.address[RESTORE_COLLISION_EXPORT_SLOT] = host.collision_vehicle + 0x159e8;
    put(collision_vehicle + 0x15a50, float{1250});
    put(collision_vehicle + 0x15a24, float{80});
    for (unsigned i = 0; i < 9; ++i) {
        put(collision_vehicle + 0x159e8 + i * 4, float(i % 4 == 0));
        put(reinterpret_cast<unsigned char *>(source_bindings.address[104]) + 0x2500 + i * 8,
            double(i % 4 == 0));
    }
    auto *adapter = vehicle_restore_adapter_create(&host);
    assert(adapter);
    vehicle_restore_adapter_fixture_reader(adapter, fixture_read,
                                           reinterpret_cast<uintptr_t>(score));
    auto runtime = std::make_unique<ResetNativeRuntime>();
    runtime->control.owner_thread = GetCurrentThreadId();
    SimulationFence fence{};
    uintptr_t objects[]{0x1234, reinterpret_cast<uintptr_t>(score),
                        reinterpret_cast<uintptr_t>(tire[0])};
    uintptr_t tires[4];
    for (unsigned i = 0; i < 4; ++i)
        tires[i] = reinterpret_cast<uintptr_t>(tire[i]);
    simulation_fence_init(&fence, 42, GetCurrentThreadId(), objects, tires, 4);
    VehicleRestoreContext context{
        {1, 2, 3, 4}, 42, source_bindings.address[104], source_frame.simulation_time, {}};
    double current_angular = 2.5;
    auto samples = [&] {
        for (unsigned i = 0; i < 4; ++i)
            context.contacts[i] = {tires[i],         .001,       .003,
                                   current_angular,  0x12345678, GetCurrentThreadId(),
                                   GetTickCount64(), 42};
    };
    samples();
    assert(!vehicle_restore_adapter_bind(adapter, &context)); // no native lock
    EnterCriticalSection(&game_lock);
    assert(!vehicle_restore_adapter_bind(adapter, &context)); // worker lock alone is insufficient
    NativeLifetimePin lifetime_pin{};
    auto *handoff = native_handoff_create(host.native_lock, host.control_lock,
                                          reinterpret_cast<uintptr_t>(&gate_lock));
    assert(handoff);
    control_entered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    control_yield = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    control_resumed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    assert(control_entered && control_yield && control_resumed);
    HANDLE control_thread = CreateThread(nullptr, 0, control_loop, nullptr, 0, nullptr);
    assert(control_thread && WaitForSingleObject(control_entered, 5000) == WAIT_OBJECT_0);
    assert(native_handoff_request(handoff, 50));
    auto deadline = GetTickCount64() + 1000;
    while (native_handoff_state(handoff) == LifetimeHandoffState::Queued &&
           GetTickCount64() < deadline)
        SwitchToThread();
    assert(native_handoff_state(handoff) == LifetimeHandoffState::Offered);
    assert(!native_handoff_try_pin(handoff, &lifetime_pin));
    SetEvent(control_yield);
    while (!native_handoff_try_pin(handoff, &lifetime_pin) && GetTickCount64() < deadline)
        SwitchToThread();
    assert(lifetime_pin.acquired && native_lifetime_valid(&lifetime_pin));
    assert(WaitForSingleObject(control_resumed, 0) == WAIT_TIMEOUT);
    auto ops = vehicle_restore_adapter_ops(adapter);
    auto dispatch = [&](uint64_t id, unsigned action) {
        assert(vehicle_restore_adapter_bind(adapter, &context));
        ResetRequest request{id, GetTickCount64(), context.identity, action};
        return active_reset_boundary_dispatch(runtime.get(), &fence, objects[0], host.native_lock,
                                              &ops, GetTickCount64(), &context.identity, 1,
                                              &request, original, 77);
    };
    assert(dispatch(1, RESET_SAVE).control_result == CONTROL_OK);
    put(collision_vehicle + 0x159e8, float{0}); // Singular collision-facing rotation.
    assert(ops.capture(ops.user, &context.identity) != 0);
    put(collision_vehicle + 0x159e8, float{1});
    auto original_saved_position = source_frame.bodies[0].position_world[0];
    source_frame.terminal_failure = 1;
    assert(dispatch(2, RESET_SAVE).control_result == CONTROL_SAVE_FAILED);
    load_frame(argv[2]);
    // An impact may set the shared failure latch without replacing the assembly.
    // Loading must recover the healthy checkpoint; failed transactions restore 4.
    source_frame.terminal_failure = 4;
    put(score + 0xd4, uint32_t{4});
    put(collision_vehicle + 0x15a50, float{1240});
    put(collision_vehicle + 0x15a24, float{-100});
    current_angular = 3.5;
    context.simulation_time = source_frame.simulation_time;
    samples();
    context.contacts[3].tick_ms = GetTickCount64() - 101;
    assert(dispatch(3, RESET_LOAD).control_result == CONTROL_REJECTED && calls == 0);
    samples();
    ++context.contacts[0].epoch;
    assert(dispatch(4, RESET_LOAD).control_result == CONTROL_REJECTED && calls == 0);
    samples();
    const auto topology = source_frame.topology_token;
    ++source_frame.topology_token;
    assert(dispatch(5, RESET_LOAD).control_result == CONTROL_REJECTED && calls == 0);
    assert(runtime->plan.count == 0);
    source_frame.topology_token = topology;
    samples();
    auto result = dispatch(6, RESET_LOAD);
    std::fprintf(stderr, "full adapter result=%d fields=%zu flow=%d cause=%d contacts=%llu\n",
                 result.control_result, runtime->plan.count, runtime->report.result,
                 runtime->report.cause, static_cast<unsigned long long>(calls));
    assert(runtime->plan.count == 10085);
    assert(result.control_result == (fault == 2   ? CONTROL_QUARANTINED
                                     : fault != 0 ? CONTROL_RESTORE_RECOVERED
                                                  : CONTROL_OK));
    assert(result.forwarded == (fault != 2));
    assert(runtime->control.loads == (fault == 0 ? 1 : 0));
    if (fault != 2) {
        assert(get<uint32_t>(score + 0xd4) == (fault ? 4u : 0u));
        assert(calls == (fault == 1 ? 6 : fault == 3 ? 8 : 4));
        assert(collision_refreshes == (fault == 3 ? 2 : 1));
        assert(vehicle_restore_adapter_contact_report(adapter).completed_mask == 15);
        for (size_t i = 0; i < runtime->plan.count; ++i) {
            const auto &field = runtime->plan.fields[i];
            const auto address = field.object_slot == RESTORE_OBJECT_COUNT
                                     ? reinterpret_cast<uintptr_t>(score)
                                     : source_bindings.address[field.object_slot];
            assert(std::memcmp(reinterpret_cast<void *>(address + field.field_offset),
                               fault ? field.expected : field.desired, field.size) == 0);
        }
        assert(get<double>(reinterpret_cast<void *>(source_bindings.address[0] + 0x40)) ==
               (fault ? 0 : original_saved_position));
    } else {
        assert(!vehicle_restore_adapter_bind(adapter, &context));
        assert(!simulation_fence_stop(&fence));
        const auto before = original_calls;
        auto denied = active_reset_boundary_dispatch(runtime.get(), &fence, objects[0],
                                                     host.native_lock, &ops, GetTickCount64(),
                                                     &context.identity, 1, nullptr, original, 77);
        assert(!denied.forwarded && original_calls == before);
    }
    assert(score[0x164] == 1 && score[0x180] == 0x5a);
    for (unsigned offset : {0x160u, 0x27290u, 0x27360u})
        assert(get<int32_t>(score + offset) == (fault ? 2 : 0));
    if (fault != 2) {
        // Reconstructing the vehicle at identical addresses still ends its lifetime.
        ++context.epoch;
        samples();
        assert(dispatch(7, RESET_LOAD).control_result == CONTROL_REJECTED);
        --context.epoch;
        samples();
        assert(dispatch(8, RESET_LOAD).control_result == CONTROL_REJECTED);
        ++context.identity.session;
        samples();
        assert(dispatch(9, RESET_LOAD).control_result == CONTROL_NO_CHECKPOINT);
    }
    pinned = false;
    assert(!vehicle_restore_adapter_bind(adapter, &context));
    assert(WaitForSingleObject(control_resumed, 0) == WAIT_TIMEOUT);
    assert(native_lifetime_release(&lifetime_pin));
    LeaveCriticalSection(&game_lock);
    assert(WaitForSingleObject(control_thread, 5000) == WAIT_OBJECT_0);
    assert(WaitForSingleObject(control_resumed, 0) == WAIT_OBJECT_0);
    assert(native_handoff_destroy(handoff));
    for (HANDLE handle : {control_thread, control_entered, control_yield, control_resumed})
        CloseHandle(handle);
    vehicle_restore_adapter_destroy(adapter);
    DeleteCriticalSection(&game_lock);
    DeleteCriticalSection(&control_lock);
    DeleteCriticalSection(&gate_lock);
    assert(original_calls == (fault == 2 ? 5 : 9) && abi_errors == 0);
    puts("PASS full vehicle adapter: asynchronous control handoff -> saved/current capture -> "
         "10085-field native boundary with impact failure recovery with collision cache "
         "compensation "
         "transaction -> four contact rebuilds; failed save preserves checkpoint; stale contacts, "
         "missing lock/pin and changed session rejected. Owned memory only; refresh modeled.");
    return 0;
}
