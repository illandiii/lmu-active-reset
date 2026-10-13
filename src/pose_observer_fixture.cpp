#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include "pose_observer_ipc.h"
static unsigned char vehicle[0x3000], bodies[2][0x160], tires[4][0x1200];
static uintptr_t body_list[2], aggregate_list[1];
static uint64_t dynamics_calls, contact_calls, force_calls;
static int abi_errors;
template <class T> static void put(void *p, const T &v) {
    memcpy(p, &v, sizeof(v));
}
extern "C" __declspec(dllexport) void *WINAPI FixturePlayer() {
    return vehicle;
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureContacts(void *, double dt,
                                                                                  double horizon,
                                                                                  void *world,
                                                                                  double angular) {
    ++contact_calls;
    if (dt != .001 || horizon != .003 || world != (void *)0x12345678 || angular != 2.5)
        ++abi_errors;
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureForces(void *object) {
    ++force_calls;
    if (object != vehicle)
        ++abi_errors;
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureDynamics(void *object,
                                                                                  double time) {
    ++dynamics_calls;
    if (object != vehicle || time != 42.5)
        ++abi_errors;
    void(WINAPI *volatile contact)(void *, double, double, void *, double) = FixtureContacts;
    void(WINAPI *volatile force)(void *) = FixtureForces;
    for (int step = 0; step < 6; step++) {
        if (step % 3 == 0)
            for (int w = 0; w < 4; w++)
                contact(tires[w], .001, .003, (void *)0x12345678, 2.5);
        force(object);
    }
    double x;
    memcpy(&x, bodies[0] + 0x40, 8);
    x += .002;
    put(bodies[0] + 0x40, x);
}
static void init_body(unsigned char *b) {
    double one = 1;
    put(b + 0x58, one);
    double diag[3] = {1, 1, 1};
    put(b + 0xf8, diag);
    double r[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    put(b + 0x90, r);
    double inertia[6] = {1, 0, 1, 0, 0, 1};
    put(b + 0x60, inertia);
}
int main(int argc, char **argv) {
    if (argc < 2)
        return 90;
    for (int i = 0; i < 2; i++) {
        body_list[i] = (uintptr_t)bodies[i];
        init_body(bodies[i]);
    }
    aggregate_list[0] = body_list[0];
    uintptr_t v[3] = {(uintptr_t)body_list, (uintptr_t)body_list + 16, (uintptr_t)body_list + 16};
    put(vehicle + 0x23f8, v);
    uintptr_t a[3] = {(uintptr_t)aggregate_list, (uintptr_t)aggregate_list + 8,
                      (uintptr_t)aggregate_list + 8};
    put(vehicle + 0x2440, a);
    for (int w = 0; w < 4; w++) {
        init_body(tires[w] + 0xac0);
        put(vehicle + 0x618 + w * 0x6b0 + 0x3e8, (uintptr_t)tires[w]);
        put(vehicle + 0x618 + w * 0x6b0 + 0x1d0, body_list[w % 2]);
    }
    wchar_t name[96];
    pose_mapping_name(name, GetCurrentProcessId());
    HANDLE mapping =
        CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(PoseIPC), name);
    auto *s = (PoseIPC *)MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(PoseIPC));
    if (!s)
        return 91;
    memset(s, 0, sizeof(*s));
    s->magic = POSE_MAGIC;
    s->version = 1;
    s->pid = GetCurrentProcessId();
    s->heartbeat_ms = GetTickCount64();
    s->allow_observation = 1;
    void *targets[3] = {(void *)FixtureDynamics, (void *)FixtureContacts, (void *)FixtureForces};
    unsigned char prior[3][32];
    for (int i = 0; i < 3; i++)
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
    if (dynamics_calls != 128 || contact_calls != 1024 || force_calls != 768 || abi_errors ||
        s->observations != 4 || s->frame.phase_errors || s->frame.total_count != 6 ||
        s->frame.prepare_before || s->frame.prepare_after)
        return 94;
    if (s->frame.after[0].state.position_world[0] <= s->frame.before[0].state.position_world[0])
        return 95;
    s->allow_observation = 0;
    for (int i = 0; i < 64; i++)
        run(vehicle, 42.5);
    if (s->observations != 4)
        return 96;
    if (argc > 2)
        Sleep(2300);
    else
        s->stop = 1;
    for (int i = 0; i < 100 && s->status == 2; i++)
        Sleep(20);
    if (s->status != 3)
        return 97;
    for (int i = 0; i < 3; i++)
        if (memcmp(prior[i], targets[i], 32))
            return 98;
    printf("PASS pose observer: 3 original-call ABIs, TLS nested phase order, 6 typed bodies, "
           "gate, %s, all target bytes restored. IPC=%zu.\n",
           argc > 2 ? "watchdog" : "stop", sizeof(PoseIPC));
    return 0;
}
