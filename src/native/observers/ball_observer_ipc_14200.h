#ifndef LMU_BALL_OBSERVER_IPC_H
#define LMU_BALL_OBSERVER_IPC_H
#include <windows.h>
#include <stdint.h>
extern "C" {
#include "../physics/ball_constraint_core.h"
}
#define POSE_MAGIC 0x4c4d5542414c3231ULL
struct BallRow {
    double native_jacobian[12], native_weighted[12], native_coefficient;
    double impulse_before, impulse_after, lower, upper;
};
struct BallFrame {
    RigidSavedBody bodies[2];
    BallGeometryInput geometry;
    BallRow rows[3];
    BarWarmState warm_before, warm_after;
    uint64_t prepare_status, warm_status;
    double jacobian_error, weighted_error, coefficient_error, warm_error;
    uint64_t constraint_index, tick_ms;
};
struct PoseIPC {
    uint64_t magic, version, pid;
    volatile LONG stop, status;
    volatile LONG64 heartbeat_ms;
    volatile LONG allow_observation, error;
    volatile LONG64 callbacks, observations, skipped, sequence, writer_thread;
    uint64_t qpc_frequency;
    BallFrame frame;
};
static_assert(sizeof(BallFrame) == 1392, "Ball frame ABI");
static_assert(sizeof(PoseIPC) == 1488, "Ball IPC ABI");
static void pose_mapping_name(wchar_t *out, DWORD pid) {
    wsprintfW(out, L"Local\\LMU_BallObserver14200_v1_%lu", pid);
}
#endif
