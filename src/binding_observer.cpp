/* Build 14200 thread-bound observations only. No reset entry or physics writes. */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>
#include <cmath>
#include <cstring>
#include "binding_observer_ipc.h"
#include "MinHook.h"
static BindingIPC *ipc;
static uintptr_t module_base;
static void(WINAPI *original_dynamics)(void *, double);
static void(WINAPI *original_transform)(void *, void *);
static void *targets[2];
static unsigned char originals[2][32];
struct Cycle {
    uintptr_t player;
    BindingFrame *frame;
    bool valid;
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
static uintptr_t player() {
#ifdef LMU_BINDING_FIXTURE
    typedef void *(WINAPI * F)();
    auto f = (F)GetProcAddress((HMODULE)module_base, "FixturePlayer");
    return f ? (uintptr_t)f() : 0;
#else
    uintptr_t owner = 0, p = 0;
    return read_field(module_base + 0x1e161e8, owner) && read_field(owner + 0x159a0, p) ? p : 0;
#endif
}
static bool body(uintptr_t p, RigidSavedBody &b) {
    return read_field(p + 0x58, b.mass.inverse_mass) &&
           read_field(p + 0xf8, b.mass.inverse_inertia_local) &&
           read_field(p + 0x40, b.position_world) && read_field(p + 0x28, b.velocity_world) &&
           read_field(p + 0x90, b.orientation) && read_field(p + 0x110, b.momentum_world);
}
static bool sample_wheel(uintptr_t p, int i, BindingSample &s, uintptr_t expected_hub = 0) {
    uintptr_t w = p + 0x618 + i * 0x6b0, wheel = 0, hub = 0, rear = 0;
    unsigned char detached = 0, deformed = 0;
    if (!read_field(w + 0x1d0, wheel) || !read_field(p + 0x20d8, rear))
        return false;
    if (i >= 2 && rear)
        hub = rear;
    else if (!read_field(w + 0x1d8, hub))
        return false;
    if (expected_hub && expected_hub != hub)
        return false;
    if (!read_field(w + 0x301, detached) || !read_field(w + 0x302, deformed) ||
        !read_field(w + 0x120, s.spins[i].angle) || !read_field(w + 0x128, s.spins[i].angular_rate))
        return false;
    s.spins[i].attached = detached ? 0 : 1;
    s.spins[i].deformed = deformed;
    return body(hub, s.hubs[i]) && body(wheel, s.wheels[i]);
}
static void validate(BindingSample &s) {
    WheelPreparedSpin prepared[4];
    s.validation = wheel_prepare_consistent_checkpoint(s.spins, s.hubs, s.wheels, prepared);
}
static bool sample(uintptr_t p, BindingSample &s) {
    for (int i = 0; i < 4; i++)
        if (!sample_wheel(p, i, s))
            return false;
    validate(s);
    return true;
}
static void WINAPI transform(void *object, void *hub) {
    original_transform(object, hub);
    if (!cycle)
        return;
    for (int i = 0; i < 4; i++)
        if ((uintptr_t)object == cycle->player + 0x618 + i * 0x6b0) {
            ++cycle->frame->wheel_calls[i];
            if (!sample_wheel(cycle->player, i, cycle->frame->last_transform, (uintptr_t)hub)) {
                cycle->valid = false;
                ++cycle->frame->hub_mismatches;
            }
            return;
        }
}
static void WINAPI dynamics(void *object, double time) {
    LONG64 serial = InterlockedIncrement64(&ipc->callbacks);
    if (serial % 32 || cycle || !lease() || !std::isfinite(time) || player() != (uintptr_t)object) {
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
    LARGE_INTEGER start, begin, end, finish;
    QueryPerformanceCounter(&start);
    BindingFrame frame{};
    Cycle current{(uintptr_t)object, &frame, true};
    if (!sample(current.player, frame.before)) {
        InterlockedIncrement64(&ipc->skipped);
        original_dynamics(object, time);
        return;
    }
    cycle = &current;
    QueryPerformanceCounter(&begin);
    original_dynamics(object, time);
    QueryPerformanceCounter(&end);
    cycle = nullptr;
    if (!current.valid || !lease() || player() != current.player ||
        !sample(current.player, frame.after)) {
        InterlockedIncrement64(&ipc->skipped);
        return;
    }
    for (int i = 0; i < 4; i++)
        if (!frame.wheel_calls[i]) {
            InterlockedIncrement64(&ipc->skipped);
            return;
        }
    validate(frame.last_transform);
    frame.tick_ms = GetTickCount64();
    frame.simulation_time = time;
    QueryPerformanceCounter(&finish);
    frame.original_us = (end.QuadPart - begin.QuadPart) * 1e6 / ipc->qpc_frequency;
    frame.observer_total_us = (finish.QuadPart - start.QuadPart) * 1e6 / ipc->qpc_frequency;
    InterlockedIncrement64(&ipc->sequence);
    ipc->frame = frame;
    MemoryBarrier();
    InterlockedIncrement64(&ipc->sequence);
    InterlockedIncrement64(&ipc->observations);
}

static int check_game_hash(void) {
#ifdef LMU_BINDING_FIXTURE
    return 1;
#else
    static const unsigned char expected[32] = {0xb3, 0x7a, 0x1d, 0xd3, 0xa5, 0x91, 0x23, 0x91,
                                               0xdb, 0x23, 0x60, 0x51, 0x99, 0x5c, 0xaa, 0xdd,
                                               0x6d, 0x55, 0x58, 0x63, 0x1e, 0xd9, 0xce, 0x5d,
                                               0x68, 0xf8, 0x39, 0x7a, 0xbe, 0x2f, 0x6a, 0x8c};
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
    binding_mapping_name(name, GetCurrentProcessId());
    HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!mapping)
        return 0;
    ipc = (BindingIPC *)MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*ipc));
    if (!ipc || ipc->magic != BINDING_MAGIC || ipc->version != 1 ||
        ipc->pid != GetCurrentProcessId())
        return 0;
    ipc->status = 1;
    module_base = (uintptr_t)GetModuleHandleW(NULL);
    if (!check_game_hash()) {
        ipc->error = 100;
        ipc->status = 4;
        return 0;
    }
#ifdef LMU_BINDING_FIXTURE
    targets[0] = (void *)GetProcAddress((HMODULE)module_base, "FixtureDynamics");
    targets[1] = (void *)GetProcAddress((HMODULE)module_base, "FixtureTransform");
#else
    const uintptr_t offsets[2] = {0xec87e0, 0xee1310};
    const unsigned char prologs[2][15] = {
        {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x10, 0x48, 0x89, 0x70, 0x18, 0x48, 0x89, 0x78, 0x20},
        {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x10, 0x48, 0x89, 0x78, 0x18}};
    const size_t lengths[2] = {15, 11};
    const unsigned char owner_code[14] = {0x48, 0x8b, 0x05, 0x22, 0x30, 0x27, 0x01,
                                          0x48, 0x8b, 0xb8, 0xa0, 0x59, 0x01, 0x00};
    if (memcmp((void *)(module_base + 0xba31bf), owner_code, 14)) {
        ipc->error = 104;
        ipc->status = 4;
        return 0;
    }
    for (int i = 0; i < 2; i++) {
        targets[i] = (void *)(module_base + offsets[i]);
        if (memcmp(targets[i], prologs[i], lengths[i])) {
            ipc->error = 101 + i;
            ipc->status = 4;
            return 0;
        }
    }
#endif
    for (int i = 0; i < 2; i++) {
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
    void *hooks[2] = {(void *)&dynamics, (void *)&transform};
    void **trampolines[2] = {(void **)&original_dynamics, (void **)&original_transform};
    for (int i = 0; i < 2; i++) {
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
    for (int i = 0; i < 2; i++)
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
