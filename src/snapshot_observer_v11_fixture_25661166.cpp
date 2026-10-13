#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include "snapshot_observer_v11_25661166.h"
static unsigned char vehicle[0x5000], motor[0x300], shaft[32], gearbox[128], bodies[9][0x160],
    tires[4][0x1200];
static uintptr_t body_list[9], aggregate_list[1], constraints[31];
static unsigned char joints[31][0x400];
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
    put(b + 8, one);
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
    put(vehicle + 0x4918, (uintptr_t)shaft);
    put(vehicle + 0x4920, (uintptr_t)gearbox);
    put(vehicle + 0x46f0, (uintptr_t)motor);
    put(motor, (uintptr_t)GetModuleHandleW(NULL) + 0x1793578);
    for (int i = 0; i < 9; i++) {
        body_list[i] = (uintptr_t)bodies[i];
        init_body(bodies[i]);
        double x = i;
        put(bodies[i] + 0x40, x);
    }
    aggregate_list[0] = body_list[0];
    uintptr_t v[3] = {(uintptr_t)body_list, (uintptr_t)body_list + 72, (uintptr_t)body_list + 72};
    put(vehicle + 0x23f8, v);
    uintptr_t a[3] = {(uintptr_t)aggregate_list, (uintptr_t)aggregate_list + 8,
                      (uintptr_t)aggregate_list + 8};
    put(vehicle + 0x2440, a);
    for (int w = 0; w < 4; w++) {
        init_body(tires[w] + 0xac0);
        double total = 1;
        put(tires[w] + 0xc18, total);
        float ti[3] = {1, 1, 1};
        put(tires[w] + 0xce4, ti);
        put(vehicle + 0x618 + w * 0x6b0 + 0x3e8, (uintptr_t)tires[w]);
        put(vehicle + 0x618 + w * 0x6b0 + 0x1d0, body_list[1 + w]);
    }
    for (int w = 0; w < 4; w++) {
        double base = 1;
        put(vehicle + 0x618 + w * 0x6b0 + 0x130, base);
    }
    double one = 1, half = .5;
    put(vehicle + 0x448, one);
    put(vehicle + 0x4a8, one);
    put(vehicle + 0x4c0, body_list[0]);
    put(vehicle + 0x550, (uintptr_t)1);
    put(vehicle + 0x46f8, (uintptr_t)motor);
    put(motor + 0x160, (uintptr_t)(vehicle + 0x4f8));
    put(vehicle + 0x4f8, half);
    put(vehicle + 0x508, one);
    put(vehicle + 0x518, one);
    for (int i = 0; i < 31; i++) {
        auto *c = joints[i];
        constraints[i] = (uintptr_t)c;
        uintptr_t vt = (uintptr_t)GetModuleHandleW(NULL) + (i < 20   ? 0x1807698
                                                            : i < 26 ? 0x1807ba8
                                                            : i < 30 ? 0x1807d18
                                                                     : 0x1808098);
        put(c, vt);
        uintptr_t pair[2] = {body_list[i % 9], body_list[(i + 1) % 9]};
        put(c + (i < 20 ? 0xa0 : i < 30 ? 0x98 : 0x30), pair);
        if (i >= 26)
            for (int j = 0; j < (i < 30 ? 2 : 3); j++) {
                double axis[3] = {0, 0, 0};
                axis[j] = 1;
                put(c + (i < 30 ? 0xa8 : 0x60) + j * (i < 30 ? 0xa8 : 0xe8), axis);
            }
    }
    uintptr_t cv[3] = {(uintptr_t)constraints, (uintptr_t)constraints + sizeof(constraints),
                       (uintptr_t)constraints + sizeof(constraints)};
    put(vehicle + 0x2410, cv);
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
        s->observations != 4 || s->frame.total_count != 13)
        return 94;
    if (s->frame.plan_status || !s->frame.plan_fields)
        return 103;
    if (s->frame.bank_status || s->frame.bank_saves != 1 || s->frame.bank_preparations != 3)
        return 102;
    if (s->frame.mechanical_status || s->frame.geometry_status || !s->frame.memory_valid ||
        s->frame.constraint_count != 31)
        return 101;
    uintptr_t oldvt;
    memcpy(&oldvt, joints[30], sizeof(oldvt));
    put(joints[30], (uintptr_t)0);
    for (int i = 0; i < 32; i++)
        run(vehicle, 42.5);
    if (s->observations != 4 || s->skipped != 1 || dynamics_calls != 160)
        return 102;
    put(joints[30], oldvt);
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
    printf("PASS pose observer: Dynamics original exactly once, 13 typed bodies and 31 constraint "
           "memories, unknown-type rejection, gate, %s, all target bytes restored. IPC=%zu.\n",
           argc > 2 ? "watchdog" : "stop", sizeof(PoseIPC));
    return 0;
}
