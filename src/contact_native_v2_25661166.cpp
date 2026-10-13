/* Exact-build research module. Natural EF8880 calls are forwarded once.
   Default: observation only. A separately leased, single-use request may retire
   the current geometric contacts at rest. It does NOT restore a checkpoint. */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>
#include <stdint.h>
#include <cstring>
#include <cmath>
#include <optional>
#include <utility>
#include "contact_ipc_v2_25661166.h"
#include "MinHook.h"
#ifndef LMU_CONTACT_FIXTURE
#include "SharedMemoryInterface.hpp"
static_assert(sizeof(SharedMemoryLayout) == 324824, "Pinned SDK size");
#endif
static ContactIPC *ipc;
static uintptr_t module_base;
static void(WINAPI *original_refresh)(void *, double, double, void *, double);
static void *target;
static unsigned char target_before[32];
static uint64_t wheel_calls[4], good_samples[4];
static uintptr_t observed_player;
static uint64_t accepted_request;
static LONG attempted_mask;
static bool self_read(uintptr_t p, void *out, size_t size) {
    SIZE_T got = 0;
    return p >= 0x10000 && p < 0x800000000000ULL &&
           ReadProcessMemory(GetCurrentProcess(), (void *)p, out, size, &got) && got == size;
}
template <class T> static bool field(uintptr_t p, T &out) {
    return self_read(p, &out, sizeof(out));
}
static bool lease(bool rebuild = false) {
    LONG64 now = (LONG64)GetTickCount64();
    LONG64 hb = InterlockedCompareExchange64(&ipc->heartbeat_ms, 0, 0);
    if (ipc->stop || ipc->allow_observation != 1 || hb > now || now - hb > 300)
        return false;
    if (rebuild && (ipc->allow_rebuild != 1 || ipc->request_deadline_ms < now))
        return false;
    return true;
}
static int player_wheel(void *object, uintptr_t &player) {
#ifdef LMU_CONTACT_FIXTURE
    typedef int(WINAPI * F)(void *, uintptr_t *);
    auto f = (F)GetProcAddress((HMODULE)module_base, "FixtureWheel");
    return f ? f(object, &player) : -1;
#else
    uintptr_t owner = 0, tire = 0;
    if (!field(module_base + 0x1e194c8, owner) || !field(owner + 0x159a8, player))
        return -1;
    for (int i = 0; i < 4; i++)
        if (field(player + 0x618 + i * 0x6b0 + 0x3e8, tire) && tire == (uintptr_t)object)
            return i;
    return -1;
#endif
}
struct Node {
    uintptr_t address;
    uint32_t index;
    uint16_t surface_index;
    uint8_t active, aux;
    double depth, normal[3], plane;
};
struct Snapshot {
    Node nodes[CONTACT_MAX];
    uint64_t count, surfaces, bad, positive;
    double min_depth, max_depth, normal_min, normal_max, position[3];
};
static bool vector_size(const uintptr_t *v, size_t stride, size_t limit, size_t &count) {
    if (v[0] == 0 && v[1] == 0 && v[2] == 0) {
        count = 0;
        return true;
    }
    if (v[0] < 0x10000 || v[1] < v[0] || v[2] < v[1] || (v[1] - v[0]) % stride ||
        (v[2] - v[0]) % stride)
        return false;
    count = (v[1] - v[0]) / stride;
    return count <= limit;
}
static bool snapshot(uintptr_t tire, Snapshot &s) {
    s.count = s.surfaces = s.bad = s.positive = 0;
    s.min_depth = s.normal_min = 1e300;
    s.max_depth = -1e300;
    s.normal_max = 0;
    uintptr_t active[3], surfaces[3], used[3], pool = 0;
    size_t count = 0, sc = 0, uc = 0;
    if (!field(tire + 0x318, pool) || !field(tire + 0x9f0, active) ||
        !field(tire + 0xa60, surfaces) || !field(tire + 0xa78, used) ||
        !vector_size(active, 8, CONTACT_MAX, count) || !vector_size(surfaces, 0x58, 1024, sc) ||
        !vector_size(used, 4, 1024, uc) || !field(tire + 0xb00, s.position))
        return false;
    for (double x : s.position)
        if (!std::isfinite(x))
            return false;
    s.count = count;
    s.surfaces = sc;
    uintptr_t nodes[CONTACT_MAX];
    uint32_t bits[1024];
    if (count && !self_read(active[0], nodes, count * 8))
        return false;
    if (uc && !self_read(used[0], bits, uc * 4))
        return false;
    for (size_t i = 0; i < count; i++) {
        Node &n = s.nodes[i];
        n.address = nodes[i];
        if (pool < 0x10000 || nodes[i] < pool || (nodes[i] - pool) % 0x110 ||
            (nodes[i] - pool) / 0x110 >= 32768)
            return false;
        n.index = (uint32_t)((nodes[i] - pool) / 0x110);
        for (size_t j = 0; j < i; j++)
            if (nodes[j] == nodes[i])
                return false;
        unsigned char flags[8];
        double geo[5];
        uintptr_t surface = 0, cached = 0;
        if (!field(nodes[i] + 0x60, flags) || !field(nodes[i] + 0xd0, geo) ||
            !field(nodes[i] + 0xf8, surface))
            return false;
        memcpy(&n.surface_index, flags + 2, 2);
        n.active = flags[0];
        n.aux = flags[7];
        n.depth = geo[0];
        memcpy(n.normal, geo + 1, 24);
        n.plane = geo[4];
        for (double x : geo)
            if (!std::isfinite(x))
                return false;
        double norm = std::sqrt(geo[1] * geo[1] + geo[2] * geo[2] + geo[3] * geo[3]);
        if (n.surface_index >= sc || n.surface_index / 32 >= uc)
            return false;
        if (!field(surfaces[0] + n.surface_index * 0x58, cached))
            return false;
        if (n.active != 1 || !surface || surface != cached ||
            !(bits[n.surface_index / 32] & (1u << (n.surface_index % 32))) || norm < .999 ||
            norm > 1.001)
            ++s.bad;
        if (n.depth > 0)
            ++s.positive;
        if (n.depth < s.min_depth)
            s.min_depth = n.depth;
        if (n.depth > s.max_depth)
            s.max_depth = n.depth;
        if (norm < s.normal_min)
            s.normal_min = norm;
        if (norm > s.normal_max)
            s.normal_max = norm;
    }
    if (!count)
        s.min_depth = s.max_depth = s.normal_min = s.normal_max = 0;
    return true;
}
static bool writable_depths(const Snapshot &s) {
    for (size_t i = 0; i < s.count; i++) {
        MEMORY_BASIC_INFORMATION m{};
        uintptr_t p = s.nodes[i].address + 0xd0;
        if (!VirtualQuery((void *)p, &m, sizeof(m)) || m.State != MEM_COMMIT ||
            m.Protect & (PAGE_GUARD | PAGE_NOACCESS) ||
            !(m.Protect & (PAGE_READWRITE | PAGE_WRITECOPY)) ||
            p + 8 > (uintptr_t)m.BaseAddress + m.RegionSize)
            return false;
    }
    return true;
}
struct LockWords {
    volatile LONG waiters, busy;
};
#ifndef LMU_CONTACT_FIXTURE
static const SharedMemoryLayout *native_data;
static LockWords *native_lock;
static HANDLE native_event;
#endif
static bool native_stationary_gate(uintptr_t player) {
#ifdef LMU_CONTACT_FIXTURE
    typedef int(WINAPI * F)(void);
    auto f = (F)GetProcAddress((HMODULE)module_base, "FixtureStationary");
    return f && f() != 0;
#else
    if (!native_data || !native_lock || !native_event)
        return false;
    if (InterlockedCompareExchange(&native_lock->busy, 1, 0) != 0)
        return false;
    // Nonblocking SDK lock; no allocation, native calls or physics writes held.
    const auto &d = native_data->data;
    const auto &sc = d.scoring.scoringInfo;
    bool ok = d.generic.gameVersion == 14200 && sc.mSession >= 1 && sc.mSession <= 4 &&
              sc.mGameMode == 0 && sc.mGamePhase == 5 && sc.mInRealtime && sc.mNumVehicles == 1 &&
              d.telemetry.activeVehicles == 1 && d.telemetry.playerHasVehicle &&
              d.telemetry.playerVehicleIdx == 0;
    if (ok) {
        const auto &v = d.scoring.vehScoringInfo[0];
        const auto &t = d.telemetry.telemInfo[0];
        double speed2 = t.mLocalVel.x * t.mLocalVel.x + t.mLocalVel.y * t.mLocalVel.y +
                        t.mLocalVel.z * t.mLocalVel.z;
        ok = v.mIsPlayer && v.mControl == 0 && v.mID == t.mID && v.mInPits && t.mGear == 0 &&
             t.mUnfilteredThrottle <= .01 && std::isfinite(speed2) && speed2 <= .0001;
    }
    InterlockedExchange(&native_lock->busy, 0);
    if (InterlockedCompareExchange(&native_lock->waiters, 0, 0) > 0)
        SetEvent(native_event);
    if (!ok)
        return false;
    // Use the verified current root-body vector, not an old inline velocity offset.
    uintptr_t roots[3], body[28];
    size_t count = 0;
    if (!field(player + 0x23f8, roots) || !vector_size(roots, 8, 28, count) || !count ||
        !self_read(roots[0], body, count * 8))
        return false;
    for (size_t i = 0; i < count; i++) {
        double velocity[3], omega[3];
        if (!field(body[i] + 0x28, velocity) || !field(body[i] + 0xe0, omega))
            return false;
        double speed2 = 0, spin2 = 0;
        for (int k = 0; k < 3; k++) {
            if (!std::isfinite(velocity[k]) || !std::isfinite(omega[k]))
                return false;
            speed2 += velocity[k] * velocity[k];
            spin2 += omega[k] * omega[k];
        }
        if (speed2 > .0001 || spin2 > .0001)
            return false;
    }
    return true;
#endif
}
static bool args_valid(double dt, double horizon, void *world, double angular) {
    bool ok = std::isfinite(dt) && dt > 0 && dt <= .01 && std::isfinite(horizon) && horizon >= dt &&
              horizon <= .02 && std::isfinite(angular) && angular >= 0 && angular < 5000;
#ifndef LMU_CONTACT_FIXTURE
    ok = ok && (uintptr_t)world == module_base + 0x1aa4090;
#else
    ok = ok && world == (void *)0x12345678;
#endif
    return ok;
}
static void WINAPI refresh(void *object, double dt, double horizon, void *world, double angular) {
    InterlockedIncrement64(&ipc->callbacks);
    uintptr_t player = 0;
    int wheel = -1;
    if (lease() && args_valid(dt, horizon, world, angular))
        wheel = player_wheel(object, player);
    if (wheel < 0) {
        original_refresh(object, dt, horizon, world, angular);
        return;
    }
    LONG64 tid = GetCurrentThreadId();
    LONG64 previous = InterlockedCompareExchange64(&ipc->writer_thread, tid, 0);
    if (previous && previous != tid) {
        InterlockedIncrement64(&ipc->skipped);
        original_refresh(object, dt, horizon, world, angular);
        return;
    }
    if (observed_player && observed_player != player) {
        ipc->allow_rebuild = 0;
        ipc->stop = 1;
        original_refresh(object, dt, horizon, world, angular);
        return;
    }
    observed_player = player;
    uint64_t request = (uint64_t)InterlockedCompareExchange64(&ipc->request_id, 0, 0);
    bool pending = request == 1 && (!accepted_request || accepted_request == request) &&
                   !(attempted_mask & (1 << wheel)) && lease(true);
    bool observe = (++wheel_calls[wheel] % 32) == 0 || pending;
    if (!observe) {
        original_refresh(object, dt, horizon, world, angular);
        return;
    }
    Snapshot before, after;
    bool valid_pre = snapshot((uintptr_t)object, before);
    bool native_ready = native_stationary_gate(player);
    bool perform = pending && good_samples[wheel] >= 64 && valid_pre && !before.bad &&
                   before.count > 0 && before.positive > 0 && native_ready &&
                   writable_depths(before) && lease(true);
    // One-sided sentinel is interpreted by the ORIGINAL removal loop. Never
    // manually erase the vector, flags, cached indices, allocator or pointers.
    if (perform) {
        accepted_request = request;
        ipc->request_seen = request;
        attempted_mask |= 1 << wheel;
        const uint64_t sentinel = 0xc7efffffe0000000ULL;
        for (size_t i = 0; i < before.count; i++)
            memcpy((void *)(before.nodes[i].address + 0xd0), &sentinel, 8);
        ipc->physics_writes += before.count;
    }
    LARGE_INTEGER begin, end;
    QueryPerformanceCounter(&begin);
    original_refresh(object, dt, horizon, world, angular);
    QueryPerformanceCounter(&end);
    bool valid_post = snapshot((uintptr_t)object, after);
    ContactResult r{};
    r.wheel = wheel;
    r.tick_ms = GetTickCount64();
    r.samples = ++good_samples[wheel];
    r.pre_count = valid_pre ? before.count : 0;
    r.post_count = valid_post ? after.count : 0;
    r.pre_bad = valid_pre ? before.bad : UINT64_MAX;
    r.post_bad = valid_post ? after.bad : UINT64_MAX;
    r.native_stationary = native_ready;
    r.dt = dt;
    r.horizon = horizon;
    r.angular_speed = angular;
    r.duration_us = (end.QuadPart - begin.QuadPart) * 1e6 / ipc->qpc_frequency;
    if (valid_pre)
        r.pre_min_depth = before.min_depth;
    if (valid_post) {
        r.post_min_depth = after.min_depth;
        r.post_max_depth = after.max_depth;
        r.normal_min = after.normal_min;
        r.normal_max = after.normal_max;
        r.post_positive = after.positive;
        r.surface_count = after.surfaces;
    }
    if (valid_pre && valid_post) {
        for (size_t i = 0; i < before.count; i++) {
            bool found = false;
            for (size_t j = 0; j < after.count; j++)
                if (before.nodes[i].index == after.nodes[j].index)
                    found = true;
            if (!found)
                ++r.removed;
        }
        for (size_t i = 0; i < after.count; i++) {
            bool found = false;
            for (size_t j = 0; j < before.count; j++)
                if (after.nodes[i].index == before.nodes[j].index)
                    found = true;
            if (!found)
                ++r.added;
            if (!after.nodes[i].aux)
                ++r.cold_flags_clear;
        }
        double d2 = 0;
        for (int i = 0; i < 3; i++) {
            double d = after.position[i] - before.position[i];
            d2 += d * d;
        }
        r.position_delta = std::sqrt(d2);
    }
    if (!valid_pre || !valid_post || before.bad || after.bad) {
        good_samples[wheel] = 0;
        InterlockedIncrement64(&ipc->skipped);
    }
    if (perform)
        r.retired = before.count;
    InterlockedIncrement64(&ipc->sequence);
    ipc->latest[wheel] = r;
    if (perform) {
        ipc->rebuilt[wheel] = r;
        // A stationary cold reconstruction should retain contacts, unit normals,
        // valid surface links, cleared history flag and the unmodified body pose.
        if (valid_post && !after.bad && after.count > 0 && after.positive > 0 &&
            r.cold_flags_clear == after.count && r.position_delta < 1e-12)
            InterlockedOr(&ipc->completed_mask, 1 << wheel);
        else {
            InterlockedOr(&ipc->failed_mask, 1 << wheel);
            ipc->allow_rebuild = 0;
            ipc->stop = 1;
        }
    }
    MemoryBarrier();
    InterlockedIncrement64(&ipc->sequence);
    InterlockedIncrement64(&ipc->observations);
}

static int check_game_hash(void) {
#ifdef LMU_CONTACT_FIXTURE
    return 1;
#else
    static const unsigned char expected[32] = {0x66, 0x94, 0x23, 0x37, 0x81, 0x3b, 0x30, 0x1b,
                                               0x87, 0xd1, 0x2e, 0xdf, 0xa3, 0x30, 0xf3, 0x33,
                                               0x54, 0xa2, 0xa4, 0xb8, 0xcd, 0x9b, 0x04, 0x88,
                                               0x85, 0xcb, 0xf7, 0x7e, 0xb6, 0xf9, 0x40, 0xcf};
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
    contact_mapping_name(name, GetCurrentProcessId());
    HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!mapping)
        return 0;
    ipc = (ContactIPC *)MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*ipc));
    if (!ipc || ipc->magic != CONTACT_MAGIC || ipc->version != 1 ||
        ipc->pid != GetCurrentProcessId())
        return 0;
    ipc->status = 1;
    module_base = (uintptr_t)GetModuleHandleW(NULL);
    if (!check_game_hash()) {
        ipc->error = 100;
        ipc->status = 4;
        return 0;
    }
#ifdef LMU_CONTACT_FIXTURE
    target = (void *)GetProcAddress((HMODULE)module_base, "FixtureRefresh");
#else
    target = (void *)(module_base + 0xef8880);
    const unsigned char prolog[] = {0x4c, 0x89, 0x4c, 0x24, 0x20, 0x48, 0x89, 0x4c,
                                    0x24, 0x08, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54,
                                    0x41, 0x55, 0x41, 0x56, 0x41, 0x57};
    if (memcmp(target, prolog, sizeof(prolog))) {
        ipc->error = 101;
        ipc->status = 4;
        return 0;
    }
    HANDLE data_mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, "LMU_Data");
    HANDLE lock_mapping =
        OpenFileMappingA(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, "LMU_SharedMemoryLockData");
    native_event = OpenEventA(EVENT_MODIFY_STATE, FALSE, "LMU_SharedMemoryLockEvent");
    if (data_mapping)
        native_data = (const SharedMemoryLayout *)MapViewOfFile(data_mapping, FILE_MAP_READ, 0, 0,
                                                                sizeof(SharedMemoryLayout));
    if (lock_mapping)
        native_lock = (LockWords *)MapViewOfFile(lock_mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0,
                                                 sizeof(LockWords));
    if (!native_data || !native_lock || !native_event) {
        ipc->error = 105;
        ipc->status = 4;
        return 0;
    }
#endif
    if (!target) {
        ipc->error = 102;
        ipc->status = 4;
        return 0;
    }
    memcpy(target_before, target, 32);
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    ipc->qpc_frequency = freq.QuadPart;
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK) {
        ipc->error = 200 + status;
        ipc->status = 4;
        return 0;
    }
    status = MH_CreateHook(target, (void *)&refresh, (void **)&original_refresh);
    if (status != MH_OK) {
        ipc->error = 300 + status;
        ipc->status = 4;
        return 0;
    }
    LONG64 now = GetTickCount64(), hb = ipc->heartbeat_ms;
    if (ipc->stop || hb > now || now - hb > 2000) {
        ipc->error = 103;
        ipc->status = 4;
        return 0;
    }
    status = MH_EnableHook(target);
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
        if (hb > now || now - hb > 2000 || (ULONGLONG)now - started > 240000)
            break;
        Sleep(20);
    }
    ipc->allow_rebuild = 0;
    ipc->allow_observation = 0;
    status = MH_DisableHook(target);
    if (status != MH_OK) {
        ipc->error = 500 + status;
        ipc->status = 5;
        return 0;
    }
    if (memcmp(target, target_before, 32)) {
        ipc->error = 104;
        ipc->status = 5;
        return 0;
    }
    ipc->status = 3;
    // No unsafe hot-unload: mapping, native read views and trampoline stay resident.
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
