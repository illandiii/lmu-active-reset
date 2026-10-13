#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "observer_ipc.h"
static unsigned char vehicle[0x5000];
__declspec(dllexport) void *WINAPI FixtureVehicle(void) {
    return vehicle;
}
__declspec(dllexport) __declspec(noinline) void WINAPI FixtureIntegrate(void *ptr, double dt) {
    unsigned char *p = (unsigned char *)ptr;
    for (int i = 0; i < 3; i++) {
        double pos, vel;
        memcpy(&pos, p + 0x24b0 + 8 * i, 8);
        memcpy(&vel, p + 0x2498 + 8 * i, 8);
        pos += vel * dt;
        memcpy(p + 0x24b0 + 8 * i, &pos, 8);
    }
}
int main(int argc, char **argv) {
    if (argc < 2)
        return 90;
    wchar_t name[96];
    mapping_name(name, GetCurrentProcessId());
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                                        sizeof(ObserverIPC), name);
    if (!mapping || GetLastError() == ERROR_ALREADY_EXISTS)
        return 91;
    ObserverIPC *s = (ObserverIPC *)MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*s));
    memset(s, 0, sizeof(*s));
    s->magic = OBS_MAGIC;
    s->version = 1;
    s->pid = GetCurrentProcessId();
    s->heartbeat_ms = GetTickCount64();
    s->allow_observation = 1;
    unsigned char prior[32];
    memcpy(prior, (void *)FixtureIntegrate, 32);
    double vel[3] = {3, -2, 1};
    memcpy(vehicle + 0x2498, vel, sizeof(vel));
    uintptr_t begin = 0x10000, end = 0x10008;
    memcpy(vehicle + 0x2440, &begin, 8);
    memcpy(vehicle + 0x2448, &end, 8);
    if (!LoadLibraryA(argv[1])) {
        printf("LoadLibrary error %lu\n", GetLastError());
        return 92;
    }
    for (int i = 0; i < 100 && s->status < 2; i++) {
        s->heartbeat_ms = GetTickCount64();
        Sleep(20);
    }
    if (s->status != 2) {
        printf("status=%ld error=%ld\n", s->status, s->error);
        return 93;
    }
    void(WINAPI *volatile fn)(void *, double) = FixtureIntegrate;
    for (int i = 0; i < 128; i++) {
        s->heartbeat_ms = GetTickCount64();
        fn(vehicle, .01);
    }
    double position;
    memcpy(&position, vehicle + 0x24b0, 8);
    if (fabs(position - 3.84) > 1e-10 || s->observations != 16 || s->callbacks != 128 ||
        fabs(s->dt - .01) > 1e-12) {
        printf("data mismatch %.12f obs=%lld calls=%lld dt=%.12f\n", position, s->observations,
               s->callbacks, s->dt);
        return 94;
    }
    s->allow_observation = 0;
    LONG64 observations = s->observations;
    for (int i = 0; i < 128; i++) {
        s->heartbeat_ms = GetTickCount64();
        fn(vehicle, .01);
    }
    if (s->observations != observations)
        return 95;
    if (argc > 2 && strcmp(argv[2], "watchdog") == 0)
        Sleep(2300);
    else
        s->stop = 1;
    for (int i = 0; i < 100 && s->status == 2; i++)
        Sleep(20);
    if (s->status != 3 || memcmp(prior, (void *)FixtureIntegrate, 32)) {
        printf("disable status=%ld error=%ld\n", s->status, s->error);
        return 96;
    }
    LONG64 calls = s->callbacks;
    fn(vehicle, .01);
    if (s->callbacks != calls)
        return 97;
    printf("PASS trampoline preserves double ABI and results; 16 pre/post observations; gate "
           "blocks; %s disables; original bytes restored.\n",
           argc > 2 ? "watchdog" : "stop");
    return 0;
}
