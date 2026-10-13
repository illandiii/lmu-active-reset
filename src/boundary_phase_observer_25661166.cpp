/* Observe the relationship of the two natural callbacks. No state writes.
   A stable sample is evidence only, never permission to execute a reset. */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>
#include <cmath>
#include <cstring>
#include "boundary_ipc_v3_25661166.h"
#include "MinHook.h"
#include "native_lock_witness.h"
#include "native_lifetime_pin.h"
#ifdef LMU_BOUNDARY_HANDOFF
#include "native_lifetime_handoff.h"
static NativeLifetimeHandoff *handoff;
static volatile LONG64 pins_acquired, pins_valid, pins_released, control_owner_before;
#endif
static thread_local bool boundary_lock_held;
static PhaseIPC *ipc;
static uintptr_t module_base;
static void(WINAPI *original_dynamics)(void *, double);
static void(WINAPI *original_score)(void *, float);
static void *targets[3];
static unsigned char original[3][32];
static volatile LONG64 score_thread, physics_thread, score_calls, score_sequence, overlaps,
    thread_changes;
static void(WINAPI *original_boundary)(uint64_t);
static thread_local unsigned boundary_depth;
static volatile LONG64 boundary_thread, boundary_calls, outside_boundary;
static volatile LONG score_depth, physics_depth, publishing;
template <class T> static bool field(uintptr_t p, T &value) {
    SIZE_T got = 0;
    return p >= 0x10000 && p < 0x800000000000ULL &&
           ReadProcessMemory(GetCurrentProcess(), (void *)p, &value, sizeof(value), &got) &&
           got == sizeof(value);
}
static LONG64 number(volatile LONG64 *p) {
    return InterlockedCompareExchange64(p, 0, 0);
}
static bool lease() {
    LONG64 now = GetTickCount64(), hb = number(&ipc->heartbeat_ms);
    return !ipc->stop && ipc->allow_observation == 1 && hb <= now && now - hb <= 300;
}
static bool roots(uintptr_t &owner, uintptr_t &physics, uintptr_t &scoring) {
#ifdef LMU_PHASE_FIXTURE
    using F = int(WINAPI *)(uintptr_t *, uintptr_t *, uintptr_t *);
    auto f = (F)GetProcAddress((HMODULE)module_base, "FixtureRoots");
    return f && f(&owner, &physics, &scoring);
#else
    uintptr_t vehicle = 0, back = 0;
    if (!field(module_base + 0x1e194c8, owner) || !field(owner + 0x159a8, physics) ||
        !field(owner, vehicle) || !physics || !vehicle)
        return false;
    scoring = vehicle + 0x1cec8;
    return field(scoring, back) && back == owner;
#endif
}
static void note_thread(volatile LONG64 *value) {
    LONG64 tid = GetCurrentThreadId(), old = InterlockedCompareExchange64(value, tid, 0);
    if (old && old != tid)
        InterlockedIncrement64(&thread_changes);
}
static NativeLockWitness lock_witness() {
#ifdef LMU_PHASE_FIXTURE
    using F = uintptr_t(WINAPI *)();
    auto f = (F)GetProcAddress((HMODULE)module_base, "FixtureLock");
    return native_lock_witness(f ? f() : 0);
#else
    return native_lock_witness(module_base + 0x3b8a968);
#endif
}
static uintptr_t control_lock_address() {
#ifdef LMU_PHASE_FIXTURE
    using F = uintptr_t(WINAPI *)();
    auto f = (F)GetProcAddress((HMODULE)module_base, "FixtureControlLock");
    return f ? f() : 0;
#else
    return module_base + 0x1a5a5c8;
#endif
}
static uintptr_t worker_lock_address() {
#ifdef LMU_PHASE_FIXTURE
    using F = uintptr_t(WINAPI *)();
    auto f = (F)GetProcAddress((HMODULE)module_base, "FixtureLock");
    return f ? f() : 0;
#else
    return module_base + 0x3b8a968;
#endif
}
#ifdef LMU_BOUNDARY_HANDOFF
static uintptr_t gate_address() {
#ifdef LMU_PHASE_FIXTURE
    using F = uintptr_t(WINAPI *)();
    auto f = (F)GetProcAddress((HMODULE)module_base, "FixtureGate");
    return f ? f() : 0;
#else
    return module_base + 0x1a5a5f0;
#endif
}
#endif
static void WINAPI boundary(uint64_t input) {
    bool prior = boundary_lock_held;
    boundary_lock_held = lock_witness().held_by_caller;
    ++boundary_depth;
    if (lease()) {
        note_thread(&boundary_thread);
        InterlockedIncrement64(&boundary_calls);
#ifdef LMU_BOUNDARY_HANDOFF
        if (boundary_depth == 1 && boundary_lock_held && !number(&thread_changes)) {
            NativeLifetimePin pin{};
            InterlockedExchange64(&control_owner_before,
                                  native_lock_witness(control_lock_address()).owner);
            if (native_handoff_try_pin(handoff, &pin)) {
                InterlockedIncrement64(&pins_acquired);
                if (native_lifetime_valid(&pin))
                    InterlockedIncrement64(&pins_valid);
                if (native_lifetime_release(&pin))
                    InterlockedIncrement64(&pins_released);
            }
        }
#endif
    }
    original_boundary(input);
    --boundary_depth;
    boundary_lock_held = prior;
}
static void WINAPI score(void *object, float dt) {
    uintptr_t o = 0, p = 0, s = 0;
    bool selected = lease() && std::isfinite(dt) && roots(o, p, s) && s == (uintptr_t)object;
    if (!selected) {
        original_score(object, dt);
        return;
    }
    if (!boundary_depth)
        InterlockedIncrement64(&outside_boundary);
    note_thread(&score_thread);
    InterlockedIncrement64(&score_calls);
    InterlockedIncrement64(&score_sequence);
    InterlockedIncrement(&score_depth);
    if (InterlockedCompareExchange(&physics_depth, 0, 0))
        InterlockedIncrement64(&overlaps);
    original_score(object, dt);
    InterlockedDecrement(&score_depth);
    InterlockedIncrement64(&score_sequence);
}
static void WINAPI dynamics(void *object, double dt) {
    uintptr_t o = 0, p = 0, s = 0;
    bool selected = lease() && std::isfinite(dt) && roots(o, p, s) && p == (uintptr_t)object;
    if (!selected) {
        original_dynamics(object, dt);
        return;
    }
    if (!boundary_depth)
        InterlockedIncrement64(&outside_boundary);
    note_thread(&physics_thread);
    LONG64 serial = InterlockedIncrement64(&ipc->callbacks);
    InterlockedIncrement(&physics_depth);
    if (InterlockedCompareExchange(&score_depth, 0, 0))
        InterlockedIncrement64(&overlaps);
#ifdef LMU_PHASE_FIXTURE
    const int stride = 1;
#else
    const int stride = 320;
#endif
    if (serial % stride == 0 && InterlockedCompareExchange(&publishing, 1, 0) == 0) {
        PhaseFrame f{};
        f.tick_ms = GetTickCount64();
        f.dynamics_thread = GetCurrentThreadId();
        f.scoring_thread = number(&score_thread);
        f.scoring_calls = number(&score_calls);
        f.overlaps = number(&overlaps);
        f.thread_changes = number(&thread_changes);
        f.owner = o;
        f.physics = p;
        f.scoring = s;
        f.boundary_thread = number(&boundary_thread);
        f.boundary_calls = number(&boundary_calls);
        f.boundary_depth = boundary_depth;
        f.outside_boundary = number(&outside_boundary);
        auto lock = lock_witness();
        f.lock_owner = lock.owner;
        f.lock_recursion = lock.recursion;
        f.boundary_lock_held = boundary_lock_held;
        LONG64 seq = number(&score_sequence);
        int32_t flags[3];
        uintptr_t o2 = 0, p2 = 0, s2 = 0;
        bool valid = field(s + 0x160, flags[0]) && field(s + 0x27290, flags[1]) &&
                     field(s + 0x27360, flags[2]);
        for (int i = 0; i < 3 && valid; i++)
            valid = flags[i] >= 0 && flags[i] <= 2;
        f.score_sequence = number(&score_sequence);
        f.stable = boundary_lock_held && lock.held_by_caller && boundary_depth == 1 &&
                   f.boundary_thread == f.dynamics_thread && valid &&
                   seq == (LONG64)f.score_sequence && !(seq & 1) &&
                   !InterlockedCompareExchange(&score_depth, 0, 0) && roots(o2, p2, s2) &&
                   o2 == o && p2 == p && s2 == s && f.scoring_thread == f.dynamics_thread &&
                   !f.thread_changes && !f.overlaps;
#ifdef LMU_BOUNDARY_HANDOFF
        // Cumulative counters preserve short admission windows between samples.
        f.control_owner_before = number(&control_owner_before);
        f.pin_acquired = number(&pins_acquired);
        f.pin_valid = number(&pins_valid);
        f.pin_released = number(&pins_released);
#else
        if (f.stable) {
            auto control = control_lock_address();
            f.control_owner_before = native_lock_witness(control).owner;
            NativeLifetimePin pin{};
            if (native_lifetime_try_acquire(&pin, worker_lock_address(), control)) {
                f.pin_acquired = 1;
                f.pin_valid = native_lifetime_valid(&pin);
                f.pin_released = native_lifetime_release(&pin);
            }
        }
#endif
        if (valid && lease()) {
            f.current = flags[0];
            f.remembered = flags[1];
            f.last_computed = flags[2];
            ipc->writer_thread = GetCurrentThreadId();
            InterlockedIncrement64(&ipc->sequence);
            ipc->frame = f;
            MemoryBarrier();
            InterlockedIncrement64(&ipc->sequence);
            InterlockedIncrement64(&ipc->observations);
        } else
            InterlockedIncrement64(&ipc->skipped);
        InterlockedExchange(&publishing, 0);
    }
    original_dynamics(object, dt);
    InterlockedDecrement(&physics_depth);
}
static int check_game_hash(void) {
#ifdef LMU_PHASE_FIXTURE
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
    phase_mapping_name(name, GetCurrentProcessId());
    HANDLE map = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!map)
        return 0;
    ipc = (PhaseIPC *)MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*ipc));
    if (!ipc || ipc->magic != PHASE_MAGIC || ipc->version != 1 || ipc->pid != GetCurrentProcessId())
        return 0;
    ipc->status = 1;
    module_base = (uintptr_t)GetModuleHandleW(nullptr);
    if (!check_game_hash()) {
        ipc->error = 100;
        ipc->status = 4;
        return 0;
    }
#ifdef LMU_PHASE_FIXTURE
    targets[0] = (void *)GetProcAddress((HMODULE)module_base, "FixtureDynamics");
    targets[1] = (void *)GetProcAddress((HMODULE)module_base, "FixtureScore");
    targets[2] = (void *)GetProcAddress((HMODULE)module_base, "FixtureBoundary");
#else
    targets[0] = (void *)(module_base + 0xecb4e0);
    targets[1] = (void *)(module_base + 0xcf19f0);
    targets[2] = (void *)(module_base + 0xcd1f40);
    const unsigned char d[] = {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x10, 0x48,
                               0x89, 0x70, 0x18, 0x48, 0x89, 0x78, 0x20};
    const unsigned char s[] = {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x20, 0xf3,
                               0x0f, 0x11, 0x48, 0x10, 0x55, 0x56, 0x57};
    if (memcmp(targets[0], d, sizeof(d)) || memcmp(targets[1], s, sizeof(s)) ||
        memcmp(targets[2], d, sizeof(d))) {
        ipc->error = 101;
        ipc->status = 4;
        return 0;
    }
#endif
    for (int i = 0; i < 3; i++) {
        if (!targets[i]) {
            ipc->error = 102;
            ipc->status = 4;
            return 0;
        }
        memcpy(original[i], targets[i], 32);
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
    void *hooks[3] = {(void *)dynamics, (void *)score, (void *)boundary};
    void **trampoline[3] = {(void **)&original_dynamics, (void **)&original_score,
                            (void **)&original_boundary};
    for (int i = 0; i < 3; i++) {
        status = MH_CreateHook(targets[i], hooks[i], trampoline[i]);
        if (status != MH_OK) {
            ipc->error = 300 + status;
            ipc->status = 4;
            return 0;
        }
    }
    LONG64 now = GetTickCount64(), hb = number(&ipc->heartbeat_ms);
    if (ipc->stop || hb > now || now - hb > 2000) {
        ipc->error = 103;
        ipc->status = 4;
        return 0;
    }
#ifdef LMU_BOUNDARY_HANDOFF
    handoff = native_handoff_create(worker_lock_address(), control_lock_address(), gate_address());
    if (!handoff) {
        ipc->error = 110;
        ipc->status = 4;
        return 0;
    }
#endif
    status = MH_EnableHook(MH_ALL_HOOKS);
    if (status != MH_OK) {
#ifdef LMU_BOUNDARY_HANDOFF
        native_handoff_destroy(handoff);
        handoff = nullptr;
#endif
        ipc->error = 400 + status;
        ipc->status = 4;
        return 0;
    }
    ipc->status = 2;
    ULONGLONG start = GetTickCount64();
#ifdef LMU_BOUNDARY_HANDOFF
    ULONGLONG next_request = start;
#endif
    while (!ipc->stop) {
        now = GetTickCount64();
        hb = number(&ipc->heartbeat_ms);
        if (hb > now || now - hb > 2000 || (ULONGLONG)now - start > 120000)
            break;
#ifdef LMU_BOUNDARY_HANDOFF
        if (!lease())
            native_handoff_cancel(handoff);
        else if (static_cast<ULONGLONG>(now) >= next_request) {
            native_handoff_request(handoff, 25);
            next_request = static_cast<ULONGLONG>(now) + 500;
        }
#endif
        Sleep(20);
    }
    ipc->allow_observation = 0;
    ipc->stop = 1;
#ifdef LMU_BOUNDARY_HANDOFF
    // Join the helper before detaching. Keep its storage resident because a
    // callback may still return through a trampoline after hooks are disabled.
    // Its storage/handles are reclaimed on process exit, like the DLL.
    if (!native_handoff_shutdown(handoff)) {
        ipc->error = 111;
        ipc->status = 5;
        return 0;
    }
#endif
    status = MH_DisableHook(MH_ALL_HOOKS);
    if (status != MH_OK) {
        ipc->error = 500 + status;
        ipc->status = 5;
        return 0;
    }
    for (int i = 0; i < 3; i++)
        if (memcmp(original[i], targets[i], 32)) {
            ipc->error = 104 + i;
            ipc->status = 5;
            return 0;
        }
    ipc->status = 3;
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE t = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
        if (t)
            CloseHandle(t);
    }
    return TRUE;
}
