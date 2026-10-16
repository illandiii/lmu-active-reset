#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "../../src/native/observers/boundary_ipc_v2_25661166.h"
static CRITICAL_SECTION game_lock;
extern "C" __declspec(dllexport) uintptr_t WINAPI FixtureLock() {
    return (uintptr_t)&game_lock;
}
static unsigned char physics[128], scoring[0x28000];
static int owner_token;
static volatile LONG dc, sc, bc;
static unsigned abi_errors;
static bool nested;
extern "C" __declspec(dllexport) int WINAPI FixtureRoots(uintptr_t *o, uintptr_t *p, uintptr_t *s) {
    *o = (uintptr_t)&owner_token;
    *p = (uintptr_t)physics;
    *s = (uintptr_t)scoring;
    return 1;
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureScore(void *p, float dt) {
    if (p != scoring || dt != .125f)
        ++abi_errors;
    InterlockedIncrement(&sc);
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureDynamics(void *p,
                                                                                  double dt) {
    if (p != physics || dt != .25)
        ++abi_errors;
    InterlockedIncrement(&dc);
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureBoundary(uint64_t input) {
    if (input != 0x1234567887654321ULL)
        ++abi_errors;
    InterlockedIncrement(&bc);
    if (nested) {
        nested = false;
        void(WINAPI *volatile b)(uint64_t) = FixtureBoundary;
        b(input);
        nested = true;
    }
    void(WINAPI *volatile s)(void *, float) = FixtureScore;
    s(scoring, .125f);
    void(WINAPI *volatile d)(void *, double) = FixtureDynamics;
    d(physics, .25);
}
static void WINAPI RunBoundary(uint64_t input) {
    EnterCriticalSection(&game_lock);
    void(WINAPI *volatile f)(uint64_t) = FixtureBoundary;
    f(input);
    LeaveCriticalSection(&game_lock);
}
static DWORD WINAPI other_thread(void *) {
    void(WINAPI *volatile b)(uint64_t) = RunBoundary;
    b(0x1234567887654321ULL);
    return 0;
}
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            printf("FAIL line %d %s\n", __LINE__, #x);                                             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv) {
    CHECK(argc >= 2);
    InitializeCriticalSection(&game_lock);
    for (unsigned off : {0x160u, 0x27290u, 0x27360u}) {
        int32_t x = 2;
        memcpy(scoring + off, &x, 4);
    }
    wchar_t name[96];
    phase_mapping_name(name, GetCurrentProcessId());
    HANDLE map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                    sizeof(PhaseIPC), name);
    CHECK(map && GetLastError() != ERROR_ALREADY_EXISTS);
    auto *ipc = (PhaseIPC *)MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(PhaseIPC));
    CHECK(ipc);
    memset(ipc, 0, sizeof(*ipc));
    ipc->magic = PHASE_MAGIC;
    ipc->version = 1;
    ipc->pid = GetCurrentProcessId();
    ipc->heartbeat_ms = GetTickCount64();
    ipc->allow_observation = 1;
    void *targets[] = {(void *)FixtureDynamics, (void *)FixtureScore, (void *)FixtureBoundary};
    unsigned char original[3][32];
    for (int i = 0; i < 3; i++)
        memcpy(original[i], targets[i], 32);
    CHECK(LoadLibraryA(argv[1]));
    for (int i = 0; i < 100 && ipc->status < 2; i++) {
        ipc->heartbeat_ms = GetTickCount64();
        Sleep(20);
    }
    CHECK(ipc->status == 2);
    void(WINAPI *volatile b)(uint64_t) = RunBoundary;
    void(WINAPI *volatile d)(void *, double) = FixtureDynamics;
    b(0x1234567887654321ULL);
    CHECK(ipc->frame.stable && ipc->frame.boundary_depth == 1 &&
          ipc->frame.boundary_thread == GetCurrentThreadId() && ipc->frame.outside_boundary == 0 &&
          ipc->frame.boundary_lock_held && ipc->frame.lock_owner == GetCurrentThreadId() &&
          ipc->frame.lock_recursion > 0 && dc == 1 && sc == 1 && bc == 1);
    d(physics, .25);
    CHECK(!ipc->frame.stable && ipc->frame.boundary_depth == 0 &&
          ipc->frame.outside_boundary == 1 && dc == 2);
    nested = true;
    b(0x1234567887654321ULL);
    nested = false;
    CHECK(dc == 4 && bc == 3 && sc == 3 && ipc->frame.boundary_depth == 1);
    void(WINAPI *volatile unlocked)(uint64_t) = FixtureBoundary;
    unlocked(0x1234567887654321ULL);
    CHECK(!ipc->frame.stable && !ipc->frame.boundary_lock_held && ipc->frame.boundary_depth == 1);
    HANDLE t = CreateThread(nullptr, 0, other_thread, nullptr, 0, nullptr);
    CHECK(t && WaitForSingleObject(t, 1000) == WAIT_OBJECT_0);
    CloseHandle(t);
    CHECK(!ipc->frame.stable && ipc->frame.thread_changes >= 1);
    LONG64 observed = ipc->observations;
    ipc->allow_observation = 0;
    b(0x1234567887654321ULL);
    CHECK(ipc->observations == observed && !abi_errors);
    ipc->allow_observation = 1;
    if (argc > 2)
        Sleep(2300);
    else
        ipc->stop = 1;
    for (int i = 0; i < 150 && ipc->status == 2; i++)
        Sleep(20);
    CHECK(ipc->status == 3);
    for (int i = 0; i < 3; i++)
        CHECK(!memcmp(original[i], targets[i], 32));
    observed = ipc->observations;
    b(0x1234567887654321ULL);
    CHECK(ipc->observations == observed && !abi_errors);
    printf("PASS outer boundary observer: three ABIs, enclosed/outside callbacks, nesting, thread "
           "migration, lease and %s. No game writes.\n",
           argc > 2 ? "watchdog" : "stop");
    return 0;
}
