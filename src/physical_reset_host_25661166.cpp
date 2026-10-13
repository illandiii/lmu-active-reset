// One-session experimental host. No automatic save, restore or hotkey handling.
// Commands require a fresh external offline lease and are consumed at the outer
// physics boundary under all three lifetime scopes. Hooks remain after fatal recovery.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "physical_reset_host_25661166.h"
#include "vehicle_restore_adapter_25661166.h"
#include "active_reset_boundary.h"
#include "native_lifetime_handoff.h"
#include "native_lifecycle_hooks_25661166.h"
#include "physical_reset_targets_25661166.h"
#include "verified_game_25661166.h"
#include "MinHook.h"
#include <cmath>
#include <cstring>
#include <new>
#include <cstdio>

static ResetHostIPC *ipc;
static uintptr_t base, owner_root, physics_root, scoring_root, tires[4];
static uintptr_t worker_lock, control_lock, gate_lock;
static DWORD physics_thread;
static NativeLifecycleEpoch lifecycle;
static NativeLifetimeHandoff *handoff;
static NativeLifetimePin native_pin;
static NativeLifecyclePin epoch_pin;
static SimulationFence fence;
static VehicleRestoreAdapter *adapter;
static ResetNativeRuntime *runtime;
static SnapshotFrame *diagnostic;
static RestoreBindings diagnostic_bindings;
static VehicleRestoreContext context;
static ResetHostReport report;
static double last_time;
static bool seen_time, scope_active;
static uint64_t last_id, callbacks, pins;
static volatile LONG fatal, active_boundaries;
static thread_local unsigned boundary_depth;
static void(WINAPI *outer_original)(uint64_t);
static void(WINAPI *dynamics_original)(void *, double);
static void(WINAPI *scoring_original)(void *, float);
static void(WINAPI *contacts_original)(void *, double, double, void *, double);
static VehicleContactSample samples[4];

static LONG64 number(volatile LONG64 *p) {
    return InterlockedCompareExchange64(p, 0, 0);
}
template <class T> static bool field(uintptr_t p, T &out) {
    SIZE_T got = 0;
    return p >= 0x10000 && p < 0x800000000000ULL &&
           ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void *>(p), &out, sizeof(out),
                             &got) &&
           got == sizeof(out);
}
static bool lease() {
    const auto now = GetTickCount64();
    const auto heartbeat = static_cast<uint64_t>(number(&ipc->heartbeat_ms));
    return !ipc->stop && ipc->allow == 1 && heartbeat <= now && now - heartbeat <= 300;
}
static bool roots_match() {
#ifdef LMU_RESET_HOST_FIXTURE
    using Roots = bool (*)(uintptr_t *, uintptr_t *, uintptr_t *, uintptr_t *);
    auto read = reinterpret_cast<Roots>(
        GetProcAddress(reinterpret_cast<HMODULE>(base), "FixtureHostRoots"));
    uintptr_t o = 0, p = 0, s = 0, t[4]{};
    return read && read(&o, &p, &s, t) && o == owner_root && p == physics_root &&
           s == scoring_root && std::memcmp(t, tires, sizeof(t)) == 0;
#else
    uintptr_t owner = 0, physics = 0, vehicle = 0, back = 0;
    if (!field(base + 0x1e194c8, owner) || owner != owner_root ||
        !field(owner + 0x159a8, physics) || physics != physics_root || !field(owner, vehicle) ||
        vehicle + 0x1cec8 != scoring_root || !field(scoring_root, back) || back != owner_root)
        return false;
    for (unsigned i = 0; i < 4; ++i) {
        uintptr_t tire = 0;
        if (!field(physics + 0x618 + i * 0x6b0 + 0x3e8, tire) || tire != tires[i])
            return false;
    }
    return true;
#endif
}
static bool read_vehicle(uintptr_t module, uintptr_t physics, uint64_t epoch, double time,
                         SnapshotFrame *frame, RestoreBindings *bindings) {
#ifdef LMU_RESET_HOST_FIXTURE
    auto reader = reinterpret_cast<VehicleFixtureReader>(
        GetProcAddress(reinterpret_cast<HMODULE>(base), "FixtureHostRead"));
    return reader && reader(module, physics, epoch, time, frame, bindings);
#else
    return vehicle_read_25661166(module, physics, epoch, time, frame, bindings);
#endif
}
static int pinned(void *, const VehicleRestoreContext *c) {
    return scope_active && c && c->epoch == 1 && c->physics == physics_root &&
           GetCurrentThreadId() == physics_thread && native_lifetime_valid(&native_pin) &&
           native_lifecycle_valid(&epoch_pin) && roots_match();
}
static void release_scope() {
    if (!scope_active)
        return;
    scope_active = false;
    const bool epoch_released = native_lifecycle_release(&epoch_pin);
    const bool native_released = native_lifetime_release(&native_pin);
    if (!epoch_released || !native_released)
        InterlockedExchange(&fatal, 1);
}
static void WINAPI resume_outer(uint64_t input) {
    if (scope_active && report.action == HOST_RESTORE && runtime->report.result == FLOW_OK &&
        runtime->report.forward.attempted) {
        unsigned char actual[RESTORE_FIELD_BYTES];
        for (size_t i = 0; i < runtime->plan.count; ++i) {
            const auto &entry = runtime->plan.fields[i];
            const auto address = entry.object_slot == RESTORE_OBJECT_COUNT
                                     ? runtime->scoring
                                     : runtime->bindings.address[entry.object_slot];
            SIZE_T got = 0;
            const bool read =
                ReadProcessMemory(GetCurrentProcess(),
                                  reinterpret_cast<void *>(address + entry.field_offset), actual,
                                  entry.size, &got) &&
                got == entry.size;
            if (read && std::memcmp(actual, entry.desired, entry.size) == 0)
                ++report.verified_fields;
            else
                ++report.mismatched_fields;
        }
    }
    if (scope_active && diagnostic_bindings.address[0])
        field(diagnostic_bindings.address[0] + 0x40, report.after_position);
    // The native update must not inherit additional locks acquired for reset.
    release_scope();
    if (!fatal)
        outer_original(input);
}
static void publish(uint64_t id) {
    report.callbacks = callbacks;
    report.pins = pins;
    report.saves = runtime->control.saves;
    report.loads = runtime->control.loads;
    InterlockedIncrement64(&ipc->report_sequence);
    ipc->report = report;
    MemoryBarrier();
    InterlockedIncrement64(&ipc->report_sequence);
    InterlockedExchange64(&ipc->completed_id, id);
}
static void WINAPI dynamics(void *object, double time) {
    if (reinterpret_cast<uintptr_t>(object) == physics_root &&
        GetCurrentThreadId() == physics_thread && std::isfinite(time)) {
        if (seen_time && time < last_time)
            runtime->control.checkpoint_valid = 0;
        last_time = time;
        seen_time = true;
    }
    if (!fatal)
        dynamics_original(object, time);
}
static void WINAPI scoring(void *object, float dt) {
    FenceTicket ticket{};
    if (!simulation_fence_callback_enter(&fence, FENCE_SCORING, reinterpret_cast<uintptr_t>(object),
                                         &ticket))
        return;
    if (simulation_fence_may_advance(&ticket))
        scoring_original(object, dt);
    simulation_fence_callback_leave(&ticket);
}
static void WINAPI contacts(void *object, double dt, double horizon, void *world, double angular) {
    FenceTicket ticket{};
    if (!simulation_fence_callback_enter(&fence, FENCE_CONTACTS,
                                         reinterpret_cast<uintptr_t>(object), &ticket))
        return;
    if (simulation_fence_may_advance(&ticket)) {
        contacts_original(object, dt, horizon, world, angular);
        if (GetCurrentThreadId() == physics_thread)
            for (unsigned i = 0; i < 4; ++i)
                if (reinterpret_cast<uintptr_t>(object) == tires[i])
                    samples[i] = {tires[i],
                                  dt,
                                  horizon,
                                  angular,
                                  reinterpret_cast<uintptr_t>(world),
                                  physics_thread,
                                  GetTickCount64(),
                                  1};
    }
    simulation_fence_callback_leave(&ticket);
}
static void WINAPI boundary(uint64_t input) {
    InterlockedIncrement(&active_boundaries);
    ++boundary_depth;
    ++callbacks;
    if (fatal) {
        --boundary_depth;
        InterlockedDecrement(&active_boundaries);
        return;
    }
    const uint64_t id = static_cast<uint64_t>(number(&ipc->request_id));
    const auto action = static_cast<unsigned>(number(&ipc->request_action));
    const uint64_t issued = static_cast<uint64_t>(number(&ipc->request_tick));
    const uint64_t now = GetTickCount64();
    const bool pending = id && id > last_id;
    bool acquired = false;
    if (pending && boundary_depth == 1 && GetCurrentThreadId() == physics_thread && lease() &&
        seen_time && issued <= now && now - issued <= 1500)
        acquired = native_handoff_try_pin(handoff, &native_pin);
    if (acquired) {
        ++pins;
        if (!native_lifecycle_try_pin(&lifecycle, 1, &epoch_pin)) {
            native_lifetime_release(&native_pin);
            report.action = action;
            report.result = 1001; // Lifecycle changed; this module never re-arms it.
            last_id = id;
            publish(id);
        } else {
            scope_active = true;
            report.action = action;
            report.result = CONTROL_REJECTED;
            report.fields = report.forward_writes = report.recovery_writes = 0;
            report.verified_fields = report.mismatched_fields = 0;
            report.flow_result = report.cause = report.contact_mask = report.contact_calls = 0;
            report.epoch = epoch_pin.generation;
            context.simulation_time = last_time;
            std::memcpy(context.contacts, samples, sizeof(samples));
            report.readable =
                pinned(nullptr, &context) &&
                read_vehicle(base, physics_root, 1, last_time, diagnostic, &diagnostic_bindings);
            if (!report.readable)
                report.cause = pinned(nullptr, &context) ? diagnostic->binding_status : 90004;
            if (!report.readable && report.cause == 90205 && now - issued < 1200) {
                // Native inertia caches may disagree at a particular boundary.
                // Retry this SAME request on a later boundary, without weakening
                // the reader or writing anything. The existing TTL still applies.
                release_scope();
                if (!fatal)
                    outer_original(input);
                --boundary_depth;
                InterlockedDecrement(&active_boundaries);
                return;
            }
            if (report.readable && vehicle_restore_adapter_bind(adapter, &context)) {
                std::memcpy(report.before_position, diagnostic->bodies[0].position_world,
                            sizeof(report.before_position));
                double speed2 = 0;
                for (double component : diagnostic->bodies[0].velocity_world)
                    speed2 += component * component;
                report.speed = std::sqrt(speed2);
                auto ops = vehicle_restore_adapter_ops(adapter);
                if (action == HOST_PREPARE) {
                    report.result = ops.prepare(ops.user, &context.identity, &runtime->plan,
                                                &runtime->bindings, &runtime->scoring);
                    report.fields = runtime->plan.count;
                } else if (action == HOST_SAVE ||
                           (action == HOST_RESTORE && ipc->permit_writes == 1)) {
                    // Distance and speed do not define checkpoint validity.
                    // Full state, lifetime and contact checks remain mandatory.
                    {
                        const ResetRequest request{id, issued, context.identity,
                                                   action == HOST_SAVE ? RESET_SAVE : RESET_LOAD};
                        const auto result = active_reset_boundary_dispatch(
                            runtime, &fence, base + 0xcd1f40, worker_lock, &ops, now,
                            &context.identity, 1, &request, resume_outer, input);
                        report.result = result.control_result;
                        if (action == HOST_SAVE && result.control_result == CONTROL_OK)
                            std::memcpy(report.saved_position, report.before_position,
                                        sizeof(report.saved_position));
                        report.fields = runtime->plan.count;
                        report.forward_writes = runtime->report.forward.attempted;
                        report.recovery_writes = runtime->report.recovery.attempted;
                        if (action == HOST_RESTORE)
                            InterlockedExchangeAdd64(&ipc->physical_field_write_attempts,
                                                     report.forward_writes +
                                                         report.recovery_writes);
                        report.flow_result = runtime->report.result;
                        report.cause = runtime->report.cause;
                        const auto contact_report = vehicle_restore_adapter_contact_report(adapter);
                        report.contact_mask = contact_report.completed_mask;
                        report.contact_calls = contact_report.refresh_calls;
                        if (runtime->control.quarantined || runtime->flow.quarantined)
                            InterlockedExchange(&fatal, 1);
                        release_scope();
                        last_id = id;
                        publish(id);
                        --boundary_depth;
                        InterlockedDecrement(&active_boundaries);
                        return;
                    }
                }
            }
            release_scope();
            last_id = id;
            publish(id);
        }
    } else if (pending && (issued > now || now - issued > 1500)) {
        report.action = action;
        report.result = 1003;
        last_id = id;
        publish(id);
    }
    if (!fatal)
        outer_original(input);
    --boundary_depth;
    InterlockedDecrement(&active_boundaries);
}

static DWORD WINAPI worker(void *) {
    wchar_t name[96];
    swprintf(name, 96, L"Local\\LMU_PhysicalReset25661166_v%u_%lu",
             static_cast<unsigned>(LMU_RESET_HOST_REVISION), GetCurrentProcessId());
    HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!mapping)
        return 0;
    ipc = static_cast<ResetHostIPC *>(
        MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*ipc)));
    if (!ipc || ipc->magic != RESET_HOST_MAGIC || ipc->version != 1 ||
        ipc->pid != GetCurrentProcessId())
        return 0;
    ipc->status = 1;
    base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    physics_thread = static_cast<DWORD>(ipc->owner_thread);
#ifdef LMU_RESET_HOST_FIXTURE
    using Roots = bool (*)(uintptr_t *, uintptr_t *, uintptr_t *, uintptr_t *);
    using Lock = uintptr_t (*)();
    auto roots = reinterpret_cast<Roots>(
        GetProcAddress(reinterpret_cast<HMODULE>(base), "FixtureHostRoots"));
    auto w = reinterpret_cast<Lock>(
        GetProcAddress(reinterpret_cast<HMODULE>(base), "FixtureWorkerLock"));
    auto c = reinterpret_cast<Lock>(
        GetProcAddress(reinterpret_cast<HMODULE>(base), "FixtureControlLock"));
    auto g = reinterpret_cast<Lock>(GetProcAddress(reinterpret_cast<HMODULE>(base), "FixtureGate"));
    if (!roots || !w || !c || !g || !roots(&owner_root, &physics_root, &scoring_root, tires)) {
        ipc->error = 100;
        ipc->status = 4;
        return 0;
    }
    worker_lock = w();
    control_lock = c();
    gate_lock = g();
#else
    uintptr_t vehicle = 0;
    if (!verified_game_25661166() || !physics_thread || !field(base + 0x1e194c8, owner_root) ||
        !field(owner_root + 0x159a8, physics_root) || !field(owner_root, vehicle)) {
        ipc->error = 100;
        ipc->status = 4;
        return 0;
    }
    scoring_root = vehicle + 0x1cec8;
    for (unsigned i = 0; i < 4; ++i)
        field(physics_root + 0x618 + i * 0x6b0 + 0x3e8, tires[i]);
    worker_lock = base + 0x3b8a968;
    control_lock = base + 0x1a5a5c8;
    gate_lock = base + 0x1a5a5f0;
#endif
    if (!roots_match()) {
        ipc->error = 101;
        ipc->status = 4;
        return 0;
    }
#ifndef LMU_RESET_HOST_FIXTURE
    for (const auto &target : RESET_HOST_TARGETS)
        if (std::memcmp(reinterpret_cast<void *>(base + target.rva), target.prolog,
                        sizeof(target.prolog))) {
            ipc->error = 102;
            ipc->status = 4;
            return 0;
        }
#endif
    native_lifecycle_init(&lifecycle);
    context = {{GetCurrentProcessId(), 1, physics_root, 25661166}, 1, physics_root, 0, {}};
    uintptr_t objects[]{base + 0xcd1f40, scoring_root, tires[0]};
    simulation_fence_init(&fence, 1, physics_thread, objects, tires, 4);
    runtime = new (std::nothrow) ResetNativeRuntime{};
    diagnostic = new (std::nothrow) SnapshotFrame{};
    if (!runtime || !diagnostic || MH_Initialize() != MH_OK) {
        ipc->error = 103;
        ipc->status = 4;
        return 0;
    }
    runtime->control.owner_thread = physics_thread;
#ifdef LMU_RESET_HOST_FIXTURE
    const char *names[]{"FixtureBoundary", "FixtureDynamics", "FixtureScore", "FixtureContacts"};
#else
    const uintptr_t rvas[]{0xcd1f40, 0xecb4e0, 0xcf19f0, 0xef8880};
#endif
    void *hooks[]{reinterpret_cast<void *>(boundary), reinterpret_cast<void *>(dynamics),
                  reinterpret_cast<void *>(scoring), reinterpret_cast<void *>(contacts)};
    void **originals[]{reinterpret_cast<void **>(&outer_original),
                       reinterpret_cast<void **>(&dynamics_original),
                       reinterpret_cast<void **>(&scoring_original),
                       reinterpret_cast<void **>(&contacts_original)};
    for (unsigned i = 0; i < 4; ++i) {
#ifdef LMU_RESET_HOST_FIXTURE
        void *target =
            reinterpret_cast<void *>(GetProcAddress(reinterpret_cast<HMODULE>(base), names[i]));
#else
        void *target = reinterpret_cast<void *>(base + rvas[i]);
#endif
        if (!target || MH_CreateHook(target, hooks[i], originals[i]) != MH_OK) {
            ipc->error = 104;
            ipc->status = 4;
            return 0;
        }
    }
    if (!native_lifecycle_hooks_create(base, &lifecycle)) {
        ipc->error = 105;
        ipc->status = 4;
        return 0;
    }
    VehicleRestoreHost host{base,    worker_lock, control_lock,     physics_thread,
                            nullptr, pinned,      contacts_original};
    adapter = vehicle_restore_adapter_create(&host);
    handoff = native_handoff_create(host.native_lock, host.control_lock, gate_lock);
#ifdef LMU_RESET_HOST_FIXTURE
    if (adapter)
        vehicle_restore_adapter_fixture_reader(adapter, read_vehicle, scoring_root);
#endif
    if (!adapter || !handoff || MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        ipc->error = 106;
        ipc->status = 4;
        return 0;
    }
    ipc->status = 2;
    const auto start = GetTickCount64();
    uint64_t next_request = 0;
    while (!ipc->stop && !fatal) {
        const auto now = GetTickCount64();
        const auto heartbeat = static_cast<uint64_t>(number(&ipc->heartbeat_ms));
        if (heartbeat > now || now - heartbeat > 2000 || now - start > 300000)
            break;
        if (!lease())
            native_handoff_cancel(handoff);
        else if (number(&ipc->request_id) > number(&ipc->completed_id) && now >= next_request) {
            native_handoff_request(handoff, 25);
            next_request = now + 100;
        }
        Sleep(5);
    }
    ipc->allow = 0;
    ipc->stop = 1;
    if (!native_handoff_shutdown(handoff)) {
        ipc->error = 107;
        ipc->status = 5;
        return 0;
    }
    const auto drain_deadline = GetTickCount64() + 5000;
    while (InterlockedCompareExchange(&active_boundaries, 0, 0) &&
           GetTickCount64() < drain_deadline)
        Sleep(1);
    if (InterlockedCompareExchange(&active_boundaries, 0, 0)) {
        ipc->error = 111;
        ipc->status = 5;
        return 0;
    }
    if (fatal) {
        ipc->error = 108;
        ipc->status = 5;
        return 0; // Keep the physical/scoring/contact fence installed.
    }
    if (MH_DisableHook(MH_ALL_HOOKS) != MH_OK) {
        ipc->error = 109;
        ipc->status = 5;
        return 0;
    }
#ifndef LMU_RESET_HOST_FIXTURE
    for (const auto &target : RESET_HOST_TARGETS)
        if (std::memcmp(reinterpret_cast<void *>(base + target.rva), target.prolog,
                        sizeof(target.prolog))) {
            ipc->error = 110;
            ipc->status = 5;
            return 0;
        }
#endif
    ipc->status = 3;
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
        if (thread)
            CloseHandle(thread);
    }
    return TRUE;
}
