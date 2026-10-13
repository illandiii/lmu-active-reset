#ifndef LMU_SNAPSHOT_OBSERVER_25661166_H
#define LMU_SNAPSHOT_OBSERVER_25661166_H
#include <windows.h>
#include <stdint.h>
extern "C" {
#include "constraint_warm_group.h"
}
#define POSE_MAGIC 0x4c4d55534e503032ULL
struct SnapshotFrame {
    uint64_t tick_ms, root_count, total_count, constraint_count, geometry_status, memory_valid;
    double simulation_time, observer_us, inertia_error, omega_error;
    RigidSavedBody bodies[32];
    ConstraintRecipe recipes[64];
    ConstraintMemory memory[64];
};
struct PoseIPC {
    uint64_t magic, version, pid;
    volatile LONG stop, status;
    volatile LONG64 heartbeat_ms;
    volatile LONG allow_observation, error;
    volatile LONG64 callbacks, observations, skipped, sequence, writer_thread;
    uint64_t qpc_frequency;
    SnapshotFrame frame;
};
static_assert(sizeof(SnapshotFrame) == 21072, "Snapshot frame ABI");
static_assert(sizeof(PoseIPC) == 21168, "Snapshot IPC ABI");
static void pose_mapping_name(wchar_t *out, DWORD pid) {
    wsprintfW(out, L"Local\\LMU_Snapshot25661166_v2_%lu", pid);
}
#endif
