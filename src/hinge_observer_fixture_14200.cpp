#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include "hinge_observer_ipc_14200.h"
static unsigned char vehicle[0x5000], objects[2][0x160], constraint[0x400];
static uintptr_t roots[2], constraints[1];
static uint64_t calls;
static int errors;
template <class T> static void put(void *p, const T &v) {
    memcpy(p, &v, sizeof(v));
}
extern "C" __declspec(dllexport) void *WINAPI FixturePlayer() {
    return vehicle;
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureHinge(void *p) {
    ++calls;
    if (p != constraint) {
        ++errors;
        return;
    }
    RigidSavedBody b[2] = {};
    AngularGeometryInput in{};
    BarGeometryPrepared out[3];
    for (int a = 0; a < 3; a++)
        in.axes_local[a][a] = 1;
    for (int i = 0; i < 2; i++) {
        b[i].mass = {1, {1, 1, 1}};
        b[i].orientation[0] = b[i].orientation[4] = b[i].orientation[8] = 1;
    }
    b[0].position_world[0] = 2;
    if (angular_prepare_geometry(b, &in, 2, out)) {
        ++errors;
        return;
    }
    BarWarmState state{};
    for (int i = 0; i < 2; i++) {
        memcpy(state.velocity[i], objects[i] + 0x28, 24);
        memcpy(state.omega[i], objects[i] + 0xe0, 24);
    }
    for (int axis = 0; axis < 2; axis++) {
        auto *row = constraint + axis * 0xa8;
        put(row + 0xc0, out[axis].jacobian[1]);
        put(row + 0xd8, out[axis].jacobian[3]);
        put(row + 0x120, out[axis].weighted[1]);
        put(row + 0x138, out[axis].weighted[3]);
        put(row + 0x118, out[axis].inverse_effective_mass);
        memcpy(&state.impulse, row + 0x100, 8);
        BarWarmState next;
        if (bar_prepare_warm_start(out + axis, &state, -1, 1, &next)) {
            ++errors;
            return;
        }
        state = next;
        put(row + 0x100, next.impulse);
    }
    for (int i = 0; i < 2; i++) {
        put(objects[i] + 0x28, state.velocity[i]);
        put(objects[i] + 0xe0, state.omega[i]);
    }
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
    for (int axis = 0; axis < 2; axis++) {
        double impulse = .25 * (axis + 1), lower = -1, upper = 1;
        auto *row = constraint + axis * 0xa8;
        put(row + 0x100, impulse);
        put(row + 0x108, lower);
        put(row + 0x110, upper);
    }
    for (int axis = 0; axis < 2; axis++) {
        double v[3] = {0, 0, 0};
        v[axis] = 1;
        put(constraint + 0xa8 + axis * 0xa8, v);
    }
    double x = 2;
    put(objects[0] + 0x40, x);
    put(constraint + 0x98, roots);
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
    memcpy(prior, (void *)FixtureHinge, 32);
    if (!LoadLibraryA(argv[1]))
        return 92;
    for (int i = 0; i < 100 && s->status < 2; i++) {
        s->heartbeat_ms = GetTickCount64();
        Sleep(20);
    }
    if (s->status != 2)
        return 93;
    void(WINAPI *volatile run)(void *) = FixtureHinge;
    for (int i = 0; i < 508; i++) {
        s->heartbeat_ms = GetTickCount64();
        run(constraint);
    }
    if (calls != 508 || errors || s->observations != 4 || s->frame.prepare_status ||
        s->frame.jacobian_error || s->frame.weighted_error || s->frame.coefficient_error ||
        s->frame.warm_status || s->frame.warm_error)
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
    if (s->status != 3 || memcmp(prior, (void *)FixtureHinge, 32))
        return 96;
    printf("PASS Ball observer: original exactly once, coherent post-call geometry, ownership, "
           "lease gate, %s, original bytes restored. IPC=%zu\n",
           argc > 2 ? "watchdog" : "stop", sizeof(PoseIPC));
    return 0;
}
