#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include "bar_observer_ipc_14200.h"
static unsigned char vehicle[0x5000], objects[2][0x160], constraint[0x1a0];
static uintptr_t roots[2], constraints[1];
static uint64_t calls;
static int errors;
template <class T> static void put(void *p, const T &v) {
    memcpy(p, &v, sizeof(v));
}
extern "C" __declspec(dllexport) void *WINAPI FixturePlayer() {
    return vehicle;
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureBar(void *p) {
    ++calls;
    if (p != constraint) {
        ++errors;
        return;
    }
    RigidSavedBody b[2] = {};
    BarGeometryInput in{};
    BarGeometryPrepared out;
    for (int i = 0; i < 2; i++) {
        b[i].mass = {1, {1, 1, 1}};
        b[i].orientation[0] = b[i].orientation[4] = b[i].orientation[8] = 1;
    }
    b[0].position_world[0] = 2;
    if (bar_prepare_geometry(b, &in, &out)) {
        ++errors;
        return;
    }
    put(constraint + 0xb0, out.jacobian);
    put(constraint + 0x140, out.weighted);
    put(constraint + 0x138, out.inverse_effective_mass);
}
int main(int argc, char **argv) {
    if (argc < 2)
        return 90;
    for (int i = 0; i < 2; i++) {
        roots[i] = (uintptr_t)objects[i];
        double one = 1, diag[3] = {1, 1, 1}, r[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
        put(objects[i] + 0x58, one);
        put(objects[i] + 0xf8, diag);
        put(objects[i] + 0x90, r);
    }
    double x = 2;
    put(objects[0] + 0x40, x);
    put(constraint + 0xa0, roots);
    constraints[0] = (uintptr_t)constraint;
    uintptr_t rv[3] = {(uintptr_t)roots, (uintptr_t)roots + 16, (uintptr_t)roots + 16};
    put(vehicle + 0x23f8, rv);
    uintptr_t cv[3] = {(uintptr_t)constraints, (uintptr_t)constraints + 8,
                       (uintptr_t)constraints + 8};
    put(vehicle + 0x2410, cv);
    wchar_t name[96];
    pose_mapping_name(name, GetCurrentProcessId());
    HANDLE m =
        CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(PoseIPC), name);
    auto *s = (PoseIPC *)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(PoseIPC));
    if (!s)
        return 91;
    memset(s, 0, sizeof(*s));
    s->magic = POSE_MAGIC;
    s->version = 1;
    s->pid = GetCurrentProcessId();
    s->heartbeat_ms = GetTickCount64();
    s->allow_observation = 1;
    unsigned char prior[32];
    memcpy(prior, (void *)FixtureBar, 32);
    if (!LoadLibraryA(argv[1]))
        return 92;
    for (int i = 0; i < 100 && s->status < 2; i++) {
        s->heartbeat_ms = GetTickCount64();
        Sleep(20);
    }
    if (s->status != 2)
        return 93;
    void(WINAPI *volatile run)(void *) = FixtureBar;
    for (int i = 0; i < 508; i++) {
        s->heartbeat_ms = GetTickCount64();
        run(constraint);
    }
    if (calls != 508 || errors || s->observations != 4 || s->frame.prepare_status ||
        s->frame.jacobian_error || s->frame.weighted_error || s->frame.coefficient_error)
        return 94;
    s->allow_observation = 0;
    for (int i = 0; i < 127; i++)
        run(constraint);
    if (s->observations != 4)
        return 95;
    if (argc > 2)
        Sleep(2300);
    else
        s->stop = 1;
    for (int i = 0; i < 100 && s->status == 2; i++)
        Sleep(20);
    if (s->status != 3 || memcmp(prior, (void *)FixtureBar, 32))
        return 96;
    printf("PASS Bar observer: original exactly once, coherent post-call geometry, ownership, "
           "lease gate, %s, original bytes restored. IPC=%zu\n",
           argc > 2 ? "watchdog" : "stop", sizeof(PoseIPC));
    return 0;
}
