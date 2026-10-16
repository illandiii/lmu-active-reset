#ifndef LMU_BINDING_OBSERVER_IPC_H
#define LMU_BINDING_OBSERVER_IPC_H
#include <windows.h>
#include <stdint.h>
extern "C" {
#include "../physics/wheel_restore_core.h"
}
#define BINDING_MAGIC 0x4c4d5542494e4431ULL
struct BindingSample {
    WheelSavedSpin spins[4];
    RigidSavedBody hubs[4], wheels[4];
    uint64_t validation;
};
struct BindingFrame {
    uint64_t tick_ms, wheel_calls[4], hub_mismatches;
    double simulation_time, original_us, observer_total_us;
    BindingSample before, after, last_transform;
};
struct BindingIPC {
    uint64_t magic, version, pid;
    volatile LONG stop, status;
    volatile LONG64 heartbeat_ms;
    volatile LONG allow_observation, error;
    volatile LONG64 callbacks, observations, skipped, sequence, writer_thread;
    uint64_t qpc_frequency;
    BindingFrame frame;
};
static_assert(sizeof(BindingSample) == 1512, "Binding sample ABI");
static_assert(sizeof(BindingFrame) == 4608, "Binding frame ABI");
static_assert(sizeof(BindingIPC) == 4704, "Binding IPC ABI");
static void binding_mapping_name(wchar_t *out, DWORD pid) {
    wsprintfW(out, L"Local\\LMU_BindingObserver14200_v1_%lu", pid);
}
#endif
