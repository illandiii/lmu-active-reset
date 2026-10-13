#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "contact_ipc_v2_25661166.h"
static unsigned char tire[4][0x1200], pool[4][4][0x110], surface[4][2][0x58];
static uintptr_t active[4][8];
static uint32_t bits[4];
static uint64_t calls;
static int stationary = 1, abi_errors = 0;
template <class T> static void put(void *p, const T &value) {
    memcpy(p, &value, sizeof(value));
}
template <class T> static T get(void *p) {
    T value;
    memcpy(&value, p, sizeof(value));
    return value;
}
extern "C" __declspec(dllexport) int WINAPI FixtureWheel(void *p, uintptr_t *player) {
    *player = (uintptr_t)tire;
    for (int i = 0; i < 4; i++)
        if (p == tire[i])
            return i;
    return -1;
}
extern "C" __declspec(dllexport) int WINAPI FixtureStationary() {
    return stationary;
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureRefresh(void *p, double dt,
                                                                                 double horizon,
                                                                                 void *world,
                                                                                 double angular) {
    ++calls;
    if (dt != .001 || horizon != .003 || world != (void *)0x12345678 || angular != 2.5) {
        ++abi_errors;
        return;
    }
    uintptr_t player;
    int w = FixtureWheel(p, &player);
    if (w < 0)
        return;
    size_t count = (get<uintptr_t>(tire[w] + 0x9f8) - (uintptr_t)active[w]) / 8;
    for (size_t i = 0; i < count;) {
        auto *n = (unsigned char *)active[w][i];
        double d = get<double>(n + 0xd0);
        if (d <= 0) {
            n[0x60] = n[0x67] = 0;
            active[w][i] = active[w][--count];
        } else
            ++i;
    }
    for (int i = 0; i < 3; i++) {
        auto *n = pool[w][i];
        if (!n[0x60]) {
            n[0x60] = 1;
            active[w][count++] = (uintptr_t)n;
        }
        double geometry[5] = {.02 + i * .001, 0, 1, 0, 3};
        put(n + 0xd0, geometry);
    }
    put(tire[w] + 0x9f8, (uintptr_t)active[w] + count * 8);
}
static void init() {
    for (int w = 0; w < 4; w++) {
        put(tire[w] + 0x318, (uintptr_t)pool[w]);
        uintptr_t v[3] = {(uintptr_t)active[w], (uintptr_t)active[w] + 24,
                          (uintptr_t)active[w] + 64};
        put(tire[w] + 0x9f0, v);
        uintptr_t sv[3] = {(uintptr_t)surface[w], (uintptr_t)surface[w] + 0xb0,
                           (uintptr_t)surface[w] + 0xb0};
        put(tire[w] + 0xa60, sv);
        uintptr_t bv[3] = {(uintptr_t)&bits[w], (uintptr_t)&bits[w] + 4, (uintptr_t)&bits[w] + 4};
        put(tire[w] + 0xa78, bv);
        bits[w] = 3;
        for (int i = 0; i < 2; i++)
            put(surface[w][i], (uintptr_t)(0x20000 + i * 0x1000));
        for (int i = 0; i < 3; i++) {
            active[w][i] = (uintptr_t)pool[w][i];
            pool[w][i][0x60] = pool[w][i][0x67] = 1;
            put(pool[w][i] + 0x62, (uint16_t)(i % 2));
            put(pool[w][i] + 0xf8, (uintptr_t)(0x20000 + (i % 2) * 0x1000));
            double geo[5] = {.02, 0, 1, 0, 3};
            put(pool[w][i] + 0xd0, geo);
        }
    }
}
int main(int argc, char **argv) {
    if (argc < 2)
        return 90;
    init();
    wchar_t name[96];
    contact_mapping_name(name, GetCurrentProcessId());
    HANDLE mapping =
        CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(ContactIPC), name);
    if (!mapping || GetLastError() == ERROR_ALREADY_EXISTS)
        return 91;
    auto *s = (ContactIPC *)MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ContactIPC));
    if (!s)
        return 91;
    memset(s, 0, sizeof(*s));
    s->magic = CONTACT_MAGIC;
    s->version = 1;
    s->pid = GetCurrentProcessId();
    s->heartbeat_ms = GetTickCount64();
    s->allow_observation = 1;
    unsigned char original[32];
    memcpy(original, (void *)FixtureRefresh, 32);
    if (!LoadLibraryA(argv[1])) {
        printf("load error %lu\n", GetLastError());
        return 92;
    }
    for (int i = 0; i < 100 && s->status < 2; i++) {
        s->heartbeat_ms = GetTickCount64();
        Sleep(20);
    }
    if (s->status != 2) {
        printf("status %ld error %ld\n", s->status, s->error);
        return 93;
    }
    void(WINAPI *volatile f)(void *, double, double, void *, double) = FixtureRefresh;
    auto step = [&](int wheel) {
        s->heartbeat_ms = GetTickCount64();
        f(tire[wheel], .001, .003, (void *)0x12345678, 2.5);
    };
    for (int i = 0; i < 2048; i++)
        for (int w = 0; w < 4; w++)
            step(w);
    if (s->observations != 256 || s->physics_writes || calls != 8192 || abi_errors)
        return 94;
    for (int w = 0; w < 4; w++)
        if (s->latest[w].pre_bad || s->latest[w].post_bad || s->latest[w].post_count != 3 ||
            s->latest[w].native_stationary != 1)
            return 95;
    // Native stationary gate blocks the request, without consuming it.
    s->request_id = 1;
    s->request_deadline_ms = GetTickCount64() + 10000;
    s->allow_rebuild = 1;
    stationary = 0;
    for (int w = 0; w < 4; w++)
        step(w);
    if (s->physics_writes)
        return 96;
    stationary = 1;
    // Invalid geometry late in a batch must not produce earlier depth writes.
    double bad = NAN;
    put(pool[0][2] + 0xd8, bad);
    step(0);
    if (s->physics_writes)
        return 97;
    double good = 0;
    put(pool[0][2] + 0xd8, good);
    // Observation lease and expired request independently prohibit writes.
    s->allow_observation = 0;
    for (int w = 0; w < 4; w++)
        step(w);
    if (s->physics_writes)
        return 98;
    s->allow_observation = 1;
    s->request_deadline_ms = GetTickCount64() - 1;
    for (int w = 0; w < 4; w++)
        step(w);
    if (s->physics_writes)
        return 99;
    s->request_deadline_ms = GetTickCount64() + 10000;
    for (int i = 0; i < 70; i++)
        for (int w = 0; w < 4; w++)
            step(w);
    if (s->completed_mask != 15 || s->failed_mask || s->physics_writes != 12 || abi_errors) {
        printf("mask %ld failed %ld writes %llu ABI %d\n", s->completed_mask, s->failed_mask,
               s->physics_writes, abi_errors);
        return 100;
    }
    for (int w = 0; w < 4; w++)
        if (s->rebuilt[w].retired != 3 || s->rebuilt[w].cold_flags_clear != 3 ||
            s->rebuilt[w].position_delta)
            return 101;
    for (int i = 0; i < 70; i++)
        for (int w = 0; w < 4; w++)
            step(w);
    if (s->physics_writes != 12)
        return 102; // Request cannot repeat.
    if (argc > 2 && strcmp(argv[2], "watchdog") == 0)
        Sleep(2300);
    else
        s->stop = 1;
    for (int i = 0; i < 100 && s->status == 2; i++)
        Sleep(20);
    if (s->status != 3 || memcmp(original, (void *)FixtureRefresh, 32))
        return 103;
    uint64_t callback_count = s->callbacks;
    step(0);
    if (s->callbacks != callback_count)
        return 104;
    printf("PASS: five-argument x64 ABI; original once; four-wheel observation; late-node "
           "rejection; lease, stationary and deadline gates; single-use native cold reconstruction "
           "model; %s restores bytes. IPC=%zu result=%zu.\n",
           argc > 2 ? "watchdog" : "stop", sizeof(ContactIPC), sizeof(ContactResult));
    return 0;
}
