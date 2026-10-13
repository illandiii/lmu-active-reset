/* Read-only numerical/phase observer. No vehicle state writes or reset entry. */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>
#include <cmath>
#include <cstring>
#include "pose_observer_ipc.h"
#include "MinHook.h"
static PoseIPC *ipc;
static uintptr_t module_base;
static void(WINAPI *original_dynamics)(void *, double);
static void(WINAPI *original_contacts)(void *, double, double, void *, double);
static void(WINAPI *original_forces)(void *);
static void *targets[3];
static unsigned char originals[3][32];
struct Cycle {
    uintptr_t player, tires[4];
    uint64_t force_calls, contacts[4], masks[6], fresh, errors;
};
static thread_local Cycle *cycle;
template <class T> static bool read_field(uintptr_t p, T &out) {
    SIZE_T got = 0;
    return p >= 0x10000 && p < 0x800000000000ULL &&
           ReadProcessMemory(GetCurrentProcess(), (void *)p, &out, sizeof(out), &got) &&
           got == sizeof(out);
}
static bool lease() {
    LONG64 now = GetTickCount64(), hb = InterlockedCompareExchange64(&ipc->heartbeat_ms, 0, 0);
    return !ipc->stop && ipc->allow_observation == 1 && hb <= now && now - hb <= 300;
}
static bool player_matches(void *object) {
#ifdef LMU_POSE_FIXTURE
    typedef void *(WINAPI * F)();
    auto f = (F)GetProcAddress((HMODULE)module_base, "FixturePlayer");
    return f && f() == object;
#else
    uintptr_t owner = 0, player = 0;
    return read_field(module_base + 0x1de6b88, owner) && read_field(owner + 0x15978, player) &&
           player == (uintptr_t)object;
#endif
}
static bool read_vector(uintptr_t p, uintptr_t *out, size_t limit, size_t &count) {
    uintptr_t v[3];
    if (!read_field(p, v) || v[0] < 0x10000 || v[1] < v[0] || v[2] < v[1] || (v[1] - v[0]) % 8 ||
        (v[2] - v[0]) % 8)
        return false;
    count = (v[1] - v[0]) / 8;
    if (!count || count > limit)
        return false;
    for (size_t i = 0; i < count; i++)
        if (!read_field(v[0] + i * 8, out[i]) || out[i] < 0x10000)
            return false;
    return true;
}
static bool collect_topology(uintptr_t player, uintptr_t *bodies, size_t &roots,
                             uint64_t &aggregate, Cycle &c, int64_t *wheel_index) {
    uintptr_t primary[32];
    size_t primaries = 0;
    if (!read_vector(player + 0x23f8, bodies, 28, roots) ||
        !read_vector(player + 0x2440, primary, 32, primaries))
        return false;
    aggregate = 0;
    for (size_t j = 0; j < primaries; j++) {
        bool found = false;
        for (size_t i = 0; i < roots; i++)
            if (primary[j] == bodies[i]) {
                aggregate |= 1ULL << i;
                found = true;
            }
        if (!found)
            return false;
    }
    for (int w = 0; w < 4; w++) {
        uintptr_t wheel = player + 0x618 + w * 0x6b0, body = 0;
        if (!read_field(wheel + 0x3e8, c.tires[w]) || !read_field(wheel + 0x1d0, body) ||
            c.tires[w] < 0x10000)
            return false;
        bodies[roots + w] = c.tires[w] + 0xac0;
        wheel_index[w] = -1;
        for (size_t i = 0; i < roots; i++)
            if (bodies[i] == body)
                wheel_index[w] = (int64_t)i;
    }
    for (size_t i = 0; i < roots + 4; i++)
        for (size_t j = 0; j < i; j++)
            if (bodies[i] == bodies[j])
                return false;
    return true;
}
static bool capture_body(uintptr_t p, PoseBody &b) {
    int32_t counter = 0;
    bool ok =
        read_field(p + 0x58, b.state.mass.inverse_mass) &&
        read_field(p + 0xf8, b.state.mass.inverse_inertia_local) &&
        read_field(p + 0x40, b.state.position_world) &&
        read_field(p + 0x28, b.state.velocity_world) && read_field(p + 0x90, b.state.orientation) &&
        read_field(p + 0x110, b.state.momentum_world) && read_field(p + 0x60, b.inverse_world) &&
        read_field(p + 0xe0, b.omega) && read_field(p + 0x10, b.force) &&
        read_field(p + 0x128, b.torque) && read_field(p + 0xd8, counter);
    b.renormalization_counter = counter;
    return ok;
}
static uint64_t validate(PoseBody *b, size_t n, double &ie, double &we) {
    RigidSavedBody states[32];
    RigidMassProperties mass[32];
    RigidPreparedBody prepared[32];
    for (size_t i = 0; i < n; i++) {
        states[i] = b[i].state;
        mass[i] = states[i].mass;
    }
    auto result = rigid_prepare_restore(states, mass, n, prepared);
    if (result != RIGID_PREPARE_OK)
        return result;
    for (size_t i = 0; i < n; i++) {
        for (int j = 0; j < 6; j++) {
            if (!std::isfinite(b[i].inverse_world[j]))
                return 100;
            double e = std::fabs(prepared[i].inverse_inertia_world[j] - b[i].inverse_world[j]);
            if (e > ie)
                ie = e;
        }
        for (int j = 0; j < 3; j++) {
            if (!std::isfinite(b[i].omega[j]) || !std::isfinite(b[i].force[j]) ||
                !std::isfinite(b[i].torque[j]))
                return 101;
            double e = std::fabs(prepared[i].angular_velocity_world[j] - b[i].omega[j]);
            if (e > we)
                we = e;
        }
    }
    return result;
}
static void WINAPI contacts(void *tire, double dt, double horizon, void *world, double angular) {
    original_contacts(tire, dt, horizon, world, angular);
    if (!cycle)
        return;
    for (int i = 0; i < 4; i++)
        if (cycle->tires[i] == (uintptr_t)tire) {
            ++cycle->contacts[i];
            cycle->fresh |= 1ULL << i;
            return;
        }
}
static void WINAPI forces(void *object) {
    if (cycle && cycle->player == (uintptr_t)object) {
        if (cycle->force_calls < 6)
            cycle->masks[cycle->force_calls] = cycle->fresh;
        else
            ++cycle->errors;
        ++cycle->force_calls;
        cycle->fresh = 0;
    }
    original_forces(object);
}
static void WINAPI dynamics(void *object, double time) {
    LONG64 serial = InterlockedIncrement64(&ipc->callbacks);
    if (serial % 32 || cycle || !lease() || !std::isfinite(time) || !player_matches(object)) {
        original_dynamics(object, time);
        return;
    }
    LONG64 tid = GetCurrentThreadId(),
           prior = InterlockedCompareExchange64(&ipc->writer_thread, tid, 0);
    if (prior && prior != tid) {
        InterlockedIncrement64(&ipc->skipped);
        original_dynamics(object, time);
        return;
    }
    LARGE_INTEGER total_begin, begin, end, total_end;
    QueryPerformanceCounter(&total_begin);
    PoseFrame frame{};
    Cycle c{};
    c.player = (uintptr_t)object;
    uintptr_t bodies[32];
    size_t roots = 0;
    bool valid =
        collect_topology(c.player, bodies, roots, frame.aggregate_mask, c, frame.wheel_body_index);
    frame.root_count = roots;
    frame.total_count = roots + 4;
    if (valid)
        for (size_t i = 0; i < frame.total_count; i++)
            if (!capture_body(bodies[i], frame.before[i])) {
                valid = false;
                break;
            }
    if (!valid) {
        InterlockedIncrement64(&ipc->skipped);
        original_dynamics(object, time);
        return;
    }
    frame.prepare_before =
        validate(frame.before, frame.total_count, frame.inertia_error, frame.omega_error);
    cycle = &c;
    QueryPerformanceCounter(&begin);
    original_dynamics(object, time);
    QueryPerformanceCounter(&end);
    cycle = nullptr;
    uintptr_t later[32];
    size_t later_roots = 0;
    uint64_t aggregate = 0;
    Cycle after_cycle{};
    int64_t later_indices[4];
    valid = player_matches(object) &&
            collect_topology(c.player, later, later_roots, aggregate, after_cycle, later_indices) &&
            later_roots == roots && aggregate == frame.aggregate_mask &&
            memcmp(bodies, later, frame.total_count * 8) == 0;
    if (valid)
        for (size_t i = 0; i < frame.total_count; i++)
            if (!capture_body(bodies[i], frame.after[i])) {
                valid = false;
                break;
            }
    if (!valid || !lease()) {
        InterlockedIncrement64(&ipc->skipped);
        return;
    }
    frame.prepare_after =
        validate(frame.after, frame.total_count, frame.inertia_error, frame.omega_error);
    RigidSavedBody saved[32];
    RigidMassProperties current_mass[32];
    RigidPreparedBody prepared[32];
    for (size_t i = 0; i < frame.total_count; i++) {
        saved[i] = frame.before[i].state;
        current_mass[i] = frame.after[i].state.mass;
    }
    frame.prepare_saved_with_current_mass =
        rigid_prepare_restore(saved, current_mass, frame.total_count, prepared);
    frame.tick_ms = GetTickCount64();
    frame.simulation_time = time;
    frame.force_calls = c.force_calls;
    frame.phase_errors = c.errors;
    memcpy(frame.contacts, c.contacts, sizeof(c.contacts));
    memcpy(frame.fresh_mask_at_force, c.masks, sizeof(c.masks));
    const uint64_t expected[6] = {15, 0, 0, 15, 0, 0};
    if (c.force_calls != 6 || memcmp(c.masks, expected, sizeof(expected)))
        ++frame.phase_errors;
    for (int i = 0; i < 4; i++)
        if (c.contacts[i] != 2)
            ++frame.phase_errors;
    QueryPerformanceCounter(&total_end);
    frame.original_us = (end.QuadPart - begin.QuadPart) * 1e6 / ipc->qpc_frequency;
    frame.observer_total_us =
        (total_end.QuadPart - total_begin.QuadPart) * 1e6 / ipc->qpc_frequency;
    InterlockedIncrement64(&ipc->sequence);
    ipc->frame = frame;
    MemoryBarrier();
    InterlockedIncrement64(&ipc->sequence);
    InterlockedIncrement64(&ipc->observations);
}

static int check_game_hash(void) {
#ifdef LMU_POSE_FIXTURE
    return 1;
#else
    static const unsigned char expected[32] = {0xf,  0xe1, 0xf3, 0x57, 0x20, 0x94, 0xc2, 0xcb,
                                               0x77, 0xc1, 0xf0, 0xbe, 0x7a, 0x4a, 0x1b, 0x18,
                                               0xee, 0x70, 0x9a, 0x64, 0x5d, 0xdd, 0x3b, 0x94,
                                               0xe6, 0xec, 0x7,  0x7,  0x95, 0xdc, 0x51, 0x60};
    wchar_t path[32768];
    if (!GetModuleFileNameW(NULL, path, 32768))
        return 0;
    HANDLE file =
        CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return 0;
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    int ok = 0;
    if (CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
        CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
        unsigned char buffer[65536], digest[32];
        DWORD got = 0, size = 32;
        BOOL read_ok;
        do {
            read_ok = ReadFile(file, buffer, sizeof(buffer), &got, NULL);
            if (!read_ok || (!CryptHashData(hash, buffer, got, 0)))
                break;
        } while (got);
        if (read_ok && got == 0 && CryptGetHashParam(hash, HP_HASHVAL, digest, &size, 0) &&
            size == 32)
            ok = memcmp(digest, expected, 32) == 0;
    }
    if (hash)
        CryptDestroyHash(hash);
    if (provider)
        CryptReleaseContext(provider, 0);
    CloseHandle(file);
    return ok;
#endif
}

static DWORD WINAPI worker(void *) {
    wchar_t name[96];
    pose_mapping_name(name, GetCurrentProcessId());
    HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!mapping)
        return 0;
    ipc = (PoseIPC *)MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*ipc));
    if (!ipc || ipc->magic != POSE_MAGIC || ipc->version != 1 || ipc->pid != GetCurrentProcessId())
        return 0;
    ipc->status = 1;
    module_base = (uintptr_t)GetModuleHandleW(NULL);
    if (!check_game_hash()) {
        ipc->error = 100;
        ipc->status = 4;
        return 0;
    }
#ifdef LMU_POSE_FIXTURE
    targets[0] = (void *)GetProcAddress((HMODULE)module_base, "FixtureDynamics");
    targets[1] = (void *)GetProcAddress((HMODULE)module_base, "FixtureContacts");
    targets[2] = (void *)GetProcAddress((HMODULE)module_base, "FixtureForces");
#else
    const uintptr_t offsets[3] = {0xeb79e0, 0xee5030, 0xeb7030};
    const unsigned char prologs[3][22] = {
        {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x10, 0x48, 0x89, 0x70, 0x18, 0x48, 0x89, 0x78, 0x20},
        {0x4c, 0x89, 0x4c, 0x24, 0x20, 0x48, 0x89, 0x4c, 0x24, 0x08, 0x55,
         0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57},
        {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x70, 0x10, 0x48, 0x89, 0x78, 0x18,
         0x4c, 0x89, 0x70, 0x20}};
    const size_t lengths[3] = {15, 22, 19};
    for (int i = 0; i < 3; i++) {
        targets[i] = (void *)(module_base + offsets[i]);
        if (memcmp(targets[i], prologs[i], lengths[i])) {
            ipc->error = 101 + i;
            ipc->status = 4;
            return 0;
        }
    }
#endif
    for (int i = 0; i < 3; i++) {
        if (!targets[i]) {
            ipc->error = 105;
            ipc->status = 4;
            return 0;
        }
        memcpy(originals[i], targets[i], 32);
    }
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    ipc->qpc_frequency = freq.QuadPart;
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK) {
        ipc->error = 200 + status;
        ipc->status = 4;
        return 0;
    }
    void *hooks[3] = {(void *)&dynamics, (void *)&contacts, (void *)&forces};
    void **trampolines[3] = {(void **)&original_dynamics, (void **)&original_contacts,
                             (void **)&original_forces};
    for (int i = 0; i < 3; i++) {
        status = MH_CreateHook(targets[i], hooks[i], trampolines[i]);
        if (status != MH_OK) {
            ipc->error = 300 + status;
            ipc->status = 4;
            return 0;
        }
    }
    LONG64 now = GetTickCount64(), hb = ipc->heartbeat_ms;
    if (ipc->stop || hb > now || now - hb > 2000) {
        ipc->error = 106;
        ipc->status = 4;
        return 0;
    }
    status = MH_EnableHook(MH_ALL_HOOKS);
    if (status != MH_OK) {
        ipc->error = 400 + status;
        ipc->status = 4;
        return 0;
    }
    ipc->status = 2;
    ULONGLONG started = GetTickCount64();
    while (!ipc->stop) {
        now = GetTickCount64();
        hb = InterlockedCompareExchange64(&ipc->heartbeat_ms, 0, 0);
        if (hb > now || now - hb > 2000 || (ULONGLONG)now - started > 120000)
            break;
        Sleep(20);
    }
    ipc->allow_observation = 0;
    status = MH_DisableHook(MH_ALL_HOOKS);
    if (status != MH_OK) {
        ipc->error = 500 + status;
        ipc->status = 5;
        return 0;
    }
    for (int i = 0; i < 3; i++)
        if (memcmp(targets[i], originals[i], 32)) {
            ipc->error = 110 + i;
            ipc->status = 5;
            return 0;
        }
    ipc->status = 3;
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE t = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        if (t)
            CloseHandle(t);
    }
    return TRUE;
}
