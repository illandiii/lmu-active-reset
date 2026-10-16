#ifndef LMU_SNAPSHOT_OBSERVER_25661166_H
#define LMU_SNAPSHOT_OBSERVER_25661166_H
#include "vehicle_snapshot_25661166.h"
#define POSE_MAGIC 0x4c4d55534e503055ULL
struct PoseIPC {
    uint64_t magic, version, pid;
    volatile LONG stop, status;
    volatile LONG64 heartbeat_ms;
    volatile LONG allow_observation, error;
    volatile LONG64 callbacks, observations, skipped, sequence, writer_thread;
    uint64_t qpc_frequency;
    volatile LONG64 context_generation;
    SnapshotFrame frame;
};
static_assert(sizeof(SnapshotFrame) == 306224, "Snapshot frame ABI");
static_assert(sizeof(PoseIPC) == 306328, "Snapshot IPC ABI");
static void pose_mapping_name(wchar_t *out, DWORD pid) {
    wsprintfW(out, L"Local\\LMU_Snapshot25661166_v31_%lu", pid);
}
#endif
