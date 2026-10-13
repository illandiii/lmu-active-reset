#ifndef LMU_BAR_OBSERVER_IPC_H
#define LMU_BAR_OBSERVER_IPC_H
#include <windows.h>
#include <stdint.h>
extern "C" {
#include "bar_constraint_core.h"
}
#define POSE_MAGIC 0x4c4d554241523232ULL
struct BarFrame {
    RigidSavedBody bodies[2];
    BarGeometryInput geometry;
    double native_jacobian[12], native_weighted[12], native_coefficient;
    uint64_t prepare_status;
    double jacobian_error, weighted_error, coefficient_error;
    uint64_t constraint_index, tick_ms;
};
struct PoseIPC {
    uint64_t magic, version, pid;
    volatile LONG stop, status;
    volatile LONG64 heartbeat_ms;
    volatile LONG allow_observation, error;
    volatile LONG64 callbacks, observations, skipped, sequence, writer_thread;
    uint64_t qpc_frequency;
    BarFrame frame;
};
static_assert(sizeof(BarFrame) == 656, "Bar frame ABI");
static_assert(sizeof(PoseIPC) == 752, "Bar IPC ABI");
static void pose_mapping_name(wchar_t *out, DWORD pid) {
    wsprintfW(out, L"Local\\LMU_BarObserver14200_v2_%lu", pid);
}
#endif
