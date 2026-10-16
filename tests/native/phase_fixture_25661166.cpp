#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "../../src/native/observers/phase_ipc_25661166.h"
static unsigned char physics[128], scoring[0x28000];
static int owner_token;
static volatile LONG dc, sc;
static bool nested;
static unsigned abi_errors;
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
    if (nested) {
        void(WINAPI *volatile f)(void *, float) = FixtureScore;
        f(scoring, .125f);
    }
}
static DWORD WINAPI other_thread(void *) {
    void(WINAPI *volatile f)(void *, float) = FixtureScore;
    f(scoring, .125f);
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
    unsigned char originals[2][32];
    memcpy(originals[0], (void *)FixtureDynamics, 32);
    memcpy(originals[1], (void *)FixtureScore, 32);
    CHECK(LoadLibraryA(argv[1]));
    for (int i = 0; i < 100 && ipc->status < 2; i++) {
        ipc->heartbeat_ms = GetTickCount64();
        Sleep(20);
    }
    CHECK(ipc->status == 2);
    void(WINAPI *volatile d)(void *, double) = FixtureDynamics;
    void(WINAPI *volatile s)(void *, float) = FixtureScore;
    d(physics, .25);
    CHECK(!ipc->frame.stable && dc == 1 && sc == 0);
    s(scoring, .125f);
    d(physics, .25);
    CHECK(ipc->frame.stable && dc == 2 && sc == 1 &&
          ipc->frame.scoring_thread == GetCurrentThreadId());
    nested = true;
    d(physics, .25);
    nested = false;
    d(physics, .25);
    CHECK(!ipc->frame.stable && ipc->frame.overlaps == 1 && dc == 4 && sc == 2);
    HANDLE thread = CreateThread(nullptr, 0, other_thread, nullptr, 0, nullptr);
    CHECK(thread);
    CHECK(WaitForSingleObject(thread, 1000) == WAIT_OBJECT_0);
    CloseHandle(thread);
    d(physics, .25);
    CHECK(ipc->frame.thread_changes == 1 && !ipc->frame.stable);
    LONG64 observed = ipc->observations;
    ipc->allow_observation = 0;
    d(physics, .25);
    s(scoring, .125f);
    CHECK(ipc->observations == observed && !abi_errors);
    ipc->allow_observation = 1;
    if (argc > 2)
        Sleep(2300);
    else
        ipc->stop = 1;
    for (int i = 0; i < 150 && ipc->status == 2; i++)
        Sleep(20);
    CHECK(ipc->status == 3 && !memcmp(originals[0], (void *)FixtureDynamics, 32) &&
          !memcmp(originals[1], (void *)FixtureScore, 32));
    observed = ipc->observations;
    d(physics, .25);
    s(scoring, .125f);
    CHECK(ipc->observations == observed && !abi_errors);
    printf("PASS dual phase hooks: both ABIs once, same-thread sampling, nested overlap, thread "
           "migration, observation lease and %s restores both prologs.\n",
           argc > 2 ? "watchdog" : "stop");
    return 0;
}
