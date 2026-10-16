#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include "../../src/native/observers/scoring_ipc_25661166.h"
static unsigned char object[0x28000], foreign_object[0x28000];
static uint64_t calls;
static unsigned abi_errors;
template <class T> static void put(unsigned offset, T value) {
    memcpy(object + offset, &value, sizeof(value));
}
extern "C" __declspec(dllexport) int WINAPI FixturePlayerScore(void *p) {
    return p == object;
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureScore(void *p, float dt) {
    ++calls;
    if (dt != .125f)
        ++abi_errors;
    if (p == object)
        put<int32_t>(0x27360, 2);
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
    put<int32_t>(0x160, 2);
    put<int32_t>(0x27290, 2);
    put<int32_t>(0x27360, 0);
    put<int32_t>(0x10, 8);
    put<uint8_t>(0x164, 1);
    wchar_t name[96];
    scoring_mapping_name(name, GetCurrentProcessId());
    HANDLE map =
        CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(ScoringIPC), name);
    CHECK(map && GetLastError() != ERROR_ALREADY_EXISTS);
    auto *ipc = (ScoringIPC *)MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ScoringIPC));
    CHECK(ipc);
    memset(ipc, 0, sizeof(*ipc));
    ipc->magic = SCORING_MAGIC;
    ipc->version = 1;
    ipc->pid = GetCurrentProcessId();
    ipc->heartbeat_ms = GetTickCount64();
    ipc->allow_observation = 1;
    unsigned char original[32];
    memcpy(original, (void *)FixtureScore, 32);
    CHECK(LoadLibraryA(argv[1]));
    for (int i = 0; i < 100 && ipc->status < 2; i++) {
        ipc->heartbeat_ms = GetTickCount64();
        Sleep(20);
    }
    CHECK(ipc->status == 2);
    void(WINAPI *volatile f)(void *, float) = FixtureScore;
    f(object, .125f);
    CHECK(calls == 1 && ipc->observations == 1 && ipc->frame.before.last_computed == 0 &&
          ipc->frame.after.last_computed == 2);
    CHECK(ipc->frame.prepared == 1 && ipc->frame.proposed.current == 0 &&
          ipc->frame.proposed.remembered == 0 && ipc->frame.proposed.last_computed == 0);
    CHECK(ipc->frame.after.current == 2 && ipc->frame.proposed.next_timed == 1 &&
          ipc->frame.lap_after == 8);
    f(foreign_object, .125f);
    CHECK(calls == 2 && ipc->observations == 1);
    ipc->allow_observation = 0;
    f(object, .125f);
    CHECK(calls == 3 && ipc->observations == 1);
    ipc->allow_observation = 1;
    put<int32_t>(0x160, 3);
    f(object, .125f);
    CHECK(calls == 4 && ipc->observations == 1 && ipc->skipped == 1);
    put<int32_t>(0x160, 2);
    ipc->heartbeat_ms = GetTickCount64() - 500;
    f(object, .125f);
    CHECK(calls == 5 && ipc->observations == 1);
    ipc->heartbeat_ms = GetTickCount64();
    f(object, .125f);
    CHECK(calls == 6 && ipc->observations == 2 && !abi_errors);
    if (argc > 2)
        Sleep(2300);
    else
        ipc->stop = 1;
    for (int i = 0; i < 150 && ipc->status == 2; i++)
        Sleep(20);
    CHECK(ipc->status == 3 && ipc->error == 0 && memcmp(original, (void *)FixtureScore, 32) == 0);
    auto cb = ipc->callbacks;
    f(object, .125f);
    CHECK(calls == 7 && ipc->callbacks == cb && !abi_errors);
    printf("PASS scoring RCX/XMM1 forwarding exactly once; player-only pre/post capture; proposal "
           "only; invalid flag and lease rejection; %s restores original code.\n",
           argc > 2 ? "watchdog" : "stop");
    return 0;
}
