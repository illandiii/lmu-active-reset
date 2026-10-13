#ifndef LMU_SNAPSHOT_OBSERVER_25661166_H
#define LMU_SNAPSHOT_OBSERVER_25661166_H
#include <windows.h>
#include <stdint.h>
extern "C" {
#include "constraint_warm_group.h"
#include "mechanical_prepare_core.h"
}
#define POSE_MAGIC 0x4c4d55534e503036ULL
struct TirePersistentState {
    double total_mass, sector_mass[8], cached_sector_inertia[3];
    uint32_t update_phase, reserved;
    double thermal_fb0, thermal_fb8, accumulated_fc0;
};
static_assert(sizeof(TirePersistentState) == 128, "Tire ABI");
struct PowertrainPartialState {
    uint64_t engine_type_rva;
    double engine_1f8, engine_250, engine_260, engine_280;
    double hdv_48a0, hdv_48a8, hdv_48b0, hdv_48b8, hdv_48e8, hdv_48f0, hdv_48f8;
};
static_assert(sizeof(PowertrainPartialState) == 96, "Powertrain ABI");
struct TransmissionPartialState {
    double shaft_0, shaft_8, input_18, input_20, input_30, input_38;
    int32_t gear_64;
    uint32_t reserved;
};
struct SnapshotFrame {
    uint64_t tick_ms, root_count, total_count, constraint_count, geometry_status, memory_valid;
    double simulation_time, observer_us, inertia_error, omega_error;
    RigidSavedBody bodies[32];
    ConstraintRecipe recipes[64];
    ConstraintMemory memory[64];
    MechanicalDependencies dependencies;
    MechanicalLinks links;
    uint64_t mechanical_status;
    TirePersistentState tire_state[4];
    PowertrainPartialState powertrain;
    TransmissionPartialState transmission;
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
static_assert(sizeof(SnapshotFrame) == 22816, "Snapshot frame ABI");
static_assert(sizeof(PoseIPC) == 22912, "Snapshot IPC ABI");
static void pose_mapping_name(wchar_t *out, DWORD pid) {
    wsprintfW(out, L"Local\\LMU_Snapshot25661166_v6_%lu", pid);
}
#endif
