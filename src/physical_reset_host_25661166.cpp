// One-session experimental host. No automatic save, restore or hotkey handling.
// Commands require a fresh external offline lease and are consumed at the outer
// physics boundary under all three lifetime scopes. Hooks remain after fatal recovery.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "physical_reset_host_25661166.h"
#include "physical_reset_trace.h"
#include "reset_host_policy.h"
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
static HINSTANCE own_module;
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
static volatile LONG ready, observed_thread;
static thread_local unsigned boundary_depth;
static void(WINAPI *outer_original)(uint64_t);
static void(WINAPI *dynamics_original)(void *, double);
static void(WINAPI *scoring_original)(void *, float);
static void(WINAPI *contacts_original)(void *, double, double, void *, double);
#if LMU_RESET_HOST_REVISION >= 12
static void(WINAPI *collision_correct_original)(void *);
static void(WINAPI *collision_prepare_original)(void *);
#endif
static VehicleContactSample samples[4];
static ResetTrace *trace;
static unsigned trace_steps;
static uint64_t trace_request;
static uintptr_t trace_body;

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
static bool trace_read_body(uintptr_t body, double *out) {
    const unsigned offsets[]{0x40, 0x28, 0x90, 0x110};
    const unsigned counts[]{3, 3, 9, 3};
    unsigned cursor = 0;
    for (unsigned j = 0; j < 4; ++j) {
        SIZE_T got = 0;
        if (!body ||
            !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void *>(body + offsets[j]),
                               out + cursor, counts[j] * 8, &got) ||
            got != counts[j] * 8)
            return false;
        cursor += counts[j];
    }
    return true;
}
static void trace_sample(uint64_t phase) {
    if (!trace || GetCurrentThreadId() != physics_thread ||
        !native_lock_witness(worker_lock).held_by_caller || !roots_match())
        return;
#if LMU_RESET_HOST_REVISION < 14
    if (InterlockedCompareExchange64(&trace->count, 0, 0) >= 512)
        return;
#endif
    ResetTraceRecord sample{};
    sample.phase = phase;
    sample.request = trace_request;
    sample.tick = GetTickCount64();
    sample.step = trace_steps;
    for (unsigned i = 0; i < 5; ++i) {
        uintptr_t body = i ? tires[i - 1] + 0xac0 : trace_body;
        if (trace_read_body(body, sample.bodies[i]))
            sample.valid |= 1ULL << i;
    }
    for (unsigned w = 0; w < 4; ++w) {
        uintptr_t v[3]{};
        if (!field(tires[w] + 0x9f0, v) || v[1] < v[0] || v[2] < v[1] || (v[1] - v[0]) % 8 ||
            (v[1] - v[0]) / 8 > 512)
            continue;
        auto &c = sample.contacts[w];
        c[0] = static_cast<double>((v[1] - v[0]) / 8);
        c[1] = 1e300;
        c[2] = -1e300;
        bool valid = true;
        for (uintptr_t p = v[0]; p < v[1]; p += 8) {
            uintptr_t node = 0;
            double values[4]{};
            if (!field(p, node) || !field(node + 0xd0, values)) {
                valid = false;
                break;
            }
            if (values[0] < c[1])
                c[1] = values[0];
            if (values[0] > c[2]) {
                c[2] = values[0];
                std::memcpy(c + 3, values + 1, 24);
            }
        }
        if (valid)
            sample.valid |= 1ULL << (5 + w);
    }
#if LMU_RESET_HOST_REVISION >= 12
    const auto vehicle = owner_root + 8;
    if (trace_read_body(vehicle + 0x16d00, sample.collision_body))
        sample.valid |= 1ULL << 9;
    if (field(vehicle + 0x159e8, sample.exported_vehicle))
        sample.valid |= 1ULL << 10;
    unsigned char flags = 0;
    if (field(vehicle + 0x16cfa, flags)) {
        sample.collision_flags = flags;
        sample.valid |= 1ULL << 11;
    }
    double position[3], orientation[9], reference[3];
    if (field(physics_root + 0x24b0, position) && field(physics_root + 0x2500, orientation) &&
        field(physics_root + 0x2610, reference)) {
        std::memcpy(sample.hdv_pose, position, sizeof(position));
        std::memcpy(sample.hdv_pose + 3, orientation, sizeof(orientation));
        std::memcpy(sample.hdv_pose + 12, reference, sizeof(reference));
        sample.valid |= 1ULL << 12;
    }
    if (field(physics_root + 0x4bc8, sample.collision_feedback))
        sample.valid |= 1ULL << 13;
#endif
    reset_trace_append(trace, sample);
}
#if LMU_RESET_HOST_REVISION >= 12
static bool trace_collision(void *object) {
    return InterlockedCompareExchange(&ready, 0, 0) && GetCurrentThreadId() == physics_thread &&
           trace_steps && reinterpret_cast<uintptr_t>(object) == owner_root + 8;
}
static void WINAPI collision_correct(void *object) {
    const bool tracing = trace_collision(object);
    if (tracing)
        trace_sample(11);
    collision_correct_original(object);
    if (tracing)
        trace_sample(12);
}
static void WINAPI collision_prepare(void *object) {
    const bool tracing = trace_collision(object);
    if (tracing)
        trace_sample(13);
    collision_prepare_original(object);
    if (tracing)
        trace_sample(14);
}
#endif
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
static void forward_outer(uint64_t input) {
    const bool tracing = GetCurrentThreadId() == physics_thread && trace_steps;
    if (tracing)
        trace_sample(7);
    outer_original(input);
    if (tracing)
        trace_sample(8);
}
static void WINAPI resume_outer(uint64_t input) {
    if (scope_active && report.action == HOST_RESTORE) {
        trace_steps = 20;
        trace_sample(3); // Typed restore and contact rebuild completed, before native update.
    }
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
        forward_outer(input);
    if (trace_steps)
        trace_sample(4);
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
    if (!InterlockedCompareExchange(&ready, 0, 0)) {
        if (reinterpret_cast<uintptr_t>(object) == physics_root && boundary_depth == 1 &&
            native_lock_witness(worker_lock).held_by_caller) {
            const LONG thread = static_cast<LONG>(GetCurrentThreadId());
            const LONG prior = InterlockedCompareExchange(&observed_thread, thread, 0);
            if (prior && prior != thread)
                InterlockedExchange(&observed_thread, -1);
        }
        dynamics_original(object, time);
        return;
    }
    if (reinterpret_cast<uintptr_t>(object) == physics_root &&
        GetCurrentThreadId() == physics_thread && std::isfinite(time)) {
        if (seen_time && time < last_time)
            runtime->control.checkpoint_valid = 0;
        last_time = time;
        seen_time = true;
    }
    const bool tracing = trace_steps && reinterpret_cast<uintptr_t>(object) == physics_root &&
                         GetCurrentThreadId() == physics_thread;
    if (tracing)
        trace_sample(5);
    if (!fatal)
        dynamics_original(object, time);
    if (tracing) {
        trace_sample(6);
        --trace_steps;
    }
}
static void WINAPI scoring(void *object, float dt) {
    if (!InterlockedCompareExchange(&ready, 0, 0)) {
        scoring_original(object, dt);
        return;
    }
    FenceTicket ticket{};
    if (!simulation_fence_callback_enter(&fence, FENCE_SCORING, reinterpret_cast<uintptr_t>(object),
                                         &ticket))
        return;
    const bool tracing = GetCurrentThreadId() == physics_thread && trace_steps &&
                         reinterpret_cast<uintptr_t>(object) == scoring_root;
    if (tracing)
        trace_sample(9);
    if (simulation_fence_may_advance(&ticket))
        scoring_original(object, dt);
    if (tracing)
        trace_sample(10);
    simulation_fence_callback_leave(&ticket);
}
static void WINAPI contacts(void *object, double dt, double horizon, void *world, double angular) {
    if (!InterlockedCompareExchange(&ready, 0, 0)) {
        contacts_original(object, dt, horizon, world, angular);
        return;
    }
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
    if (!InterlockedCompareExchange(&ready, 0, 0)) {
        outer_original(input);
        --boundary_depth;
        InterlockedDecrement(&active_boundaries);
        return;
    }
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
            if (report.readable && vehicle_restore_adapter_bind(adapter, &context)) {
                trace_body = diagnostic_bindings.address[0];
                if (action == HOST_SAVE || action == HOST_RESTORE) {
                    trace_request = id;
                    trace_steps = 0;
                    trace_sample(action == HOST_SAVE ? 1 : 2);
                }
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
        forward_outer(input);
    --boundary_depth;
    InterlockedDecrement(&active_boundaries);
}

static DWORD WINAPI worker(void *) {
    wchar_t name[512];
#if LMU_RESET_HOST_REVISION >= 9
    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(own_module, path, MAX_PATH))
        return 0;
    const wchar_t *filename = wcsrchr(path, L'\\');
    swprintf(name, 512, L"Local\\LMU_PhysicalReset_%ls_%lu", filename ? filename + 1 : path,
             GetCurrentProcessId());
#else
    swprintf(name, 96, L"Local\\LMU_PhysicalReset25661166_v%u_%lu",
             static_cast<unsigned>(LMU_RESET_HOST_REVISION), GetCurrentProcessId());
#endif
    HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!mapping)
        return 0;
    ipc = static_cast<ResetHostIPC *>(
        MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*ipc)));
    if (!ipc || ipc->magic != RESET_HOST_MAGIC || ipc->version != 1 ||
        ipc->pid != GetCurrentProcessId())
        return 0;
    ipc->status = 1;
    wchar_t trace_name[520];
    swprintf(trace_name, 520, L"%ls_trace", name);
    HANDLE trace_mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                              sizeof(ResetTrace), trace_name);
    if (trace_mapping && GetLastError() != ERROR_ALREADY_EXISTS)
        trace = static_cast<ResetTrace *>(
            MapViewOfFile(trace_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ResetTrace)));
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
    if (!verified_game_25661166() || !field(base + 0x1e194c8, owner_root) ||
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
#if LMU_RESET_HOST_REVISION >= 12
    void *collision_targets[2];
#ifdef LMU_RESET_HOST_FIXTURE
    collision_targets[0] = reinterpret_cast<void *>(
        GetProcAddress(reinterpret_cast<HMODULE>(base), "FixtureCollisionCorrect"));
    collision_targets[1] = reinterpret_cast<void *>(
        GetProcAddress(reinterpret_cast<HMODULE>(base), "FixtureCollisionPrepare"));
#else
    collision_targets[0] = reinterpret_cast<void *>(base + 0xf4eac0);
    collision_targets[1] = reinterpret_cast<void *>(base + 0xf4e7b0);
#endif
    if (!collision_targets[0] || !collision_targets[1] ||
        MH_CreateHook(collision_targets[0], reinterpret_cast<void *>(collision_correct),
                      reinterpret_cast<void **>(&collision_correct_original)) != MH_OK ||
        MH_CreateHook(collision_targets[1], reinterpret_cast<void *>(collision_prepare),
                      reinterpret_cast<void **>(&collision_prepare_original)) != MH_OK) {
        ipc->error = 113;
        ipc->status = 4;
        return 0;
    }
#endif
    if (!native_lifecycle_hooks_create(base, &lifecycle)) {
        ipc->error = 105;
        ipc->status = 4;
        return 0;
    }
    handoff = native_handoff_create(worker_lock, control_lock, gate_lock);
    if (!handoff || MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        ipc->error = 106;
        ipc->status = 4;
        return 0;
    }
    // Observe the player's Dynamics call nested inside the outer callback,
    // with the worker lock actually owned by that caller. No sampled lock owner.
    const auto discovery_deadline = GetTickCount64() + 2000;
    LONG discovered = 0;
    while (!(discovered = InterlockedCompareExchange(&observed_thread, 0, 0)) && !ipc->stop &&
           GetTickCount64() < discovery_deadline)
        Sleep(1);
    if (discovered <= 0 || (physics_thread && physics_thread != static_cast<DWORD>(discovered))) {
        native_handoff_shutdown(handoff);
        const bool detached = MH_DisableHook(MH_ALL_HOOKS) == MH_OK;
        ipc->error = 112;
        ipc->status = detached ? 4 : 5;
        return 0;
    }
    physics_thread = static_cast<DWORD>(discovered);
    ipc->owner_thread = physics_thread;
    runtime->control.owner_thread = physics_thread;
    // First initialization: bootstrap callbacks bypass the fence until ready.
    simulation_fence_init(&fence, 1, physics_thread, objects, tires, 4);
    VehicleRestoreHost host{base,    worker_lock, control_lock,     physics_thread,
                            nullptr, pinned,      contacts_original};
    adapter = vehicle_restore_adapter_create(&host);
#ifdef LMU_RESET_HOST_FIXTURE
    if (adapter)
        vehicle_restore_adapter_fixture_reader(adapter, read_vehicle, scoring_root);
#endif
    if (!adapter) {
        native_handoff_shutdown(handoff);
        const bool detached = MH_DisableHook(MH_ALL_HOOKS) == MH_OK;
        ipc->error = 106;
        ipc->status = detached ? 4 : 5;
        return 0;
    }
    InterlockedExchange(&ready, 1);
    ipc->status = 2;
    uint64_t next_request = 0;
    while (!ipc->stop && !fatal) {
        const auto now = GetTickCount64();
        const auto heartbeat = static_cast<uint64_t>(number(&ipc->heartbeat_ms));
        if (!reset_host_controller_alive(now, heartbeat))
            break;
        if (native_lifecycle_changed(&lifecycle, 1)) {
            ipc->reserved = HOST_STOP_LIFECYCLE;
            break;
        }
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
        own_module = module;
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
        if (thread)
            CloseHandle(thread);
    }
    return TRUE;
}
