/* Observes natural Score update calls only. No scoring or physics writes. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <stdint.h>
#include <cstring>
#include <cmath>
#include "scoring_ipc_25661166.h"
#include "MinHook.h"
static ScoringIPC *ipc;
static uintptr_t module_base;
static void *target;
static unsigned char target_before[32];
static volatile LONG capturing;
static void(WINAPI *original_score)(void *, float);
template <class T> static bool field(uintptr_t p, T &out) {
    SIZE_T got = 0;
    return p >= 0x10000 && p < 0x800000000000ULL &&
           ReadProcessMemory(GetCurrentProcess(), (void *)p, &out, sizeof(out), &got) &&
           got == sizeof(out);
}
static bool lease() {
    LONG64 now = GetTickCount64(), hb = InterlockedCompareExchange64(&ipc->heartbeat_ms, 0, 0);
    return !ipc->stop && ipc->allow_observation == 1 && hb <= now && now - hb <= 300;
}
static bool player_score(void *object) {
#ifdef LMU_SCORING_FIXTURE
    typedef int(WINAPI * F)(void *);
    auto f = (F)GetProcAddress((HMODULE)module_base, "FixturePlayerScore");
    return f && f(object);
#else
    uintptr_t owner = 0, vehicle = 0, back = 0;
    return field(module_base + 0x1e194c8, owner) && field(owner, vehicle) &&
           vehicle + 0x1cec8 == (uintptr_t)object && field((uintptr_t)object, back) &&
           back == owner;
#endif
}
static bool capture(void *object, LapGuardFlags &out, int32_t &lap) {
    uintptr_t p = (uintptr_t)object;
    out = {};
    return field(p + 0x160, out.current) && field(p + 0x27290, out.remembered) &&
           field(p + 0x27360, out.last_computed) && field(p + 0x164, out.next_timed) &&
           field(p + 0x272b8, out.off_track) && field(p + 0x10, lap) && lap >= 0 && lap <= 1535 &&
           lap_guard_flags_valid(&out);
}
static void WINAPI score(void *object, float dt) {
    InterlockedIncrement64(&ipc->callbacks);
    bool selected = lease() && std::isfinite(dt) && dt >= 0 && player_score(object);
    if (!selected || InterlockedCompareExchange(&capturing, 1, 0) != 0) {
        original_score(object, dt);
        return;
    }
    LONG64 tid = GetCurrentThreadId(),
           previous = InterlockedCompareExchange64(&ipc->writer_thread, tid, 0);
    if (previous && previous != tid) {
        InterlockedIncrement64(&ipc->skipped);
        original_score(object, dt);
        InterlockedExchange(&capturing, 0);
        return;
    }
    ScoringFrame f = {};
    LARGE_INTEGER begin, end;
    QueryPerformanceCounter(&begin);
    bool valid = capture(object, f.before, f.lap_before);
    f.dt = dt;
    f.tick_ms = GetTickCount64();
    original_score(object, dt); // Original exactly once, with the observed RCX/XMM1 ABI.
    valid = valid && lease() && player_score(object) && capture(object, f.after, f.lap_after);
    if (valid) {
        f.prepared = lap_guard_prepare(&f.after, &f.proposed);
        QueryPerformanceCounter(&end);
        f.observer_us = (end.QuadPart - begin.QuadPart) * 1e6 / ipc->qpc_frequency;
        InterlockedIncrement64(&ipc->sequence);
        ipc->frame = f;
        MemoryBarrier();
        InterlockedIncrement64(&ipc->sequence);
        InterlockedIncrement64(&ipc->observations);
    } else
        InterlockedIncrement64(&ipc->skipped);
    InterlockedExchange(&capturing, 0);
}
static int check_game_hash(void) {
#ifdef LMU_SCORING_FIXTURE
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
    scoring_mapping_name(name, GetCurrentProcessId());
    HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!mapping)
        return 0;
    ipc = (ScoringIPC *)MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*ipc));
    if (!ipc || ipc->magic != SCORING_MAGIC || ipc->version != 1 ||
        ipc->pid != GetCurrentProcessId())
        return 0;
    ipc->status = 1;
    module_base = (uintptr_t)GetModuleHandleW(NULL);
    if (!check_game_hash()) {
        ipc->error = 100;
        ipc->status = 4;
        return 0;
    }
#ifdef LMU_SCORING_FIXTURE
    target = (void *)GetProcAddress((HMODULE)module_base, "FixtureScore");
#else
    target = (void *)(module_base + 0xcf19f0);
    const unsigned char prolog[] = {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x20, 0xf3,
                                    0x0f, 0x11, 0x48, 0x10, 0x55, 0x56, 0x57};
    if (memcmp(target, prolog, sizeof(prolog))) {
        ipc->error = 101;
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
    status = MH_CreateHook(target, (void *)&score, (void **)&original_score);
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
        if (hb > now || now - hb > 2000 || (ULONGLONG)now - started > 120000)
            break;
        Sleep(20);
    }
    ipc->stop = 1;
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
