/* Observational hook only: never writes vehicle state. No reset entry point. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "observer_ipc.h"
#include "MinHook.h"
static ObserverIPC *ipc;
static uintptr_t module_base;
static void(WINAPI *original_integrate)(void *, double);
static void *target;
static unsigned char target_before[32];
static int lease_valid(void) {
    LONG64 now = (LONG64)GetTickCount64();
    LONG64 heartbeat = InterlockedCompareExchange64(&ipc->heartbeat_ms, 0, 0);
    return !ipc->stop && ipc->allow_observation == 1 && heartbeat <= now && now - heartbeat <= 500;
}
static int player_matches(void *object) {
#ifdef LMU_OBSERVER_FIXTURE
    typedef void *(WINAPI * GetVehicle)(void);
    GetVehicle f = (GetVehicle)GetProcAddress((HMODULE)module_base, "FixtureVehicle");
    return f && f() == object;
#else
    /* Ordinary self-process reads make stale owner pointers a skipped sample. */
    SIZE_T n = 0;
    uintptr_t owner = 0, player = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), (void *)(module_base + 0x1dd94d8), &owner, 8, &n) ||
        n != 8 || owner < 0x10000)
        return 0;
    if (!ReadProcessMemory(GetCurrentProcess(), (void *)(owner + 0x15978), &player, 8, &n) ||
        n != 8)
        return 0;
    return player == (uintptr_t)object;
#endif
}
static void WINAPI observe_integrate(void *object, double dt) {
    LONG64 serial = InterlockedIncrement64(&ipc->callbacks);
    if ((serial & 7) != 0 || !lease_valid() || !isfinite(dt) || dt <= 0 || dt > 0.1 ||
        !player_matches(object)) {
        original_integrate(object, dt);
        return;
    }
    DWORD tid = GetCurrentThreadId();
    LONG64 previous = InterlockedCompareExchange64(&ipc->writer_thread, tid, 0);
    if (previous && previous != tid) {
        InterlockedIncrement64(&ipc->skipped);
        original_integrate(object, dt);
        return;
    }
    unsigned char *vehicle = (unsigned char *)object;
    double pre_p[3], pre_v[3];
    memcpy(pre_p, vehicle + 0x24b0, sizeof(pre_p));
    memcpy(pre_v, vehicle + 0x2498, sizeof(pre_v));
    LARGE_INTEGER begin, end;
    QueryPerformanceCounter(&begin);
    original_integrate(object, dt);
    QueryPerformanceCounter(&end);
    if (!lease_valid()) {
        InterlockedIncrement64(&ipc->skipped);
        return;
    }
    uintptr_t body_begin = 0, body_end = 0;
    memcpy(&body_begin, vehicle + 0x2440, 8);
    memcpy(&body_end, vehicle + 0x2448, 8);
    if (body_end < body_begin || (body_end - body_begin) % 8 || (body_end - body_begin) / 8 > 64) {
        InterlockedIncrement64(&ipc->skipped);
        return;
    }
    InterlockedIncrement64(&ipc->sequence);
    ipc->tick_ms = GetTickCount64();
    ipc->begin_qpc = begin.QuadPart;
    ipc->duration_qpc = end.QuadPart - begin.QuadPart;
    ipc->dt = dt;
    memcpy(ipc->pre_position, pre_p, sizeof(pre_p));
    memcpy(ipc->pre_velocity, pre_v, sizeof(pre_v));
    memcpy(ipc->post_position, vehicle + 0x24b0, sizeof(pre_p));
    memcpy(ipc->post_velocity, vehicle + 0x2498, sizeof(pre_v));
    ipc->body_count = (body_end - body_begin) / 8;
    MemoryBarrier();
    InterlockedIncrement64(&ipc->sequence);
    InterlockedIncrement64(&ipc->observations);
}
static int check_game_hash(void) {
#ifdef LMU_OBSERVER_FIXTURE
    return 1;
#else
    static const unsigned char expected[32] = {0xbb, 0x86, 0x61, 0xcf, 0x6f, 0x96, 0xa0, 0x56,
                                               0x37, 0x4a, 0xfa, 0xc2, 0x90, 0x18, 0xc1, 0xfd,
                                               0xd7, 0x1f, 0x86, 0x16, 0x4c, 0x15, 0x8a, 0x71,
                                               0x04, 0x3b, 0xb7, 0xc2, 0x44, 0xf7, 0x40, 0x7e};
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
static DWORD WINAPI worker(void *unused) {
    (void)unused;
    wchar_t name[96];
    mapping_name(name, GetCurrentProcessId());
    HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!mapping)
        return 0;
    ipc = (ObserverIPC *)MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*ipc));
    if (!ipc || ipc->magic != OBS_MAGIC || ipc->version != 1 || ipc->pid != GetCurrentProcessId())
        return 0;
    InterlockedExchange(&ipc->status, 1);
    module_base = (uintptr_t)GetModuleHandleW(NULL);
    if (!check_game_hash()) {
        ipc->error = 100;
        ipc->status = 4;
        return 0;
    }
#ifdef LMU_OBSERVER_FIXTURE
    target = (void *)GetProcAddress((HMODULE)module_base, "FixtureIntegrate");
#else
    target = (void *)(module_base + 0xeaf350);
    /* Exact first instructions, independently checked by the external controller. */
    const unsigned char prolog[] = {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x70,
                                    0x10, 0x57, 0x48, 0x81, 0xec, 0xc0, 0,    0,    0};
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
    memcpy(target_before, target, sizeof(target_before));
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    ipc->qpc_frequency = frequency.QuadPart;
    MH_STATUS result = MH_Initialize();
    if (result != MH_OK) {
        ipc->error = 200 + result;
        ipc->status = 4;
        return 0;
    }
    result = MH_CreateHook(target, (void *)&observe_integrate, (void **)&original_integrate);
    if (result != MH_OK) {
        ipc->error = 300 + result;
        ipc->status = 4;
        return 0;
    }
    LONG64 now = (LONG64)GetTickCount64(),
           hb = InterlockedCompareExchange64(&ipc->heartbeat_ms, 0, 0);
    if (ipc->stop || hb > now || now - hb > 2000) {
        ipc->error = 103;
        ipc->status = 4;
        return 0;
    }
    result = MH_EnableHook(target);
    if (result != MH_OK) {
        ipc->error = 400 + result;
        ipc->status = 4;
        return 0;
    }
    InterlockedExchange(&ipc->status, 2);
    ULONGLONG started = GetTickCount64();
    while (!ipc->stop) {
        now = (LONG64)GetTickCount64();
        hb = InterlockedCompareExchange64(&ipc->heartbeat_ms, 0, 0);
        if (hb > now || now - hb > 2000 || (ULONGLONG)now - started > 240000)
            break;
        Sleep(20);
    }
    InterlockedExchange(&ipc->allow_observation, 0);
    result = MH_DisableHook(target);
    if (result != MH_OK) {
        ipc->error = 500 + result;
        ipc->status = 5;
        return 0;
    }
    if (memcmp(target, target_before, sizeof(target_before))) {
        ipc->error = 104;
        ipc->status = 5;
        return 0;
    }
    InterlockedExchange(&ipc->status, 3);
    /* Retain module, mapping and trampoline until process exit: no unsafe unload. */
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        if (thread)
            CloseHandle(thread);
    }
    return TRUE;
}
