#ifndef LMU_OBSERVER_IPC_H
#define LMU_OBSERVER_IPC_H
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#define OBS_MAGIC 0x4c4d554f425331ULL
typedef struct ObserverIPC {
    uint64_t magic, version, pid;
    volatile LONG stop, status;
    volatile LONG64 heartbeat_ms;
    volatile LONG allow_observation, error;
    volatile LONG64 callbacks, observations, skipped, sequence, writer_thread;
    uint64_t tick_ms, begin_qpc, duration_qpc;
    double dt, pre_position[3], post_position[3], pre_velocity[3], post_velocity[3];
    uint64_t body_count, qpc_frequency;
} ObserverIPC;
_Static_assert(offsetof(ObserverIPC, heartbeat_ms) == 32, "IPC heartbeat offset");
_Static_assert(offsetof(ObserverIPC, callbacks) == 48, "IPC callbacks offset");
_Static_assert(offsetof(ObserverIPC, dt) == 112, "IPC data offset");
_Static_assert(sizeof(ObserverIPC) == 232, "IPC size");
static void mapping_name(wchar_t *out, DWORD pid) {
    wsprintfW(out, L"Local\\LMU_Observer_v1_%lu", pid);
}
#endif
