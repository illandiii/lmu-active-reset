#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include "../../src/native/observers/binding_observer_ipc.h"
static unsigned char vehicle[0x3000], hubs[4][0x160], wheels[4][0x160];
static int calls, transform_calls, abi_errors;
template <class T> static void put(void *p, const T &v) {
    memcpy(p, &v, sizeof(v));
}
extern "C" __declspec(dllexport) void *WINAPI FixturePlayer() {
    return vehicle;
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureTransform(void *object,
                                                                                   void *hub) {
    ++transform_calls;
    bool found = false;
    for (int i = 0; i < 4; i++)
        if (object == vehicle + 0x618 + i * 0x6b0 && hub == hubs[i])
            found = true;
    if (!found)
        ++abi_errors;
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureDynamics(void *object,
                                                                                  double time) {
    ++calls;
    if (object != vehicle || time != 42.5)
        ++abi_errors;
    void(WINAPI *volatile transform)(void *, void *) = FixtureTransform;
    for (int step = 0; step < 6; step++)
        for (int i = 0; i < 4; i++)
            transform(vehicle + 0x618 + i * 0x6b0, hubs[i]);
}
static void init(unsigned char *b) {
    double one = 1;
    put(b + 0x58, one);
    double d[3] = {1, 1, 1};
    put(b + 0xf8, d);
    double r[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    put(b + 0x90, r);
}
int main(int argc, char **argv) {
    if (argc < 2)
        return 90;
    for (int i = 0; i < 4; i++) {
        init(hubs[i]);
        init(wheels[i]);
        auto w = vehicle + 0x618 + i * 0x6b0;
        put(w + 0x1d0, (uintptr_t)wheels[i]);
        put(w + 0x1d8, (uintptr_t)hubs[i]);
        double rate = 2;
        put(w + 0x128, rate);
        double l[3] = {2, 0, 0};
        put(wheels[i] + 0x110, l);
    }
    wchar_t name[96];
    binding_mapping_name(name, GetCurrentProcessId());
    HANDLE mapping =
        CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(BindingIPC), name);
    auto *s = (BindingIPC *)MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(BindingIPC));
    if (!s)
        return 91;
    memset(s, 0, sizeof(*s));
    s->magic = BINDING_MAGIC;
    s->version = 1;
    s->pid = GetCurrentProcessId();
    s->heartbeat_ms = GetTickCount64();
    s->allow_observation = 1;
    void *targets[2] = {(void *)FixtureDynamics, (void *)FixtureTransform};
    unsigned char prior[2][32];
    for (int i = 0; i < 2; i++)
        memcpy(prior[i], targets[i], 32);
    if (!LoadLibraryA(argv[1]))
        return 92;
    for (int i = 0; i < 100 && s->status < 2; i++) {
        s->heartbeat_ms = GetTickCount64();
        Sleep(20);
    }
    if (s->status != 2) {
        printf("status %ld error %ld\n", s->status, s->error);
        return 93;
    }
    void(WINAPI *volatile run)(void *, double) = FixtureDynamics;
    for (int i = 0; i < 128; i++) {
        s->heartbeat_ms = GetTickCount64();
        run(vehicle, 42.5);
    }
    if (calls != 128 || transform_calls != 3072 || abi_errors || s->observations != 4 ||
        s->frame.before.validation || s->frame.after.validation ||
        s->frame.last_transform.validation)
        return 94;
    for (int i = 0; i < 4; i++)
        if (s->frame.wheel_calls[i] != 6)
            return 95;
    // A inconsistent final wheel must be observed as a rejection, never hidden.
    double wrong[3] = {4, 0, 0};
    put(wheels[3] + 0x110, wrong);
    for (int i = 0; i < 32; i++) {
        s->heartbeat_ms = GetTickCount64();
        run(vehicle, 42.5);
    }
    if (!s->frame.after.validation || !s->frame.last_transform.validation)
        return 96;
    auto n = s->observations;
    s->allow_observation = 0;
    for (int i = 0; i < 32; i++)
        run(vehicle, 42.5);
    if (s->observations != n)
        return 97;
    if (argc > 2)
        Sleep(2300);
    else
        s->stop = 1;
    for (int i = 0; i < 150 && s->status == 2; i++)
        Sleep(20);
    if (s->status != 3)
        return 98;
    for (int i = 0; i < 2; i++)
        if (memcmp(prior[i], targets[i], 32))
            return 99;
    printf("PASS binding observer: ABIs, original once, 6 updates/wheel, typed capture, "
           "inconsistent state rejected, gate, %s, original bytes restored. IPC=%zu\n",
           argc > 2 ? "watchdog" : "stop", sizeof(BindingIPC));
    return 0;
}
